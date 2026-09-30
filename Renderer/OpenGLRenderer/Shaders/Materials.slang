#language slang 2026
module Materials;

import Constants;
import Types;
import GpuTypes;
import Random;
import Resources;

public struct SurfaceData {
    float3 baseColor;
    float metallic;
    float roughness;
    float alpha;
    float transmission;
    float ior;
    float eta;
};

public enum BSDFComponent {
    Metallic,
    Diffuse,
    SpecularDielectric,
    GlassReflect,
    GlassRefract
};

public struct BxDFSample {
    float3 L;
    float3 BxDF;
    float PDF;
    BSDFComponent component;
};

public struct BxDFEval {
    float3 BxDF;
    float PDF;
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
    material.emissiveTexture = gpuMaterial.textureIndices.w;
    return material;
}

public float3 emitted(Material material, HitRecord rec) {
     float3 emission = material.emission;
        if (material.emissiveTexture >= 0) {
            float3 texel = uEmissiveTextures.SampleLevel(float3(rec.uv, float(material.emissiveTexture)), 0.0f).rgb;
                emission *= texel;
        }
        return emission * material.emissionStrength;
}

float3 toLocal(float3 v, float3 T, float3 B, float3 N) {
    return float3(dot(v, T), dot(v, B), dot(v, N));
}

float3 toWorld(float3 v, float3 T, float3 B, float3 N) {
    return T * v.x + B * v.y + N * v.z;
}

void buildBasis(float3 N, out float3 T, out float3 B) {
    float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    T = normalize(cross(up, N));
    B = cross(N, T);
}

float3 offsetRayOrigin(HitRecord rec, float3 direction) {
    float side = dot(direction, rec.geometricNormal) >= 0.0f ? 1.0f : -1.0f;
    return rec.p + rec.geometricNormal * side * RAY_EPSILON;
}

public float3 makeSafeShadingNormal(float3 shadingNormal, float3 geometricNormal, float3 V) {
    float3 Ns = normalize(shadingNormal);
    float3 Ng = normalize(geometricNormal);
    if (dot(Ns, Ng) < 0.0f) {
        Ns = -Ns;
    }
    float NoVg = dot(Ng, V);
    if (NoVg <= EPSILON) {
        return Ng;
    }

    float NoVs = dot(Ns, V);
    float3 mirrorDirection = reflect(-V, Ns);
    if (NoVs > EPSILON && dot(Ng, mirrorDirection) > EPSILON) {
        return Ns;
    }

    float low = 0.0f;
    float high = 1.0f;

    for (int i = 0; i < 6; ++i) {
        float t = 0.5f * (low + high);
        float3 candidate = normalize(lerp(Ns, Ng, t));
        bool viewValid = dot(candidate, V) > EPSILON;
        float3 candidateReflection = reflect(-V, candidate);
        bool reflectionValid = dot(Ng, candidateReflection) > EPSILON;

        if (viewValid && reflectionValid) {
            high = t;
        } else {
            low = t;
        }
    }

    return normalize(lerp(Ns, Ng, high));
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
    surface.eta = rec.frontFace ? 1.0f / surface.ior : surface.ior;
    return surface;
}

float3 fresnelSchlick(float cosTheta, float3 F0) {
    float m = clamp(1.0f - cosTheta, 0.0f, 1.0f);
    float m2 = m * m;
    float m5 = m2 * m2 * m;
    return F0 + (float3(1.0f) - F0) * m5;
}

