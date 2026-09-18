#language slang 2026
module Materials;

import Constants;
import Types;
import GpuTypes;
import Random;
import Resources;

public Material loadMaterial(int index) {
    PackedMaterial gpuMaterial = materialBuffer[index];

    Material material;
    material.baseColor = gpuMaterial.baseColor.rgb;
    material.opacity = gpuMaterial.baseColor.a;
    material.metallic = gpuMaterial.surface.x;
    material.roughness = gpuMaterial.surface.y;
    material.refractionIndex = gpuMaterial.surface.z;
    material.transmission = gpuMaterial.surface.w;
    material.emission = gpuMaterial.emission.xyz;
    material.emissionStrength = gpuMaterial.emission.w;
    material.baseColorTexture = gpuMaterial.textureIndices.x;
    material.metallicRoughnessTexture = gpuMaterial.textureIndices.y;

    return material;
}

public float3 emitted(Material material) {
    return material.emission * material.emissionStrength;
}

float reflectance(float cosine, float refractionIndex) {
    float r0 = (1.0f - refractionIndex) / (1.0f + refractionIndex);
    r0 *= r0;
    return r0 + (1.0f - r0) * pow(1.0f - cosine, 5.0f);
}

float3 fresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (float3(1.0f) - F0) * pow(1.0f - clamp(cosTheta, 0.0f, 1.0f), 5.0f);
}

float ggxDistribution(float NoH, float alpha) {
    float alpha2 = alpha * alpha;
    float f = NoH * NoH * (alpha2 - 1.0f) + 1.0f;
    return alpha2 / max(PI * f * f, EPSILON);
}

float smithGgxCorrelatedVisibility(float NoV, float NoL, float alpha) {
    float alpha2 = alpha * alpha;
    float lambdaV = NoL * sqrt(NoV * NoV * (1.0f - alpha2) + alpha2);
    float lambdaL = NoV * sqrt(NoL * NoL * (1.0f - alpha2) + alpha2);
    return 0.5f / max(lambdaV + lambdaL, EPSILON);
}

