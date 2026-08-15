//
// Created by Nutbotty on 7/23/2026.
//

#ifndef NB_RENDERER_BVHBUILDER_H
#define NB_RENDERER_BVHBUILDER_H

#include <cstdint>
#include <vector>
#include "../external/glm/glm/glm.hpp"

enum class GpuPrimitiveType : std::int32_t {
    Sphere = 0,
    Quad = 1,
    Triangle = 2,
    Mesh = 3
};

struct alignas(16) GpuBvhNode {
    glm::vec4 BoundsMin{0.0f};
    glm::vec4 BoundsMax{0.0f};
    glm::ivec4 Metadata{0};
};

struct alignas(16) GpuMaterial {
    //xyz = color w = opacity
    glm::vec4 BaseColor{1.0};
    // x = metal y = rough z = ior w = transmission
    glm::vec4 Surface{0.0f};
    // xyz =color, w = strength
    glm::vec4 Emission{0.0f};
    glm::ivec4 Metadata{0};
    // x = basecolor y = metalrough z = normal, w = emissive (-1 none)
    glm::ivec4 TextureIndices{-1};
};

struct alignas(16) GpuSphere {
    glm::vec4 CenterRadius{0.0f};
    // x = material index
    glm::ivec4 Metadata{0};
};

struct alignas(16) GpuQuad {
    glm::vec4 Q{0.0f};
    glm::vec4 u{0.0f};
    glm::vec4 v{0.0f};
    // x = material index
    glm::ivec4 Metadata{0};
};

struct alignas(16) GpuTri {
    glm::vec4 Q{0.0f};
    glm::vec4 U{0.0f};
    glm::vec4 V{0.0f};
    // x = material index
    glm::ivec4 Metadata{0};
};

struct alignas(16) GpuMeshVertex {
    glm::vec4 Position{0.0f};
    glm::vec4 Normal{0.0f};
    // xy = TEXCOORD_0
    glm::vec4 TexCoord;
};
struct alignas(16) GpuMeshTriangle {
    //xyz tri indices, w = material
    glm::ivec4 MetaData{0};
};
struct alignas(16) GpuMesh {
    glm::vec4 BoundsMin{0.0f};
    glm::vec4 BoundsMax{0.0f};
    // x = BLAS root, y = BLAS node count
    // z root tri, w = tri count
    glm::ivec4 Metadata{0};
};


struct alignas(16) GpuTransform {
    glm::mat4 ObjectToWorld{1.0f};
    glm::mat4 WorldToObject{1.0f};
};

// x = primitive type, y = index of primitive buffer (per shape), z = transform buffer index
struct alignas(16) GpuPrimitiveRef {
    glm::ivec4 Metadata{0};
};

struct BvhBuildResult {
    std::vector<GpuSphere> Spheres;
    std::vector<GpuQuad> Quads;
    std::vector<GpuTri> Tris;
    std::vector<GpuMeshVertex> MeshVertices;
    std::vector<GpuMeshTriangle> MeshTriangles;
    std::vector<GpuMesh> Meshes;


    std::vector<GpuTransform> Transforms;
    std::vector<GpuPrimitiveRef> PrimitiveRefs;

    std::vector<GpuBvhNode> TlasNodes;
    std::vector<GpuBvhNode> BlasNodes;
};

class Scene;

class BvhBuilder {
public:
    [[nodiscard]] static BvhBuildResult Build(
        const Scene &scene, std::uint32_t tlasLeafSize = 8, std::uint32_t blasLeafSize = 4);
};

#endif //NB_RENDERER_BVHBUILDER_H
