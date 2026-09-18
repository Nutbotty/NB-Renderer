#language slang 2026
module Types;

public struct Sphere {
    float3 center;
    float radius;
    int material;
};

public struct Material {
    int type;
    float3 albedo;
    float opacity;
    float metallic;
    float roughness;
    float refractionIndex;
    float transmission;
    float3 emission;
    float emissionStrength;
    int baseColorTexture;
    int metallicRoughnessTexture;
    float fuzz;
};

public struct Ray {
    float3 origin;
    float3 direction;
};

public struct Camera {
    float aspectRatio;
    float3 center;
    float3 pixel00_loc;
    float3 pixel_delta_u;
    float3 pixel_delta_v;
    float3 defocus_disc_u;
    float3 defocus_disc_v;
};

public struct HitRecord {
    float3 p;
    float t;
    float3 normal;
    float2 uv;
    bool frontFace;
    int material;
};

public struct Interval {
    float min;
    float max;
};
