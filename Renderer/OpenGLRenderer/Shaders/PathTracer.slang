// PathTracer.slang
// Port of NB-Renderer's OpenGL 4.3 GLSL compute path tracer.
//
// Resource binding policy:
//   0      output image
//   1-12   scene buffers
//   13-15  sampled textures
//   16     render constants
//
// This keeps one unique binding namespace for Vulkan while also allowing
// deterministic OpenGL bindings.
//
// IMPORTANT:
// Compile with column-major matrix layout to match glm::mat4 as currently
// uploaded by NB-Renderer:
//   -matrix-layout-column-major

static const int MATERIAL_LAMBERTIAN = 0;
static const int MATERIAL_METAL = 1;
static const int MATERIAL_DIELECTRIC = 2;
static const int MATERIAL_VOLUME = 3;
static const int MATERIAL_PBR_METAL_ROUGH = 4;

static const int PRIMITIVE_SPHERE = 0;
static const int PRIMITIVE_QUAD = 1;
static const int PRIMITIVE_TRI = 2;
static const int PRIMITIVE_MESH = 3;

static const int TLAS_STACK_SIZE = 16;
static const int BLAS_STACK_SIZE = 32;

static const float PI = 3.14159265358979323846f;
static const float TWO_PI = 6.28318530717958647692f;
static const float EPSILON = 1.0e-8f;
static const float MAX_RAY_DISTANCE = 1.0e30f;

// -----------------------------------------------------------------------------
// GPU packed scene types
// -----------------------------------------------------------------------------

struct PackedMaterial
{
    float4 baseColor;
    // x = metallic/fuzz, y = roughness/fuzz fallback, z = IOR, w = transmission
    float4 surface;
    float4 emission;
    int4 metadata;
    int4 textureIndices;
};

struct Sphere
{
    float3 center;
    float radius;
    int material;
};

struct PackedSphere
{
    float4 centerRadius;
    int4 metadata;
};

struct PackedQuad
{
    float4 Q;
    float4 u;
    float4 v;
    int4 metadata;
};

struct PackedTriangle
{
    float4 Q;
    float4 U;
    float4 V;
    int4 metadata;
};

struct PackedBvhNode
{
    float4 boundsMin;
    float4 boundsMax;
    int4 metadata;
};

struct PackedPrimitiveRef
{
    // x = primitive type
    // y = primitive index
    // z = transform-buffer index
    // w = instance material-buffer offset, -1 = non-mesh
    int4 metadata;
};

struct PackedMeshVertex
{
    float4 position;
    float4 normal;
    float4 texCoord;
};

struct PackedMeshTriangle
{
    // x = v0, y = v1, z = v2, w = object-local material slot index
    int4 metadata;
};

struct PackedMesh
{
    float4 boundsMin;
    float4 boundsMax;
    // x = global BLAS root node
    // y = BLAS node count
    // z = first triangle
    // w = triangle count
    int4 metadata;
};

struct PackedTransform
{
    float4x4 objectToWorld;
    float4x4 worldToObject;
};

// -----------------------------------------------------------------------------
// Render constants
//
// Deliberately packed into 16-byte groups so the same host struct can be used
// cleanly with OpenGL UBO/std140, Vulkan, and D3D12.
// -----------------------------------------------------------------------------

struct RenderConstants
{
    int uTlasNodeCount;
    int uSamplesPerDispatch;
    int uMaxDepth;
    uint uAccumulatedSamples;
    uint uUseHdri;
    float uEnvironmentIntensity;
    float _padding0;
    float _padding1;
    float3 uCameraLookFrom;
    float uCameraVerticalFov;
    float3 uCameraLookAt;
    float uCameraFocusDistance;
    float3 uCameraVUp;
    float uCameraDefocusAngle;
    float3 uEnvironmentRotation;
    float _padding2;
    float3 uSkyColor1;
    float _padding3;
    float3 uSkyColor2;
    float _padding4;
};

// -----------------------------------------------------------------------------
// Resources
//
// vk::binding is used by Vulkan *and* generated GLSL/OpenGL.
// register() gives deterministic D3D12 register assignments.
// -----------------------------------------------------------------------------

[[vk::binding(0, 0)]]
RWTexture2D<float4> outputImage : register(u0, space0);

[[vk::binding(1, 0)]]
StructuredBuffer<PackedMaterial, Std430DataLayout> materialBuffer : register(t1, space0);

[[vk::binding(2, 0)]]
StructuredBuffer<PackedSphere, Std430DataLayout> sphereBuffer : register(t2, space0);

[[vk::binding(3, 0)]]
StructuredBuffer<PackedBvhNode, Std430DataLayout> tlasNodeBuffer : register(t3, space0);

[[vk::binding(4, 0)]]
StructuredBuffer<PackedQuad, Std430DataLayout> quadBuffer : register(t4, space0);

[[vk::binding(5, 0)]]
StructuredBuffer<PackedTriangle, Std430DataLayout> triangleBuffer : register(t5, space0);

