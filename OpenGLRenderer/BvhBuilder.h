//
// Created by Nutbotty on 7/23/2026.
//

#ifndef NB_RENDERER_BVHBUILDER_H
#define NB_RENDERER_BVHBUILDER_H

#include <cstdint>
#include <vector>
#include "../external/glm/glm/glm.hpp"

enum class GpuPrimitiveType : std::int32_t
{
    Sphere   = 0,
    Quad     = 1,
    Triangle = 2
};
struct alignas(16) GpuBvhNode {
    glm::vec4 BoundsMin{0.0f};
    glm::vec4 BoundsMax{0.0f};
    glm::ivec4 Metadata{0};
};

struct alignas(16) GpuMaterial {
    glm::vec4 AlbedoFuzz{0.0f};
    // x = ior
    glm::vec4 Optical{0.0f};
    // xyz =color, w = strength
    glm::vec4 Emission{0.0f};
    glm::ivec4 Metadata{0};
};

struct alignas(16) GpuSphere {
    glm::vec4 CenterRadius{0.0f};
    glm::ivec4 Metadata{0};
};

struct alignas(16) GpuQuad {
    glm::vec4 Q{0.0f};
    glm::vec4 u{0.0f};
    glm::vec4 v{0.0f};
    glm::ivec4 Metadata{0};
};

struct alignas(16) GpuTri {
    glm::vec4 Q{0.0f};
    glm::vec4 U{0.0f};
    glm::vec4 V{0.0f};
    glm::ivec4 Metadata{0};
};

// x = primitive type, y = index of buffer
struct alignas(16) GpuPrimitiveRef
{
    glm::ivec4 Metadata{0};
};

struct BvhBuildResult {
    std::vector<GpuSphere> Spheres;
    std::vector<GpuQuad> Quads;
    std::vector<GpuTri> Tris;
    std::vector<GpuPrimitiveRef> PrimitiveRefs;
    std::vector<GpuBvhNode> Nodes;
};

class Scene;

class BvhBuilder {
public:
    [[nodiscard]] static BvhBuildResult Build(
        const Scene& scene, std::uint32_t leafSize = 8);
};

#endif //NB_RENDERER_BVHBUILDER_H
