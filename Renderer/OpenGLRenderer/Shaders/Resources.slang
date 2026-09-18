#language slang 2026
module Resources;

import GpuTypes;

// Resource binding policy:
//   0      output image
//   1-12   scene buffers
//   13-15  sampled textures
//   16     render constants
[[vk::binding(0, 0)]] public RWTexture2D<float4> outputImage : register(u0, space0);

[[vk::binding(1, 0)]] public StructuredBuffer<PackedMaterial, Std430DataLayout> materialBuffer : register(t1, space0);

[[vk::binding(2, 0)]] public StructuredBuffer<PackedSphere, Std430DataLayout> sphereBuffer : register(t2, space0);

[[vk::binding(3, 0)]] public StructuredBuffer<PackedBvhNode, Std430DataLayout> tlasNodeBuffer : register(t3, space0);

[[vk::binding(4, 0)]] public StructuredBuffer<PackedQuad, Std430DataLayout> quadBuffer : register(t4, space0);

[[vk::binding(5, 0)]] public StructuredBuffer<PackedTriangle, Std430DataLayout> triangleBuffer : register(t5, space0);

[[vk::binding(6, 0)]] public StructuredBuffer<PackedPrimitiveRef, Std430DataLayout> primitiveRefBuffer : register(t6, space0);

[[vk::binding(7, 0)]] public StructuredBuffer<PackedTransform, Std430DataLayout> transformBuffer : register(t7, space0);

[[vk::binding(8, 0)]] public StructuredBuffer<PackedMeshVertex, Std430DataLayout> meshVertexBuffer : register(t8, space0);

[[vk::binding(9, 0)]] public StructuredBuffer<PackedMeshTriangle, Std430DataLayout> meshTriangleBuffer : register(t9, space0);

[[vk::binding(10, 0)]] public StructuredBuffer<PackedMesh, Std430DataLayout> meshBuffer : register(t10, space0);

[[vk::binding(11, 0)]] public StructuredBuffer<PackedBvhNode, Std430DataLayout> blasNodeBuffer : register(t11, space0);

[[vk::binding(12, 0)]] public StructuredBuffer<int, Std430DataLayout> instanceMaterialBuffer : register(t12, space0);

[[vk::binding(13, 0)]] public Sampler2D<float4> uEnvironmentMap;

[[vk::binding(14, 0)]] public Sampler2DArray<float4> uBaseColorTextures;

[[vk::binding(15, 0)]] public Sampler2DArray<float4> uMetalRoughTextures;

[[vk::binding(16, 0)]] public ConstantBuffer<RenderConstants> gRender : register(b16, space0);
