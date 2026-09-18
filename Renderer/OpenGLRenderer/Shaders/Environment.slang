#language slang 2026
module Environment;

import Constants;
import Types;
import Resources;

public float2 directionToEnvironmentUv(float3 direction) {
    float3 d = normalize(direction);
    float phi = atan2(d.z, d.x) + gRender.uEnvironmentRotation.y;
    float theta = acos(clamp(d.y, -1.0f, 1.0f));
    float u = frac(phi / TWO_PI + 0.5f);
    float v = theta / PI;
    return float2(u, v);
}

public float3 sampleEnvironment(float3 direction) {
    float2 uv = directionToEnvironmentUv(direction);
    float3 radiance = uEnvironmentMap.SampleLevel(uv, 0.0f).rgb;
    return radiance * gRender.uEnvironmentIntensity;
}

public float3 skyBoxColor(Ray ray) {
    if (gRender.uUseHdri != 0u) {
        return sampleEnvironment(ray.direction);
    }
    float3 unitDirection = normalize(ray.direction);
    float blend = 0.5f * (unitDirection.y + 1.0f);
    return lerp(gRender.uSkyColor1, gRender.uSkyColor2, blend);
}