[[vk::binding(6, 0)]]
StructuredBuffer<PackedPrimitiveRef, Std430DataLayout> primitiveRefBuffer : register(t6, space0);

[[vk::binding(7, 0)]]
StructuredBuffer<PackedTransform, Std430DataLayout> transformBuffer : register(t7, space0);

[[vk::binding(8, 0)]]
StructuredBuffer<PackedMeshVertex, Std430DataLayout> meshVertexBuffer : register(t8, space0);

[[vk::binding(9, 0)]]
StructuredBuffer<PackedMeshTriangle, Std430DataLayout> meshTriangleBuffer : register(t9, space0);

[[vk::binding(10, 0)]]
StructuredBuffer<PackedMesh, Std430DataLayout> meshBuffer : register(t10, space0);

[[vk::binding(11, 0)]]
StructuredBuffer<PackedBvhNode, Std430DataLayout> blasNodeBuffer : register(t11, space0);

[[vk::binding(12, 0)]]
StructuredBuffer<int, Std430DataLayout> instanceMaterialBuffer : register(t12, space0);

[[vk::binding(13, 0)]]
Sampler2D<float4> uEnvironmentMap;

[[vk::binding(14, 0)]]
Sampler2DArray<float4> uBaseColorTextures;

[[vk::binding(15, 0)]]
Sampler2DArray<float4> uMetalRoughTextures;

[[vk::binding(16, 0)]]
ConstantBuffer<RenderConstants> gRender : register(b16, space0);

// -----------------------------------------------------------------------------
// Path tracer working types
// -----------------------------------------------------------------------------