float dielectricFresnel(float VdotH, float eta) {
    float cosThetaI = clamp(abs(VdotH), 0.0f, 1.0f);
    float sinThetaTSq = eta * eta * max(0.0f, 1.0f - cosThetaI * cosThetaI);

    if (sinThetaTSq > 1.0f) {
        return 1.0f;
    }

    float cosThetaT = sqrt(max(0.0f, 1.0f - sinThetaTSq));
    float rsDenominator = eta * cosThetaT + cosThetaI;
    float rpDenominator = eta * cosThetaI + cosThetaT;

    if (abs(rsDenominator) <= EPSILON || abs(rpDenominator) <= EPSILON) {
        return 1.0f;
    }

    float rs = (eta * cosThetaT - cosThetaI) / rsDenominator;
    float rp = (eta * cosThetaI - cosThetaT) / rpDenominator;
    return 0.5f * (rs * rs + rp * rp);
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

float smithGgxG1(float NoV, float alpha) {
    if (NoV <= EPSILON) {
        return 0.0f;
    }

    float alpha2 = alpha * alpha;
    float NoV2 = NoV * NoV;
    return (2.0f * NoV) / max(NoV + sqrt(alpha2 + (1.0f - alpha2) * NoV2), EPSILON);
}

// Enterprise directional albedo, eventually replace with LUT
float ggxDirectionalAlbedo(float NoX, float alpha) {
    NoX = clamp(NoX, 0.0f, 1.0f);
    float alphaUv = alpha * alpha;
    float a = -0.20277f + alphaUv * (2.772f + alphaUv * (-2.6175f + 0.73343f * alphaUv));
    float b = 3.09507f + NoX * (-9.11369f + NoX * (15.8884f + NoX * (-13.70343f + 4.51786f * NoX)));
    float E = 1.0f - 1.4594f * alphaUv * NoX * a * b;
    return clamp(E, 0.0f, 1.0f);
}

// Enterprise average albedo, eventually replace with LUT
float ggxAverageAlbedo(float alpha) {
    float alphaUv = alpha * alpha;
    float Eavg = 1.0f + alphaUv * (-0.113f + alphaUv * (-1.8695f + alphaUv * (2.2268f - 0.83397f * alphaUv)));
    return clamp(Eavg, 0.0f, 1.0f);
}

float3 averageSchlickFresnel(float3 F0) {
    return (float3(1.0f) + 20.0f * F0) / 21.0f;
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

void getComponentProbabilities(SurfaceData surface, out float metallicProbability, out float dielectricProbability, out float glassProbability) {
    metallicProbability = surface.metallic;
    dielectricProbability = (1.0f - surface.metallic) * (1.0f - surface.transmission);
    glassProbability = (1.0f - surface.metallic) * surface.transmission;

    float probabilitySum = metallicProbability + dielectricProbability + glassProbability;

    if (probabilitySum <= EPSILON) {
        metallicProbability = 0.0f;
        dielectricProbability = 1.0f;
        glassProbability = 0.0f;
        return;
    }

    metallicProbability /= probabilitySum;
    dielectricProbability /= probabilitySum;
    glassProbability /= probabilitySum;
}

BxDFEval evaluateDiffuse(float3 N, float3 L, float3 baseColor) {
    BxDFEval result;
    result.BxDF = float3(0.0f);
    result.PDF = 0.0f;

    float NoL = dot(N, L);
    if (NoL <= EPSILON) {
        return result;
    }

    float inversePi = 1.0f / PI;
    result.BxDF = baseColor * inversePi * NoL;
    result.PDF = NoL * inversePi;
    return result;
}

BxDFEval evaluateGgxReflection(float3 N, float3 V, float3 L, float alpha, float3 F) {
    BxDFEval result;
    result.BxDF = float3(0.0f);
    result.PDF = 0.0f;

    float NoV = dot(N, V);
    float NoL = dot(N, L);

    if (NoV <= EPSILON || NoL <= EPSILON) {
        return result;
    }

    float3 VplusL = V + L;
    float halfLengthSquared = dot(VplusL, VplusL);

    if (halfLengthSquared <= EPSILON) {
        return result;
    }

    float3 H = VplusL / sqrt(halfLengthSquared);
    float NoH = max(dot(N, H), 0.0f);
    float VoH = dot(V, H);

    if (NoH <= EPSILON || VoH <= EPSILON) {
        return result;
    }

    float D = ggxDistribution(NoH, alpha);
    float GV = smithGgxG1(NoV, alpha);
    float GL = smithGgxG1(NoL, alpha);

    result.BxDF = D * F * GV * GL / max(4.0f * NoV, EPSILON);

    float pdfH = D * GV * VoH / max(NoV, EPSILON);
    result.PDF = pdfH / max(4.0f * VoH, EPSILON);
    return result;
}

BxDFEval evaluateGgxRefraction(float3 N, float3 V, float3 L, SurfaceData surface) {
    BxDFEval result;
    result.BxDF = float3(0.0f);
    result.PDF = 0.0f;

    float NoV = dot(N, V);
    float NoL = dot(N, L);
    if (NoV <= EPSILON || NoL >= -EPSILON) {
        return result;
    }

    float3 unnormalizedH = surface.eta * V + L;
    float hLengthSquared = dot(unnormalizedH, unnormalizedH);
    if (hLengthSquared <= EPSILON) {
        return result;
    }

    float3 H = unnormalizedH / sqrt(hLengthSquared);
    if (dot(N, H) < 0.0f) {
        H = -H;
    }

    float NoH = max(dot(N, H), 0.0f);
    float VdotH = dot(V, H);
    float LdotH = dot(L, H);

    bool validRefraction =
        (VdotH > 0.0f && LdotH < 0.0f) ||
        (VdotH < 0.0f && LdotH > 0.0f);

    if (!validRefraction || NoH <= EPSILON) {
        return result;
    }

    float D = ggxDistribution(NoH, surface.alpha);
    float GV = smithGgxG1(NoV, surface.alpha);
    float GL = smithGgxG1(abs(NoL), surface.alpha);

    float eta2 = surface.eta * surface.eta;
    float denominator = LdotH + surface.eta * VdotH;
    float denominator2 = denominator * denominator;

    if (denominator2 <= EPSILON) {
        return result;
    }

    float jacobian = eta2 * abs(LdotH) / denominator2;
    float pdfH = GV * abs(VdotH) * D / max(NoV, EPSILON);

    result.PDF = pdfH * jacobian;
    result.BxDF = surface.baseColor * D * GV * GL * eta2 / denominator2 * (abs(VdotH) * abs(LdotH) / max(NoV, EPSILON));
    return result;
}

// Enterprise PBR multi-scatter, uses kulla conty model
float3 evaluateGgxMultiScatter(float3 N, float3 V, float3 L, float alpha, float3 F0) {
    float NoV = dot(N, V);
    float NoL = dot(N, L);
    if (NoV <= EPSILON || NoL <= EPSILON) {
        return float3(0.0f);
    }

    float Ev = ggxDirectionalAlbedo(NoV, alpha);
    float El = ggxDirectionalAlbedo(NoL, alpha);
    float Eavg = ggxAverageAlbedo(alpha);

    float missingAverageEnergy = 1.0f - Eavg;
    if (missingAverageEnergy <= 1.0e-5f) {
        return float3(0.0f);
    }

    float Mms = ((1.0f - Ev) * (1.0f - El)) / (PI * missingAverageEnergy);
    float3 Favg = averageSchlickFresnel(F0);
    float3 denominator = float3(1.0f) - Favg * (1.0f - Eavg);
    float3 Fms = (Favg * Favg * Eavg) / max(denominator, float3(EPSILON));
    return Mms * Fms * NoL;
}

public BxDFEval evaluateBSDF(float3 N, float3 V, float3 L, SurfaceData surface) {
    BxDFEval result;
    result.BxDF = float3(0.0f);
    result.PDF = 0.0f;

    float NoV = dot(N, V);
    if (NoV <= EPSILON) {
        return result;
    }

    float metallicProbability;
    float dielectricProbability;
    float glassProbability;

    getComponentProbabilities(surface, metallicProbability, dielectricProbability, glassProbability);

    float NoL = dot(N, L);
    if (NoL > EPSILON) {
        float3 VplusL = V + L;
        float halfLengthSquared = dot(VplusL, VplusL);

        if (halfLengthSquared <= EPSILON) {
            return result;
        }

        float3 H = VplusL / sqrt(halfLengthSquared);
        float VoH = max(dot(V, H), 0.0f);

        if (VoH <= EPSILON) {
            return result;
        }

        float FDielectric = dielectricFresnel(VoH, surface.eta);
        float FView = dielectricFresnel(NoV, surface.eta);

        if (metallicProbability > EPSILON) {
            float3 F0 = surface.baseColor;
            float3 FMetal = fresnelSchlick(VoH, F0);
            BxDFEval metallicEval = evaluateGgxReflection(N, V, L, surface.alpha, FMetal);
            float3 multiScatter = evaluateGgxMultiScatter(N, V, L, surface.alpha, F0);
            result.BxDF += (metallicEval.BxDF + multiScatter) * metallicProbability;
            result.PDF += metallicEval.PDF * metallicProbability;
        }

        if (dielectricProbability > EPSILON) {
            BxDFEval diffuseEval = evaluateDiffuse(N, L, surface.baseColor);
            result.BxDF += diffuseEval.BxDF * dielectricProbability * (1.0f - FDielectric);
            result.PDF += diffuseEval.PDF * dielectricProbability * (1.0f - FView);

            BxDFEval specularEval = evaluateGgxReflection(N, V, L, surface.alpha, float3(FDielectric));
            float F0Scalar = dielectricF0(surface.ior);
            float3 multiScatter = evaluateGgxMultiScatter(N, V, L, surface.alpha, float3(F0Scalar));
            result.BxDF += (specularEval.BxDF + multiScatter) * dielectricProbability;
            result.PDF += specularEval.PDF * dielectricProbability * FView;
        }

        if (glassProbability > EPSILON) {
            BxDFEval glassReflection = evaluateGgxReflection(
                N,
                V,
                L,
                surface.alpha,
                float3(1.0f)
            );

            result.BxDF += glassReflection.BxDF * glassProbability * FDielectric;
            result.PDF += glassReflection.PDF * glassProbability * FDielectric;
        }

        return result;
    }

    if (NoL < -EPSILON && glassProbability > EPSILON) {
        float3 unnormalizedH = surface.eta * V + L;
        float hLengthSquared = dot(unnormalizedH, unnormalizedH);

        if (hLengthSquared <= EPSILON) {
            return result;
        }

        float3 H = unnormalizedH / sqrt(hLengthSquared);

        if (dot(N, H) < 0.0f) {
            H = -H;
        }

        float VdotH = dot(V, H);
        float LdotH = dot(L, H);

        bool validRefraction =
            (VdotH > 0.0f && LdotH < 0.0f) ||
            (VdotH < 0.0f && LdotH > 0.0f);

        if (!validRefraction) {
            return result;
        }

        float FDielectric = dielectricFresnel(abs(VdotH), surface.eta);
        BxDFEval glassRefraction = evaluateGgxRefraction(N, V, L, surface);

        result.BxDF += glassRefraction.BxDF * glassProbability * (1.0f - FDielectric);
        result.PDF += glassRefraction.PDF * glassProbability * (1.0f - FDielectric);
    }

    return result;
}

public BxDFSample sampleBSDF(float3 N, float3 V, SurfaceData surface, inout uint rngState) {
    BxDFSample sample;
    sample.L = float3(0.0f);
    sample.BxDF = float3(0.0f);
    sample.PDF = 0.0f;
    sample.component = BSDFComponent::Diffuse;

    float NoV = dot(N, V);
    if (NoV <= EPSILON) {
        return sample;
    }
    float metallicProbability;
    float dielectricProbability;
    float glassProbability;

    getComponentProbabilities(surface, metallicProbability, dielectricProbability, glassProbability);

    float x = randomFloat(rngState);
    if (x < metallicProbability) {
        float3 H = sampleGgxVndf(N, V, surface.alpha, rngState);
        sample.L = normalize(reflect(-V, H));
        sample.component = BSDFComponent::Metallic;
    } else if (x < metallicProbability + dielectricProbability) {
        float FView = dielectricFresnel(NoV, surface.eta);

        if (randomFloat(rngState) < FView) {
            float3 H = sampleGgxVndf(N, V, surface.alpha, rngState);
            sample.L = normalize(reflect(-V, H));
            sample.component = BSDFComponent::SpecularDielectric;
        } else {
            sample.L = sampleCosineHemisphere(N, rngState);
            sample.component = BSDFComponent::Diffuse;
        }
    } else {
        float3 H = sampleGgxVndf(N, V, surface.alpha, rngState);
        float VdotH = max(dot(V, H), 0.0f);

        if (VdotH <= EPSILON) {
            return sample;
        }

        float FDielectric = dielectricFresnel(VdotH, surface.eta);

        if (randomFloat(rngState) < FDielectric) {
            sample.L = normalize(reflect(-V, H));
            sample.component = BSDFComponent::GlassReflect;
        } else {
            float3 refracted = refract(-V, H, surface.eta);
            float lengthSquared = dot(refracted, refracted);
            if (lengthSquared <= EPSILON) {
                return sample;
            }

            sample.L = refracted / sqrt(lengthSquared);
            sample.component = BSDFComponent::GlassRefract;
        }
    }

    bool refracted = sample.component == BSDFComponent::GlassRefract;
    float NoL = dot(N, sample.L);

    if (!refracted && NoL <= EPSILON) {
        sample.PDF = 0.0f;
        return sample;
    }

    if (refracted && NoL >= -EPSILON) {
        sample.PDF = 0.0f;
        return sample;
    }

    BxDFEval evaluation = evaluateBSDF(N, V, sample.L, surface);
    sample.BxDF = evaluation.BxDF;
    sample.PDF = evaluation.PDF;
    return sample;
}

public bool scatterBsdf(Ray rayIn, HitRecord rec, Material material, out float3 attenuation, out Ray scattered, inout uint rngState) {
    SurfaceData surface = evaluateSurface(material, rec);
    float3 V = normalize(-rayIn.direction);
    float3 N = makeSafeShadingNormal(rec.normal, rec.geometricNormal, V);
    if (dot(N, V) <= EPSILON) {
        return false;
    }

    BxDFSample sample = sampleBSDF(N, V, surface, rngState);
    if (sample.PDF <= EPSILON) {
        return false;
    }

    scattered.direction = sample.L;
    scattered.origin = offsetRayOrigin(rec, sample.L);
    attenuation = sample.BxDF / sample.PDF;
    return true;
}