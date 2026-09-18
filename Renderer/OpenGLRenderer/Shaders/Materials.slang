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
    material.type = gpuMaterial.metadata.x;
    material.albedo = gpuMaterial.baseColor.rgb;
    material.opacity = gpuMaterial.baseColor.a;
    material.metallic = gpuMaterial.surface.x;
    material.roughness = gpuMaterial.surface.y;
    material.refractionIndex = gpuMaterial.surface.z;
    material.transmission = gpuMaterial.surface.w;
    material.emission = gpuMaterial.emission.xyz;
    material.emissionStrength = gpuMaterial.emission.w;
    material.baseColorTexture = gpuMaterial.textureIndices.x;
    material.metallicRoughnessTexture = gpuMaterial.textureIndices.y;

    // Current GPU packing has no dedicated fuzz field.
    material.fuzz = gpuMaterial.surface.y;
    return material;
}

public float3 emitted(Material material) {
    return material.emission * material.emissionStrength;
}

bool scatterLambertian(Ray rayIn, HitRecord rec, Material material, out float3 attenuation, out Ray scattered, inout uint rngState) {
    float3 scatterDirection = rec.normal + randomUnitVector(rngState);
    if (nearZero(scatterDirection)) {
        scatterDirection = rec.normal;
    }
    scattered.direction = scatterDirection;
    scattered.origin = offsetRayOrigin(rec, scattered.direction);
    attenuation = material.albedo;
    return true;
}

bool scatterMetal(Ray rayIn, HitRecord rec, Material material, out float3 attenuation, out Ray scattered, inout uint rngState) {
    float3 reflected = reflect(normalize(rayIn.direction), rec.normal);
    float fuzz = clamp(material.fuzz, 0.0f, 1.0f);
    scattered.origin = rec.p;
    scattered.direction = reflected + fuzz * randomUnitVector(rngState);
    attenuation = material.albedo;
    return dot(scattered.direction, rec.normal) > 0.0f;
}

float reflectance(float cosine, float refractionIndex) {
    float r0 = (1.0f - refractionIndex) / (1.0f + refractionIndex);
    r0 *= r0;
    return r0 + (1.0f - r0) * pow(1.0f - cosine, 5.0f);
}

bool scatterDielectric(Ray rayIn, HitRecord rec, Material material, out float3 attenuation, out Ray scattered, inout uint rngState) {
    attenuation = float3(1.0f);
    float ri = rec.frontFace ? 1.0f / material.refractionIndex : material.refractionIndex;
    float3 unitDirection = normalize(rayIn.direction);
    float cosTheta = min(dot(-unitDirection, rec.normal), 1.0f);
    float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    bool cannotRefract = ri * sinTheta > 1.0f;
    float3 direction;
    if (cannotRefract || reflectance(cosTheta, ri) > randomFloat(rngState)) {
        direction = reflect(unitDirection, rec.normal);
    } else {
        direction = refract(unitDirection, rec.normal, ri);
    }
    scattered.origin = rec.p;
    scattered.direction = direction;
    return true;
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

bool scatterPbrMetallicRoughness(Ray rayIn, HitRecord rec, Material material, out float3 attenuation, out Ray scattered, inout uint rngState) {
    float3 baseColor = material.albedo;
    float opacity = material.opacity;
    float metallic = material.metallic;
    float roughness = material.roughness;
    if (material.baseColorTexture >= 0) {
        float4 texel = uBaseColorTextures.SampleLevel(float3(rec.uv, float(material.baseColorTexture)), 0.0f);
        baseColor *= texel.rgb;
        opacity *= texel.a;
    }
    if (material.metallicRoughnessTexture >= 0) {
        float4 mr = uMetalRoughTextures.SampleLevel(float3(rec.uv, float(material.metallicRoughnessTexture)), 0.0f);
        roughness *= mr.g;
        metallic *= mr.b;
    }
    metallic = clamp(metallic, 0.0f, 1.0f);
    roughness = clamp(roughness, 0.01f, 1.0f);
    float transmission = clamp(material.transmission, 0.0f, 1.0f);
    float transmissionProbability = transmission * (1.0f - metallic);
    if (transmissionProbability > 0.0f && randomFloat(rngState) < transmissionProbability) {
        attenuation = float3(1.0f);
        float ri = rec.frontFace ? 1.0f / material.refractionIndex : material.refractionIndex;
        float3 unitDirection = normalize(rayIn.direction);
        float cosTheta = min(dot(-unitDirection, rec.normal), 1.0f);
        float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
        bool cannotRefract = ri * sinTheta > 1.0f;
        float3 direction;
        if (cannotRefract || reflectance(cosTheta, ri) > randomFloat(rngState)) {
            direction = reflect(unitDirection, rec.normal);
            attenuation = float3(1.0f);
        } else {
            direction = refract(unitDirection, rec.normal, ri);
            attenuation = baseColor;
        }
        scattered.origin = offsetRayOrigin(rec, direction);
        scattered.direction = direction;
        return true;
    }
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

public bool scatterMaterial(Ray rayIn, HitRecord rec, out float3 attenuation, out Ray scattered, Material material, inout uint rngState) {
    if (material.type == MATERIAL_LAMBERTIAN) {
        return scatterLambertian(rayIn, rec, material, attenuation, scattered, rngState);
    }
    if (material.type == MATERIAL_METAL) {
        return scatterMetal(rayIn, rec, material, attenuation, scattered, rngState);
    }
    if (material.type == MATERIAL_DIELECTRIC) {
        return scatterDielectric(rayIn, rec, material, attenuation, scattered, rngState);
    }
    if (material.type == MATERIAL_PBR_METAL_ROUGH) {
        return scatterPbrMetallicRoughness(rayIn, rec, material, attenuation, scattered, rngState);
    }
    return false;
}
