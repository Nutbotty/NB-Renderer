#language slang 2026
module Random;

import Constants;

public bool nearZero(float3 value) {
    return all(abs(value) < float3(EPSILON));
}

public uint pcgHash(uint inputValue) {
    uint state = inputValue * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

public float randomFloat(inout uint rngState) {
    rngState = pcgHash(rngState);
    return float(rngState >> 8u) * (1.0f / 16777216.0f);
}

public float3 randomVec3(inout uint rngState) {
    return float3(randomFloat(rngState), randomFloat(rngState), randomFloat(rngState));
}

public float3 randomVec3(float minValue, float maxValue, inout uint rngState) {
    return float3(lerp(minValue, maxValue, randomFloat(rngState)), lerp(minValue, maxValue, randomFloat(rngState)), lerp(minValue, maxValue, randomFloat(rngState)));
}

public float3 randomUnitVector(inout uint rngState) {
    for (int attempt = 0; attempt < 64; ++attempt) {
        float3 p = randomVec3(-1.0f, 1.0f, rngState);
        float lengthSquared = dot(p, p);
        if (lengthSquared > EPSILON && lengthSquared <= 1.0f) {
            return p / sqrt(lengthSquared);
        }
    }
    return float3(1.0f, 0.0f, 0.0f);
}

public float3 randomOnHemisphere(float3 normal, inout uint rngState) {
    float3 onUnitSphere = randomUnitVector(rngState);
    if (dot(onUnitSphere, normal) > 0.0f) {
        return onUnitSphere;
    }
    return -onUnitSphere;
}

public float2 randomInUnitDisk(inout uint rngState) {
    for (int attempt = 0; attempt < 64; ++attempt) {
        float2 p = float2(2.0f * randomFloat(rngState) - 1.0f, 2.0f * randomFloat(rngState) - 1.0f);
        if (dot(p, p) < 1.0f) {
            return p;
        }
    }
    return float2(0.0f);
}

public float2 sampleSquare(inout uint rngState) {
    return float2(randomFloat(rngState) - 0.5f, randomFloat(rngState) - 0.5f);
}
