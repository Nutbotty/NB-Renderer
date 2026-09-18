#language slang 2026
module Materials;

import Constants;
import Types;
import GpuTypes;
import Constants;
import Core;
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
    float3 V = normalize(-rayIn.direction);
    float NoV = max(dot(rec.normal, V), 0.0f);
    float3 F0 = lerp(float3(0.04f), baseColor, metallic);
    float3 F = fresnelSchlick(NoV, F0);
    float3 diffuseWeight = (float3(1.0f) - F) * (1.0f - metallic) * baseColor;
    float3 specularWeight = F;
    float diffuseEnergy = luminance(diffuseWeight);
    float specularEnergy = luminance(specularWeight);
    float totalEnergy = diffuseEnergy + specularEnergy;
    if (totalEnergy <= EPSILON) {
        return false;
    }
    float specularProbability = clamp(specularEnergy / totalEnergy, 0.05f, 0.95f);

    // -------------------------------------------------------------------------
    // Specular lobe
    // -------------------------------------------------------------------------
    if (randomFloat(rngState) < specularProbability) {
        float3 reflected = reflect(normalize(rayIn.direction), rec.normal);
        float spread = roughness * roughness;
        float3 direction = normalize(reflected + spread * randomUnitVector(rngState));
        if (dot(direction, rec.normal) <= 0.0f) {
            return false;
        }
        scattered.direction = direction;
        scattered.origin = offsetRayOrigin(rec, direction);
        attenuation = specularWeight / specularProbability;
        return true;
    }
    float3 scatterDirection = rec.normal + randomUnitVector(rngState);
    if (nearZero(scatterDirection)) {
        scatterDirection = rec.normal;
    }
    scattered.direction = scatterDirection;
    scattered.origin = offsetRayOrigin(rec, scatterDirection);
    attenuation = diffuseWeight / (1.0f - specularProbability);
    return true;
}
