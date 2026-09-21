#language slang 2026
module Materials;

import Constants;
import Types;
import GpuTypes;
import Random;
import Resources;

struct SurfaceData {
    float3 baseColor;
    float metallic;
    float roughness;
    float alpha;

    float transmission;
    float ior;

    float3 F0;
};

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

float3 toLocal(float3 v, float3 T, float3 B, float3 N) {
    return float3(dot(v, T), dot(v, B), dot(v, N));
}

float3 toWorld(float3 v, float3 T, float3 B, float3 N) {
    return T * v.x + B * v.y + N * v.z;
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

float3 averageSchlickFresnel(float3 F0) {
    //F90 = 1
    return (float3(1.0f) + 20.0f * F0) / 21.0f;
}

float dielectricF0(float ior) {
    ior = max(ior, 1.0001f);
    float r = (ior - 1.0f) / (ior + 1.0f);
    return r * r;
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

//from enterprise pbr, TODO switch to LuT
float ggxDirectionalAlbedo(float NoX, float alpha) {
    NoX = clamp(NoX, 0.0f, 1.0f);
    float alphaUv = alpha * alpha;
    float roughnessTerm = -0.20277f + alphaUv * (2.772f + alphaUv * (-2.6175f + 0.73343f * alphaUv));
    float viewTerm = 3.09507f + NoX * (-9.11369f + NoX * (15.8884f + NoX * (-13.70343f + 4.51786f * NoX)));
    float E = 1.0f - 1.4594f * alphaUv * NoX * roughnessTerm * viewTerm;
    return clamp(E, 0.0f, 1.0f);
}

//from enterprise pbr, TODO switch to LuT
float ggxAverageAlbedo(float alpha) {
    float alphaUv = alpha * alpha;
    float Eavg = 1.0f + alphaUv * (-0.113f + alphaUv * (-1.8695f + alphaUv * (2.2268f - 0.83397f * alphaUv)));
    return clamp(Eavg, 0.0f, 1.0f);
}

float luminance(float3 color) {
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 offsetRayOrigin(HitRecord rec, float3 direction) {
    static const float RAY_EPSILON = 0.001f;
    float side = dot(direction, rec.normal) >= 0.0f ? 1.0f : -1.0f;
    return rec.p + rec.normal * side * RAY_EPSILON;
}

SurfaceData evaluateSurface(Material material, HitRecord rec) {
    SurfaceData surface;
    surface.baseColor = material.baseColor;
    surface.metallic = material.metallic;
    surface.roughness = material.roughness;
    surface.transmission = material.transmission;
    surface.ior = max(material.refractionIndex, 1.0001f);

    if (material.baseColorTexture >= 0) {
        float4 texel = uBaseColorTextures.SampleLevel(float3(rec.uv, float(material.baseColorTexture)), 0.0f);
        surface.baseColor *= texel.rgb;
    }
    if (material.metallicRoughnessTexture >= 0) {
        float4 mr = uMetalRoughTextures.SampleLevel(float3(rec.uv, float(material.metallicRoughnessTexture)), 0.0f);
        surface.roughness *= mr.g;
        surface.metallic *= mr.b;
    }

    surface.metallic = clamp(surface.metallic, 0.0f, 1.0f);
    surface.roughness = clamp(surface.roughness, 0.01f, 1.0f);
    surface.transmission = clamp(surface.transmission, 0.0f, 1.0f);
    surface.alpha = max(surface.roughness * surface.roughness, 0.001f);
    float dielectricReflectance = dielectricF0(surface.ior);
    surface.F0 = lerp(float3(dielectricReflectance), surface.baseColor, surface.metallic);
    return surface;
}

void buildBasis(float3 N, out float3 T, out float3 B) {
    float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    T = normalize(cross(up, N));
    B = cross(N, T);
}

//ggx importance sampling
float smithGgxG1(float NoV, float alpha) {
    float alpha2 = alpha * alpha;
    float NoV2 = NoV * NoV;
    return (2.0f * NoV) / max(NoV + sqrt(alpha2 + (1.0f - alpha2) * NoV2), EPSILON);
}

float3 sampleCosineHemisphere(float3 N, inout uint rngState) {
    float u1 = randomFloat(rngState);
    float u2 = randomFloat(rngState);
    float r = sqrt(u1);
    float phi = TWO_PI * u2;
    float3 localDirection = float3(r * cos(phi), r * sin(phi), sqrt(max(0.0f, 1.0f - u1)));
    float3 T;
    float3 B;
    buildBasis(N, T, B);
    return normalize(toWorld(localDirection, T, B, N));
}

bool sampleDiffuse(float3 N, float3 diffuseColor, float lobeProbability, out float3 direction, out float3 weight, inout uint rngState) {
    direction = sampleCosineHemisphere(N, rngState);
    float NoL = max(dot(N, direction), 0.0f);
    if (NoL <= EPSILON || lobeProbability <= EPSILON) {
        return false;
    }
    weight = diffuseColor / lobeProbability;
    return true;
}

float3 evaluateGgxSingleScatter(float3 N, float3 V, float3 L, SurfaceData surface) {
    float NoV = max(dot(N, V), 0.0f);
    float NoL = max(dot(N, L), 0.0f);
    if (NoV <= EPSILON || NoL <= EPSILON) {
        return float3(0.0f);
    }

    float3 H = normalize(V + L);
    float NoH = max(dot(N, H), 0.0f);
    float VoH = max(dot(V, H), 0.0f);
    if (NoH <= EPSILON || VoH <= EPSILON) {
        return float3(0.0f);
    }

    float D = ggxDistribution(NoH, surface.alpha);
    float visibility = smithGgxCorrelatedVisibility(NoV, NoL, surface.alpha);
    float3 F = fresnelSchlick(VoH, surface.F0);
    return D * visibility * F;
}

float3 evaluateGgxMultiScatter(float3 N, float3 V, float3 L, SurfaceData surface) {
    float NoV = max(dot(N, V), 0.0f);
    float NoL = max(dot(N, L), 0.0f);
    if (NoV <= EPSILON || NoL <= EPSILON) {
        return float3(0.0f);
    }

    float Ev = ggxDirectionalAlbedo(NoV, surface.alpha);
    float El = ggxDirectionalAlbedo(NoL, surface.alpha);
    float Eavg = ggxAverageAlbedo(surface.alpha);

    float missingAverageEnergy = 1.0f - Eavg;
    if (missingAverageEnergy <= 1.0e-5f) {
        return float3(0.0f);
    }

    float multiScatter = ((1.0f - Ev) * (1.0f - El)) / (PI * missingAverageEnergy);
    float3 Favg =averageSchlickFresnel(surface.F0);
    float3 numerator = Favg * Favg * Eavg;
    float3 denominator = float3(1.0f) - Favg * (1.0f - Eavg);
    float3 Fms = numerator / max(denominator, float3(EPSILON));
    return multiScatter * Fms;
}

float3 evaluateGgxReflection(float3 N, float3 V, float3 L, SurfaceData surface) {
    float3 singleScatter = evaluateGgxSingleScatter(N, V, L, surface);
    float3 multiScatter = evaluateGgxMultiScatter(N, V, L, surface);
    return singleScatter + multiScatter;
}

float3 sampleGgxVndf(float3 N, float3 V, float alpha, inout uint rngState) {
    float3 T;
    float3 B;
    buildBasis(N, T, B);
    float3 localV = toLocal(V, T, B, N);

    float3 Vh = normalize(float3(alpha * localV.x, alpha * localV.y, localV.z));
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    float3 T1 = lensq > EPSILON ? float3(-Vh.y, Vh.x, 0.0f) / sqrt(lensq) : float3(1.0f, 0.0f, 0.0f);

    float3 T2 = cross(Vh, T1);
    float u1 = randomFloat(rngState);
    float u2 = randomFloat(rngState);
    float r = sqrt(u1);
    float phi = TWO_PI * u2;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5f * (1.0f + Vh.z);

    t2 = (1.0f - s) * sqrt(max(0.0f, 1.0f - t1 * t1)) + s * t2;
    float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0f, 1.0f - t1 * t1 - t2 * t2)) * Vh;

    float3 localH = normalize(float3(alpha * Nh.x, alpha * Nh.y, max(0.0f, Nh.z)));
    return normalize(toWorld(localH, T, B, N));
}

float ggxVndfReflectionPdf(float3 N, float3 V, float3 H, float alpha) {
    float NoV = max(dot(N, V), 0.0f);
    float NoH = max(dot(N, H), 0.0f);
    float VoH = max(dot(V, H), 0.0f);

    if (NoV <= EPSILON || NoH <= EPSILON || VoH <= EPSILON) {
        return 0.0f;
    }
    float D = ggxDistribution(NoH, alpha);
    float G1 = smithGgxG1(NoV, alpha);
    float pdfH = D * G1 * VoH / NoV;
    return pdfH / max(4.0f * VoH, EPSILON);
}

bool sampleGgxReflection(float3 N, float3 V, SurfaceData surface, float lobeProbability, out float3 direction, out float3 weight, inout uint rngState) {
    float3 H = sampleGgxVndf(N, V, surface.alpha, rngState);
    float VoH = max(dot(V, H), 0.0f);
    if (VoH <= EPSILON) {
        return false;
    }

    float3 L = reflect(-V, H);
    float NoL = max(dot(N, L), 0.0f);
    if (NoL <= EPSILON) {
        return false;
    }

    float pdf = ggxVndfReflectionPdf(N, V, H, surface.alpha);
    pdf *= lobeProbability;
    if (pdf <= EPSILON) {
        return false;
    }

    float3 brdf = evaluateGgxReflection(N, V, L, surface);
    direction = L;
    weight = brdf * NoL / pdf;
    return true;
}

bool sampleTransmission(Ray rayIn, HitRecord rec, SurfaceData surface, out float3 direction, out float3 weight, inout uint rngState) {
    float eta = rec.frontFace ? 1.0f / surface.ior : surface.ior;
    float3 unitDirection = normalize(rayIn.direction);
    float cosTheta = min(dot(-unitDirection, rec.normal),1.0f);
    float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    bool cannotRefract = eta * sinTheta > 1.0f;
    if (cannotRefract || reflectance(cosTheta, eta) > randomFloat(rngState)) {
        direction = reflect(unitDirection, rec.normal);
        weight = float3(1.0f);
    } else {
        direction = refract(unitDirection, rec.normal, eta);
        weight = surface.baseColor;
    }
    return true;
}

// main bsdf logic
public bool scatterBsdf(Ray rayIn, HitRecord rec, Material material, out float3 attenuation, out Ray scattered, inout uint rngState) {
    SurfaceData surface = evaluateSurface(material, rec);
    float3 N = rec.normal;
    float3 V = normalize(-rayIn.direction);
    float NoV = max(dot(N, V), 0.0f);
    if (NoV <= EPSILON) {
        return false;
    }

    // ---------------------------------------------------------------------
    // Transmission
    // ---------------------------------------------------------------------
    float transmissionProbability = surface.transmission * (1.0f - surface.metallic);
    if (transmissionProbability > 0.0f && randomFloat(rngState) < transmissionProbability) {
        float3 direction;
        float3 weight;
        if (!sampleTransmission(rayIn, rec, surface, direction, weight, rngState)) {
            return false;
        }
        scattered.direction = direction;
        scattered.origin = offsetRayOrigin(rec, direction);
        attenuation = weight;
        return true;
    }

    // ---------------------------------------------------------------------
    // Opaque reflection
    // ---------------------------------------------------------------------
    float3 Fv = fresnelSchlick(NoV, surface.F0);
    float Ev = ggxDirectionalAlbedo(NoV, surface.alpha);
    float Eavg = ggxAverageAlbedo(surface.alpha);
    float3 Favg = averageSchlickFresnel(surface.F0);
    float3 Fms = (Favg * Favg * Eavg) / max(float3(1.0f) - Favg * (1.0f - Eavg), float3(EPSILON));
    float3 specularEstimate = Fv * Ev + Fms * (1.0f - Ev);
    float3 diffuseColor = (float3(1.0f) - Fv) * (1.0f - surface.metallic) * surface.baseColor;
    float diffuseEnergy = luminance(diffuseColor);
    float specularEnergy = luminance(specularEstimate);
    float totalEnergy = diffuseEnergy + specularEnergy;
    if (totalEnergy <= EPSILON) {
        return false;
    }
    float specularProbability = clamp(specularEnergy / totalEnergy, 0.05f, 0.95f);
    float diffuseProbability = 1.0f - specularProbability;

    float3 direction;
    float3 weight;
    if (randomFloat(rngState) < specularProbability) {
        if (!sampleGgxReflection(N, V, surface, specularProbability, direction, weight, rngState)) {
            return false;
        }
    } else {
        if (!sampleDiffuse(N, diffuseColor, diffuseProbability, direction, weight, rngState)) {
            return false;
        }
    }

    scattered.direction = direction;
    scattered.origin = offsetRayOrigin(rec, direction);
    attenuation = weight;
    return true;
}