struct Material
{
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

struct Ray
{
    float3 origin;
    float3 direction;
};

struct Camera
{
    float aspectRatio;
    float3 center;
    float3 pixel00_loc;
    float3 pixel_delta_u;
    float3 pixel_delta_v;
    float3 defocus_disc_u;
    float3 defocus_disc_v;
};

struct HitRecord
{
    float3 p;
    float t;
    float3 normal;
    float2 uv;
    bool frontFace;
    int material;
};

struct Interval
{
    float min;
    float max;
};

// -----------------------------------------------------------------------------
// Small constructors/helpers
// -----------------------------------------------------------------------------

Ray makeRay(float3 origin, float3 direction)
{
    Ray result;
    result.origin = origin;
    result.direction = direction;
    return result;
}

Interval makeInterval(float minValue, float maxValue) {
    Interval result;
    result.min = minValue;
    result.max = maxValue;
    return result;
}

float3 transformPoint(float4x4 transform, float3 point) {
    return mul(float4(point, 1.0f), transform).xyz;
}

float3 transformVector(float4x4 transform, float3 vectorValue) {
    return mul(float4(vectorValue, 0.0f), transform).xyz;
}

Ray transformRay(float4x4 transform, Ray ray) {
    Ray transformed;
    transformed.origin = transformPoint(transform, ray.origin);
    transformed.direction = transformVector(transform, ray.direction);
    return transformed;
}

float3 rayAt(Ray ray, float t)
{
    return ray.origin + t * ray.direction;
}

// -----------------------------------------------------------------------------
// Material loading
// -----------------------------------------------------------------------------

Material loadMaterial(int index)
{
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
    // The current CPU PackedMaterial has no dedicated fuzz member.
    // Use surface.y as a deterministic fallback instead of leaving fuzz
    // uninitialized. If you add a dedicated GPU fuzz field later, change this.
    material.fuzz = gpuMaterial.surface.y;
    return material;
}

// -----------------------------------------------------------------------------
// Hit-record helpers
// -----------------------------------------------------------------------------

void setFaceNormal(Ray ray, float3 outwardNormal, inout HitRecord record)
{
    record.frontFace = dot(ray.direction, outwardNormal) < 0.0f;
    record.normal = record.frontFace ? outwardNormal : -outwardNormal;
}

// -----------------------------------------------------------------------------
// Interval helpers
// -----------------------------------------------------------------------------

float intervalSize(Interval interval)
{
    return interval.max - interval.min;
}

bool intervalContains(Interval interval, float value)
{
    return interval.min <= value && value <= interval.max;
}

bool intervalSurrounds(Interval interval, float value)
{
    return interval.min < value && value < interval.max;
}

float intervalClamp(Interval interval, float value)
{
    return clamp(value, interval.min, interval.max);
}

// -----------------------------------------------------------------------------
// RNG / sampling
// -----------------------------------------------------------------------------

bool nearZero(float3 value)
{
    return all(abs(value) < float3(EPSILON));
}

uint pcgHash(uint inputValue)
{
    uint state = inputValue * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float randomFloat(inout uint rngState)
{
    rngState = pcgHash(rngState);
    return float(rngState >> 8u) * (1.0f / 16777216.0f);
}

float3 randomVec3(inout uint rngState)
{
    return float3(randomFloat(rngState), randomFloat(rngState), randomFloat(rngState));
}

float3 randomVec3(float minValue, float maxValue, inout uint rngState)
{
    return float3(lerp(minValue, maxValue, randomFloat(rngState)), lerp(minValue, maxValue, randomFloat(rngState)), lerp(minValue, maxValue, randomFloat(rngState)));
}

float3 randomUnitVector(inout uint rngState)
{
    for (int attempt = 0; attempt < 64; ++attempt)
    {
        float3 p = randomVec3(-1.0f, 1.0f, rngState);
        float lengthSquared = dot(p, p);
        if (lengthSquared > EPSILON && lengthSquared <= 1.0f)
        {
            return p / sqrt(lengthSquared);
        }
    }
    return float3(1.0f, 0.0f, 0.0f);
}

float3 randomOnHemisphere(float3 normal, inout uint rngState)
{
    float3 onUnitSphere = randomUnitVector(rngState);
    if (dot(onUnitSphere, normal) > 0.0f)
    {
        return onUnitSphere;
    }
    return -onUnitSphere;
}

float2 randomInUnitDisk(inout uint rngState)
{
    for (int attempt = 0; attempt < 64; ++attempt)
    {
        float2 p = float2(2.0f * randomFloat(rngState) - 1.0f, 2.0f * randomFloat(rngState) - 1.0f);
        if (dot(p, p) < 1.0f)
        {
            return p;
        }
    }
    return float2(0.0f);
}

float2 sampleSquare(inout uint rngState)
{
    return float2(randomFloat(rngState) - 0.5f, randomFloat(rngState) - 0.5f);
}

// -----------------------------------------------------------------------------
// Material scattering
// -----------------------------------------------------------------------------

float3 emitted(Material material)
{
    return material.emission * material.emissionStrength;
}

bool scatterLambertian(Ray rayIn, HitRecord rec, Material material, out float3 attenuation, out Ray scattered, inout uint rngState)
{
    float3 scatterDirection = rec.normal + randomUnitVector(rngState);
    if (nearZero(scatterDirection))
    {
        scatterDirection = rec.normal;
    }
    scattered.origin = rec.p;
    scattered.direction = scatterDirection;
    attenuation = material.albedo;
    return true;
}

bool scatterMetal(Ray rayIn, HitRecord rec, Material material, out float3 attenuation, out Ray scattered, inout uint rngState)
{
    float3 reflected = reflect(normalize(rayIn.direction), rec.normal);
    float fuzz = clamp(material.fuzz, 0.0f, 1.0f);
    scattered.origin = rec.p;
    scattered.direction = reflected + fuzz * randomUnitVector(rngState);
    attenuation = material.albedo;
    return dot(scattered.direction, rec.normal) > 0.0f;
}

float reflectance(float cosine, float refractionIndex)
{
    float r0 = (1.0f - refractionIndex) / (1.0f + refractionIndex);
    r0 *= r0;
    return r0 + (1.0f - r0) * pow(1.0f - cosine, 5.0f);
}

bool scatterDielectric(Ray rayIn, HitRecord rec, Material material, out float3 attenuation, out Ray scattered, inout uint rngState)
{
    attenuation = float3(1.0f);
    float ri = rec.frontFace ? 1.0f / material.refractionIndex : material.refractionIndex;
    float3 unitDirection = normalize(rayIn.direction);
    float cosTheta = min(dot(-unitDirection, rec.normal), 1.0f);
    float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    bool cannotRefract = ri * sinTheta > 1.0f;
    float3 direction;
    if (cannotRefract || reflectance(cosTheta, ri) > randomFloat(rngState))
    {
        direction = reflect(unitDirection, rec.normal);
    }
    else
    {
        direction = refract(unitDirection, rec.normal, ri);
    }
    scattered.origin = rec.p;
    scattered.direction = direction;
    return true;
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (float3(1.0f) - F0) * pow(1.0f - clamp(cosTheta, 0.0f, 1.0f), 5.0f);
}

float luminance(float3 color)
{
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 offsetRayOrigin(HitRecord rec, float3 direction)
{
    static const float RAY_EPSILON = 0.001f;
    float side = dot(direction, rec.normal) >= 0.0f ? 1.0f : -1.0f;
    return rec.p + rec.normal * side * RAY_EPSILON;
}

bool scatterPbrMetallicRoughness(Ray rayIn, HitRecord rec, Material material, out float3 attenuation, out Ray scattered, inout uint rngState) {
    float3 baseColor = material.albedo;
    float opacity = material.opacity;
    float metallic = material.metallic;
    float roughness = material.roughness;
    if (material.baseColorTexture >= 0)
    {
        float4 texel = uBaseColorTextures.SampleLevel(float3(rec.uv, float(material.baseColorTexture)), 0.0f);
        baseColor *= texel.rgb;
        opacity *= texel.a;
    }
    if (material.metallicRoughnessTexture >= 0)
    {
        float4 mr = uMetalRoughTextures.SampleLevel(float3(rec.uv, float(material.metallicRoughnessTexture)), 0.0f);
        roughness *= mr.g;
        metallic *= mr.b;
    }
    metallic = clamp(metallic, 0.0f, 1.0f);
    roughness = clamp(roughness, 0.01f, 1.0f);
    float transmission = clamp(material.transmission, 0.0f, 1.0f);
    float transmissionProbability = transmission * (1.0f - metallic);
    if (transmissionProbability > 0.0f && randomFloat(rngState) < transmissionProbability)
    {
        attenuation = float3(1.0f);
        float ri = rec.frontFace ? 1.0f / material.refractionIndex : material.refractionIndex;
        float3 unitDirection = normalize(rayIn.direction);
        float cosTheta = min(dot(-unitDirection, rec.normal), 1.0f);
        float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
        bool cannotRefract = ri * sinTheta > 1.0f;
        float3 direction;
        if (cannotRefract || reflectance(cosTheta, ri) > randomFloat(rngState))
        {
            direction = reflect(unitDirection, rec.normal);
            attenuation = float3(1.0f);
        }
        else
        {
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
    if (totalEnergy <= EPSILON)
    {
        return false;
    }
    float specularProbability = clamp(specularEnergy / totalEnergy, 0.05f, 0.95f);
    if (randomFloat(rngState) < specularProbability)
    {
        float3 reflected = reflect(normalize(rayIn.direction), rec.normal);
        float spread = roughness * roughness;
        float3 direction = normalize(reflected + spread * randomUnitVector(rngState));
        if (dot(direction, rec.normal) <= 0.0f)
        {
            return false;
        }
        scattered.origin = rec.p;
        scattered.direction = direction;
        attenuation = specularWeight / specularProbability;
        return true;
    }
    float3 scatterDirection = rec.normal + randomUnitVector(rngState);
    if (nearZero(scatterDirection))
    {
        scatterDirection = rec.normal;
    }
    scattered.origin = rec.p;
    scattered.direction = scatterDirection;
    attenuation = diffuseWeight / (1.0f - specularProbability);
    return true;
}

bool scatterMaterial(Ray rayIn, HitRecord rec, out float3 attenuation, out Ray scattered, Material material, inout uint rngState)
{
    if (material.type == MATERIAL_LAMBERTIAN)
    {
        return scatterLambertian(rayIn, rec, material, attenuation, scattered, rngState);
    }
    if (material.type == MATERIAL_METAL)
    {
        return scatterMetal(rayIn, rec, material, attenuation, scattered, rngState);
    }
    if (material.type == MATERIAL_DIELECTRIC)
    {
        return scatterDielectric(rayIn, rec, material, attenuation, scattered, rngState);
    }
    if (material.type == MATERIAL_PBR_METAL_ROUGH)
    {
        return scatterPbrMetallicRoughness(rayIn, rec, material, attenuation, scattered, rngState);
    }
    return false;
}

// -----------------------------------------------------------------------------
// Primitive loading / intersections
// -----------------------------------------------------------------------------

Sphere loadSphere(int index)
{
    PackedSphere gpuSphere = sphereBuffer[index];
    Sphere sphere;
    sphere.center = gpuSphere.centerRadius.xyz;
    sphere.radius = gpuSphere.centerRadius.w;
    sphere.material = gpuSphere.metadata.x;
    return sphere;
}

bool hitAabb(PackedBvhNode node, Ray ray, float3 inverseDirection, float rayTMin, float rayTMax, out float entryT)
{
    float3 t0 = (node.boundsMin.xyz - ray.origin) * inverseDirection;
    float3 t1 = (node.boundsMax.xyz - ray.origin) * inverseDirection;
    float3 nearT = min(t0, t1);
    float3 farT = max(t0, t1);
    float currentMin = max(rayTMin, max(nearT.x, max(nearT.y, nearT.z)));
    float currentMax = min(rayTMax, min(farT.x, min(farT.y, farT.z)));
    entryT = currentMin;
    return currentMax >= currentMin;
}

bool hitSphere(Sphere sphere, Ray ray, Interval rayT, out HitRecord record)
{
    float3 oc = ray.origin - sphere.center;
    float a = dot(ray.direction, ray.direction);
    float h = dot(oc, ray.direction);
    float c = dot(oc, oc) - sphere.radius * sphere.radius;
    float discriminant = h * h - a * c;
    if (discriminant < 0.0f)
    {
        return false;
    }
    float sqrtDiscriminant = sqrt(discriminant);
    float root = (-h - sqrtDiscriminant) / a;
    if (!intervalSurrounds(rayT, root))
    {
        root = (-h + sqrtDiscriminant) / a;
        if (!intervalSurrounds(rayT, root))
        {
            return false;
        }
    }
    record.t = root;
    record.p = rayAt(ray, record.t);
    float3 outwardNormal = (record.p - sphere.center) / sphere.radius;
    setFaceNormal(ray, outwardNormal, record);
    record.material = sphere.material;
    record.uv = float2(0.0f);
    return true;
}

bool hitQuad(int quadIndex, Ray ray, Interval rayT, out HitRecord record)
{
    PackedQuad quad = quadBuffer[quadIndex];
    float3 Q = quad.Q.xyz;
    float3 u = quad.u.xyz;
    float3 v = quad.v.xyz;
    float3 n = cross(u, v);
    float nLengthSquared = dot(n, n);
    if (nLengthSquared < EPSILON)
    {
        return false;
    }
    float3 normal = n / sqrt(nLengthSquared);
    float denom = dot(normal, ray.direction);
    if (abs(denom) < EPSILON)
    {
        return false;
    }
    float D = dot(normal, Q);
    float t = (D - dot(normal, ray.origin)) / denom;
    if (!intervalSurrounds(rayT, t))
    {
        return false;
    }
    float3 intersection = rayAt(ray, t);
    float3 planarHit = intersection - Q;
    float3 w = n / nLengthSquared;
    float alpha = dot(w, cross(planarHit, v));
    float beta = dot(w, cross(u, planarHit));
    if (alpha < 0.0f || alpha > 1.0f || beta < 0.0f || beta > 1.0f)
    {
        return false;
    }
    record.t = t;
    record.p = intersection;
    record.material = quad.metadata.x;
    record.uv = float2(alpha, beta);
    setFaceNormal(ray, normal, record);
    return true;
}

bool hitTri(int triIndex, Ray ray, Interval rayT, out HitRecord record)
{
    PackedTriangle tri = triangleBuffer[triIndex];
    float3 Q = tri.Q.xyz;
    float3 U = tri.U.xyz;
    float3 V = tri.V.xyz;
    float3 edge1 = U - Q;
    float3 edge2 = V - Q;
    float3 normal = normalize(cross(edge1, edge2));
    float3 h = cross(ray.direction, edge2);
    float determinant = dot(edge1, h);
    if (abs(determinant) < EPSILON)
    {
        return false;
    }
    float inverseDeterminant = 1.0f / determinant;
    float3 s = ray.origin - Q;
    float u = inverseDeterminant * dot(s, h);
    if (u < EPSILON || u - 1.0f > EPSILON)
    {
        return false;
    }
    float3 q = cross(s, edge1);
    float v = inverseDeterminant * dot(ray.direction, q);
    if (v < -EPSILON || u + v - 1.0f > EPSILON)
    {
        return false;
    }
    float t = inverseDeterminant * dot(edge2, q);
    if (!intervalSurrounds(rayT, t))
    {
        return false;
    }
    record.t = t;
    record.p = rayAt(ray, t);
    record.material = tri.metadata.x;
    record.uv = float2(u, v);
    setFaceNormal(ray, normal, record);
    return true;
}

bool hitMeshTri(int triIndex, Ray ray, Interval rayT, out HitRecord record)
{
    PackedMeshTriangle tri = meshTriangleBuffer[triIndex];
    int index0 = tri.metadata.x;
    int index1 = tri.metadata.y;
    int index2 = tri.metadata.z;
    float3 a = meshVertexBuffer[index0].position.xyz;
    float3 b = meshVertexBuffer[index1].position.xyz;
    float3 c = meshVertexBuffer[index2].position.xyz;
    float3 edge1 = b - a;
    float3 edge2 = c - a;
    float3 p = cross(ray.direction, edge2);
    float determinant = dot(edge1, p);
    if (abs(determinant) < EPSILON)
    {
        return false;
    }
    float inverseDeterminant = 1.0f / determinant;
    float3 s = ray.origin - a;
    float u = inverseDeterminant * dot(s, p);
    if (u < 0.0f || u > 1.0f)
    {
        return false;
    }
    float3 q = cross(s, edge1);
    float v = inverseDeterminant * dot(ray.direction, q);
    if (v < 0.0f || u + v > 1.0f)
    {
        return false;
    }
    float t = inverseDeterminant * dot(edge2, q);
    if (!intervalSurrounds(rayT, t))
    {
        return false;
    }
    record.t = t;
    record.p = rayAt(ray, t);
    // Material is still an object-local slot here.
    record.material = tri.metadata.w;
    float w = 1.0f - u - v;
    float3 n0 = meshVertexBuffer[index0].normal.xyz;
    float3 n1 = meshVertexBuffer[index1].normal.xyz;
    float3 n2 = meshVertexBuffer[index2].normal.xyz;
    float3 interpolatedNormal = w * n0 + u * n1 + v * n2;
    float3 outwardNormal;
    if (dot(interpolatedNormal, interpolatedNormal) > EPSILON)
    {
        outwardNormal = normalize(interpolatedNormal);
    }
    else
    {
        outwardNormal = normalize(cross(edge1, edge2));
    }
    setFaceNormal(ray, outwardNormal, record);
    float2 uv0 = meshVertexBuffer[index0].texCoord.xy;
    float2 uv1 = meshVertexBuffer[index1].texCoord.xy;
    float2 uv2 = meshVertexBuffer[index2].texCoord.xy;
    record.uv = w * uv0 + u * uv1 + v * uv2;
    return true;
}

// -----------------------------------------------------------------------------
// BLAS traversal
// -----------------------------------------------------------------------------

bool hitMesh(int meshIndex, Ray localRay, Interval rayT, out HitRecord record) {
    PackedMesh mesh = meshBuffer[meshIndex];
    int rootNode = mesh.metadata.x;
    bool hitAnything = false;
    float closestSoFar = rayT.max;
    HitRecord temporaryRecord;
    float3 inverseDirection = 1.0f / localRay.direction;
    float rootEntryT;
    if (!hitAabb(blasNodeBuffer[rootNode], localRay, inverseDirection, rayT.min, closestSoFar, rootEntryT)) {
        return false;
    }
    int stack[BLAS_STACK_SIZE];
    int stackSize = 0;
    stack[stackSize++] = rootNode;
    while (stackSize > 0) {
        int nodeIndex = stack[--stackSize];
        PackedBvhNode node = blasNodeBuffer[nodeIndex];
        bool isLeaf = node.metadata.z != 0;
        if (isLeaf) {
            int firstTriangle = node.metadata.x;
            int triangleCount = node.metadata.y;
            for (int triangleOffset = 0; triangleOffset < triangleCount; ++triangleOffset) {
                int triangleIndex = firstTriangle + triangleOffset;
                if (hitMeshTri(triangleIndex, localRay, makeInterval(rayT.min, closestSoFar), temporaryRecord)) {
                    hitAnything = true;
                    closestSoFar = temporaryRecord.t;
                    record = temporaryRecord;
                }
            }
            continue;
        }
        int leftChild = node.metadata.x;
        int rightChild = node.metadata.y;
        float leftEntryT;
        float rightEntryT;
        bool hitLeft = hitAabb(blasNodeBuffer[leftChild], localRay, inverseDirection, rayT.min, closestSoFar, leftEntryT);
        bool hitRight = hitAabb(blasNodeBuffer[rightChild], localRay, inverseDirection, rayT.min, closestSoFar, rightEntryT);
        if (hitLeft && hitRight) {
            // Push farther first so nearer is popped first.
            if (leftEntryT < rightEntryT) {
                stack[stackSize++] = rightChild;
                stack[stackSize++] = leftChild;
            } else {
                stack[stackSize++] = leftChild;
                stack[stackSize++] = rightChild;
            }
        } else if (hitLeft) {
            stack[stackSize++] = leftChild;
        } else if (hitRight) {
            stack[stackSize++] = rightChild;
        }
    }
    return hitAnything;
}

// -----------------------------------------------------------------------------
// Primitive dispatch / instance transforms
// -----------------------------------------------------------------------------

bool hitLocalPrimitive(int primitiveType, int primitiveIndex, Ray ray, Interval rayT, out HitRecord record) {
    if (primitiveType == PRIMITIVE_SPHERE) {
        return hitSphere(loadSphere(primitiveIndex), ray, rayT, record);
    }
    if (primitiveType == PRIMITIVE_QUAD) {
        return hitQuad(primitiveIndex, ray, rayT, record);
    }
    if (primitiveType == PRIMITIVE_TRI) {
        return hitTri(primitiveIndex, ray, rayT, record);
    }
    if (primitiveType == PRIMITIVE_MESH) {
        return hitMesh(primitiveIndex, ray, rayT, record);
    }
    return false;
}

bool hitPrimitive(PackedPrimitiveRef primitiveRef, Ray ray, Interval rayT, out HitRecord record) {
    int primitiveType = primitiveRef.metadata.x;
    int primitiveIndex = primitiveRef.metadata.y;
    int transformIndex = primitiveRef.metadata.z;
    int materialOffset = primitiveRef.metadata.w;
    if (transformIndex == 0) {
        if (!hitLocalPrimitive(primitiveType, primitiveIndex, ray, rayT, record)) {
            return false;
        }
        if (primitiveType == PRIMITIVE_MESH) {
            int materialSlot = record.material;
            if (materialOffset < 0 || materialSlot < 0) {
                record.material = 0;
            } else {
                uint materialCount = uint(instanceMaterialBuffer.getCount());
                int materialIndex = materialOffset + materialSlot;
                if (materialIndex < 0 || uint(materialIndex) >= materialCount) {
                    record.material = 0;
                } else {
                    record.material = instanceMaterialBuffer[materialIndex];
                }
            }
        }
        return true;
    }
    float4x4 worldToObject = transformBuffer[transformIndex].worldToObject;
    Ray localRay = transformRay(worldToObject, ray);
    HitRecord localRecord;
    if (!hitLocalPrimitive(primitiveType, primitiveIndex, localRay, rayT, localRecord)) {
        return false;
    }
    record = localRecord;
    // Because localRay.direction is intentionally NOT normalized after
    // transforming into object space, localRecord.t remains compatible
    // with the original world-space ray.
    record.p = rayAt(ray, localRecord.t);
    float3 localOutwardNormal = localRecord.frontFace ? localRecord.normal : -localRecord.normal;
    float3x3 normalToWorld = transpose((float3x3)worldToObject);
    float3 worldOutwardNormal = normalize(mul(localOutwardNormal, normalToWorld));
    setFaceNormal(ray, worldOutwardNormal, record);
    if (primitiveType == PRIMITIVE_MESH) {
        int materialSlot = record.material;
        int materialIndex = materialOffset + materialSlot;
        uint materialCount = uint(instanceMaterialBuffer.getCount());
        if (materialOffset < 0 || materialSlot < 0 || materialIndex < 0 || uint(materialIndex) >= materialCount) {
            record.material = 0;
        } else {
            record.material = instanceMaterialBuffer[materialIndex];
        }
    }
    return true;
}

// -----------------------------------------------------------------------------
// TLAS traversal
// -----------------------------------------------------------------------------

bool hitWorld(Ray ray, Interval rayT, out HitRecord record) {
    if (gRender.uTlasNodeCount <= 0) {
        return false;
    }
    bool hitAnything = false;
    float closestSoFar = rayT.max;
    HitRecord temporaryRecord;
    float rootEntryT;
    float3 inverseDirection = 1.0f / ray.direction;
    if (!hitAabb(tlasNodeBuffer[0], ray, inverseDirection, rayT.min, closestSoFar, rootEntryT)) {
        return false;
    }
    int stack[TLAS_STACK_SIZE];
    int stackSize = 0;
    stack[stackSize++] = 0;
    while (stackSize > 0) {
        int nodeIndex = stack[--stackSize];
        PackedBvhNode node = tlasNodeBuffer[nodeIndex];
        bool isLeaf = node.metadata.z != 0;
        if (isLeaf) {
            int firstPrimitive = node.metadata.x;
            int primitiveCount = node.metadata.y;
            for (int primitiveOffset = 0; primitiveOffset < primitiveCount; ++primitiveOffset) {
                int primitiveRefIndex = firstPrimitive + primitiveOffset;
                PackedPrimitiveRef primitiveRef = primitiveRefBuffer[primitiveRefIndex];
                if (hitPrimitive(primitiveRef, ray, makeInterval(rayT.min, closestSoFar), temporaryRecord)) {
                    hitAnything = true;
                    closestSoFar = temporaryRecord.t;
                    record = temporaryRecord;
                }
            }
            continue;
        }
        int leftChild = node.metadata.x;
        int rightChild = node.metadata.y;
        PackedBvhNode leftNode = tlasNodeBuffer[leftChild];
        PackedBvhNode rightNode = tlasNodeBuffer[rightChild];
        float leftEntryT;
        float rightEntryT;
        bool hitLeft = hitAabb(leftNode, ray, inverseDirection, rayT.min, closestSoFar, leftEntryT);
        bool hitRight = hitAabb(rightNode, ray, inverseDirection, rayT.min, closestSoFar, rightEntryT);
        if (hitLeft && hitRight) {
            if (leftEntryT < rightEntryT) {
                stack[stackSize++] = rightChild;
                stack[stackSize++] = leftChild;
            }
            else {
                stack[stackSize++] = leftChild;
                stack[stackSize++] = rightChild;
            }
        }
        else if (hitLeft) {
            stack[stackSize++] = leftChild;
        }
        else if (hitRight) {
            stack[stackSize++] = rightChild;
        }
    }
    return hitAnything;
}

// -----------------------------------------------------------------------------
// Environment
// -----------------------------------------------------------------------------

float2 directionToEnvironmentUv(float3 direction) {
    float3 d = normalize(direction);
    float phi = atan2(d.z, d.x) + gRender.uEnvironmentRotation.y;
    float theta = acos(clamp(d.y, -1.0f, 1.0f));
    float u = frac(phi / TWO_PI + 0.5f);
    float v = theta / PI;
    return float2(u, v);
}

float3 sampleEnvironment(float3 direction) {
    float2 uv = directionToEnvironmentUv(direction);
    float3 radiance = uEnvironmentMap.SampleLevel(uv, 0.0f).rgb;
    return radiance * gRender.uEnvironmentIntensity;
}

float3 skyBoxColor(Ray ray) {
    if (gRender.uUseHdri != 0u) {
        return sampleEnvironment(ray.direction);
    }
    float3 unitDirection = normalize(ray.direction);
    float blend = 0.5f * (unitDirection.y + 1.0f);
    return lerp(gRender.uSkyColor1, gRender.uSkyColor2, blend);
}

// -----------------------------------------------------------------------------
// Path tracing
// -----------------------------------------------------------------------------

float3 rayColor(Ray ray, inout uint rngState) {
    Ray currentRay = ray;
    float3 throughput = float3(1.0f);
    float3 radiance = float3(0.0f);
    for (int bounce = 0; bounce < gRender.uMaxDepth; ++bounce) {
        HitRecord rec;
        if (hitWorld(currentRay, makeInterval(EPSILON, MAX_RAY_DISTANCE), rec)) {
            Material material = loadMaterial(rec.material);
            radiance += throughput * emitted(material);
            Ray scattered;
            float3 attenuation;
            if (!scatterMaterial(currentRay, rec, attenuation, scattered, material, rngState)) {
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

// -----------------------------------------------------------------------------
// Camera
// -----------------------------------------------------------------------------
Camera makeCamera(int2 imageDimensions) {
    Camera camera;
    camera.center = gRender.uCameraLookFrom;
    float imageWidth = float(imageDimensions.x);
    float imageHeight = float(imageDimensions.y);
    camera.aspectRatio = imageWidth / imageHeight;
    // w points backward from target toward camera.
    float3 w = normalize(gRender.uCameraLookFrom - gRender.uCameraLookAt);
    float3 u = normalize(cross(gRender.uCameraVUp, w));
    float3 v = cross(w, u);
    float theta = radians(gRender.uCameraVerticalFov);
    float viewportHeight = 2.0f * tan(theta * 0.5f) * gRender.uCameraFocusDistance;
    float viewportWidth = viewportHeight * camera.aspectRatio;
    float3 viewportU = viewportWidth * u;
    float3 viewportV = viewportHeight * v;
    camera.pixel_delta_u = viewportU / imageWidth;
    camera.pixel_delta_v = viewportV / imageHeight;
    float3 viewportUpperLeft = camera.center - gRender.uCameraFocusDistance * w - viewportU * 0.5f - viewportV * 0.5f;
    camera.pixel00_loc = viewportUpperLeft + 0.5f * (camera.pixel_delta_u + camera.pixel_delta_v);
    float defocusRadius = gRender.uCameraFocusDistance * tan(radians(gRender.uCameraDefocusAngle * 0.5f));
    camera.defocus_disc_u = u * defocusRadius;
    camera.defocus_disc_v = v * defocusRadius;
    return camera;
}

float3 defocusDiskSample(Camera camera, inout uint rngState) {
    float2 p = randomInUnitDisk(rngState);
    return camera.center + p.x * camera.defocus_disc_u + p.y * camera.defocus_disc_v;
}

Ray getCameraRay(Camera camera, int2 pixel, inout uint rngState) {
    float2 offset = sampleSquare(rngState);
    float3 pixelSample = camera.pixel00_loc + (float(pixel.x) + offset.x) * camera.pixel_delta_u + (float(pixel.y) + offset.y) * camera.pixel_delta_v;
    float3 rayOrigin;
    if (gRender.uCameraDefocusAngle <= 0.0f) {
        rayOrigin = camera.center;
    } else {
        rayOrigin = defocusDiskSample(camera, rngState);
    }
    return makeRay(rayOrigin, pixelSample - rayOrigin);
}

// -----------------------------------------------------------------------------
// Compute entry point
// -----------------------------------------------------------------------------

[shader("compute")] [numthreads(16, 16, 1)] void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint imageWidth;
    uint imageHeight;
    outputImage.GetDimensions(imageWidth, imageHeight);
    uint2 pixel = dispatchThreadID.xy;
    if (pixel.x >= imageWidth || pixel.y >= imageHeight) {
        return;
    }
    int2 pixelCoord = int2(pixel);
    int2 imageDimensions = int2(int(imageWidth), int(imageHeight));
    uint pixelIndex = pixel.x + pixel.y * imageWidth;
    Camera camera = makeCamera(imageDimensions);
    float3 colorSum = float3(0.0f);
    int samplesThisDispatch = max(gRender.uSamplesPerDispatch, 1);
    for (int sampleIndex = 0; sampleIndex < samplesThisDispatch; ++sampleIndex) {
        // Each absolute sample receives a stable independent stream.
        uint absoluteSampleIndex = gRender.uAccumulatedSamples + uint(sampleIndex);
        uint rngState = pcgHash(pcgHash(pixelIndex + 1u) ^ pcgHash(absoluteSampleIndex + 1u));
        Ray ray = getCameraRay(camera, pixelCoord, rngState);
        colorSum += rayColor(ray, rngState);
    }
    float previousSampleCount = float(gRender.uAccumulatedSamples);
    float newSampleCount = float(samplesThisDispatch);
    float3 previousAverage = float3(0.0f);
    if (gRender.uAccumulatedSamples > 0u) {
        previousAverage = outputImage[pixelCoord].rgb;
    }
    // Correct weighted accumulation when a dispatch contributes more than
    // one sample per pixel.
    float3 accumulatedAverageColor = (previousAverage * previousSampleCount + colorSum) / (previousSampleCount + newSampleCount);
    outputImage[pixelCoord] = float4(accumulatedAverageColor, 1.0f);
}