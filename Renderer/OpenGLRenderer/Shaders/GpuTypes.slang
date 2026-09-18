#language slang 2026
module GpuTypes;

public struct PackedMaterial {
    float4 baseColor;
    // x = metallic/fuzz, y = roughness/fuzz fallback, z = IOR, w = transmission
    float4 surface;
    float4 emission;
    int4 metadata;
    int4 textureIndices;
};

public struct PackedSphere {
    float4 centerRadius;
    int4 metadata;
};

public struct PackedQuad {
    float4 Q;
    float4 u;
    float4 v;
    int4 metadata;
};

public struct PackedTriangle {
    float4 Q;
    float4 U;
    float4 V;
    int4 metadata;
};

public struct PackedBvhNode {
    float4 boundsMin;
    float4 boundsMax;
    int4 metadata;
};

public struct PackedPrimitiveRef {
    // x = primitive type
    // y = primitive index
    // z = transform-buffer index
    // w = instance material-buffer offset, -1 = non-mesh
    int4 metadata;
};

public struct PackedMeshVertex {
    float4 position;
    float4 normal;
    float4 texCoord;
};

public struct PackedMeshTriangle {
    // x = v0, y = v1, z = v2, w = object-local material slot index
    int4 metadata;
};

public struct PackedMesh {
    float4 boundsMin;
    float4 boundsMax;

    // x = global BLAS root node
    // y = BLAS node count
    // z = first triangle
    // w = triangle count
    int4 metadata;
};

public struct PackedTransform {
    float4x4 objectToWorld;
    float4x4 worldToObject;
};

public struct RenderConstants {
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
