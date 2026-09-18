#language slang 2026
module Integrator;

import Constants;
import Types;
import Core;
import Resources;
import Materials;
import Bvh;
import Environment;

public float3 rayColor(Ray ray, inout uint rngState) {
    Ray currentRay = ray;
    float3 throughput = float3(1.0f);
    float3 radiance = float3(0.0f);
    for (int bounce = 0; bounce < gRender.uMaxDepth; ++bounce) {
        HitRecord rec;
        if (hitWorld(currentRay, makeInterval(RAY_EPSILON, MAX_RAY_DISTANCE), rec)) {
            // normals test/debug, add variable later
            // return rec.normal * 0.5f + 0.5f;
            Material material = loadMaterial(rec.material);
            radiance += throughput * emitted(material);
            Ray scattered;
            float3 attenuation;
            if (!scatterBsdf(currentRay, rec, material, attenuation, scattered, rngState)) {
                break;
            }
            throughput *= attenuation;
            currentRay = scattered;
        } else {
            radiance += throughput * skyBoxColor(currentRay);
            return radiance;
        }
    }
    return radiance;
}