float luminance(float3 color) {
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 offsetRayOrigin(HitRecord rec, float3 direction) {
    static const float RAY_EPSILON = 0.001f;
    float side = dot(direction, rec.normal) >= 0.0f ? 1.0f : -1.0f;
    return rec.p + rec.normal * side * RAY_EPSILON;
}

void evaluateMaterial(Material material, HitRecord rec, out float3 baseColor, out float metallic, out float roughness) {
    baseColor = material.baseColor;
    metallic = material.metallic;
    roughness = material.roughness;

    if (material.baseColorTexture >= 0) {
        float4 texel = uBaseColorTextures.SampleLevel(float3(rec.uv, float(material.baseColorTexture)), 0.0f);
        baseColor *= texel.rgb;
    }

    if (material.metallicRoughnessTexture >= 0) {
        float4 mr = uMetalRoughTextures.SampleLevel(float3(rec.uv, float(material.metallicRoughnessTexture)), 0.0f);
        roughness *= mr.g;
        metallic *= mr.b;
    }

    metallic = clamp(metallic, 0.0f, 1.0f);
    roughness = clamp(roughness, 0.01f, 1.0f);
}

void buildBasis(float3 N, out float3 T, out float3 B) {
    float3 up = abs(N.z) < 0.999f
        ? float3(0.0f, 0.0f, 1.0f)
        : float3(1.0f, 0.0f, 0.0f);

    T = normalize(cross(up, N));
    B = cross(N, T);
}

//ggx importance sampling
float3 sampleGgxHalfVector(float3 N, float alpha, inout uint rngState) {
    float u1 = randomFloat(rngState);
    float u2 = randomFloat(rngState);
    float alpha2 = alpha * alpha;
    float phi = TWO_PI * u1;
    float cosTheta = sqrt((1.0f - u2) / max(1.0f + (alpha2 - 1.0f) * u2, EPSILON));
    float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    float3 localH = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    float3 T;
    float3 B;
    buildBasis(N, T, B);
    return normalize(T * localH.x + B * localH.y + N * localH.z);
}

public bool scatterBsdf(Ray rayIn, HitRecord rec, Material material, out float3 attenuation, out Ray scattered, inout uint rngState) {
    float3 baseColor;
    float metallic;
    float roughness;
    evaluateMaterial(material, rec, baseColor, metallic, roughness);

    // -------------------------------------------------------------------------
    // Transmission / dielectric refraction
    // -------------------------------------------------------------------------
    float transmission = clamp(material.transmission, 0.0f, 1.0f);
    float transmissionProbability = transmission * (1.0f - metallic);
    if (transmissionProbability > 0.0f && randomFloat(rngState) < transmissionProbability) {
        float refractionIndex = max(material.refractionIndex, 1.0f);
        float eta = rec.frontFace ? 1.0f / refractionIndex : refractionIndex;

        float3 unitDirection = normalize(rayIn.direction);
        float cosTheta = min(dot(-unitDirection, rec.normal), 1.0f);
        float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
        bool cannotRefract = eta * sinTheta > 1.0f;

        float3 direction;
        if (cannotRefract || reflectance(cosTheta, eta) > randomFloat(rngState)) {
            direction = reflect(unitDirection, rec.normal);
            attenuation = float3(1.0f);
        } else {
            direction = refract(unitDirection, rec.normal, eta);
            attenuation = baseColor;
        }

        scattered.direction = direction;
        scattered.origin = offsetRayOrigin(rec, direction);
        return true;
    }

    // -------------------------------------------------------------------------
    // Opaque metallic-roughness BSDF
    // -------------------------------------------------------------------------
    float3 N = rec.normal;
    float3 V = normalize(-rayIn.direction);
    float NoV = max(dot(N, V), 0.0f);
    if (NoV <= EPSILON) {
        return false;
    }

    float alpha = max(roughness * roughness, 0.001f);
    float3 F0 = lerp(float3(0.04f), baseColor, metallic);
    float3 Fv = fresnelSchlick(NoV, F0);

    float3 diffuseColor = (float3(1.0f) - Fv) * (1.0f - metallic) * baseColor;
    float diffuseEnergy = luminance(diffuseColor);
    float specularEnergy = luminance(Fv);
    float totalEnergy = diffuseEnergy + specularEnergy;

    if (totalEnergy <= EPSILON) {
        return false;
    }
    float specularProbability = clamp(specularEnergy / totalEnergy, 0.05f, 0.95f);
    float diffuseProbability = 1.0f - specularProbability;

    // -------------------------------------------------------------------------
    // Specular lobe
    // -------------------------------------------------------------------------
    if (randomFloat(rngState) < specularProbability) {
        float3 H = sampleGgxHalfVector(N, alpha, rngState);
        float VoH = max(dot(V, H), 0.0f);
        float NoH = max(dot(N, H), 0.0f);

        if (VoH <= EPSILON || NoH <= EPSILON) {
            return false;
        }

        float3 L = reflect(-V, H);
        float NoL = max(dot(N, L), 0.0f);
        if (NoL <= EPSILON) {
            return false;
        }

        float D = ggxDistribution(NoH, alpha);
        float visibility = smithGgxCorrelatedVisibility(NoV, NoL, alpha);
        float3 F = fresnelSchlick(VoH, F0);
        float3 brdf = D * visibility * F;

        float pdfHalfVector = D * NoH;
        float pdfSpecular = pdfHalfVector / max(4.0f * VoH, EPSILON);

        float combinedPdf = specularProbability * pdfSpecular;
        if (combinedPdf <= EPSILON) {
            return false;
        }

        scattered.direction = L;
        scattered.origin = offsetRayOrigin(rec, L);
        attenuation = brdf * NoL / combinedPdf;
        return true;
    }

    // -------------------------------------------------------------------------
    // Diffuse lobe
    // -------------------------------------------------------------------------
    float3 direction = N + randomUnitVector(rngState);
    if (nearZero(direction)) {
        direction = N;
    }
    scattered.direction = direction;
    scattered.origin = offsetRayOrigin(rec, direction);
    attenuation = diffuseColor / diffuseProbability;

    return true;
}
