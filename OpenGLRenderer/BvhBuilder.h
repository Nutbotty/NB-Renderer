//
// Created by Nutbotty on 7/23/2026.
//

#ifndef NB_RENDERER_BVHBUILDER_H
#define NB_RENDERER_BVHBUILDER_H

#include <cstdint>
#include <vector>
#include "../external/glm/glm/glm.hpp"

struct alignas(16) GpuBvhNode {
    glm::vec4 BoundsMin{0.0f};
    glm::vec4 BoundsMax{0.0f};
    glm::ivec4 Metadata{0};
};

struct alignas(16) GpuMaterial {
    glm::vec4 AlbedoFuzz{0.0f};
    glm::vec4 Optical{0.0f};
    glm::ivec4 Metadata{0};
};

struct alignas(16) GpuSphere {
    glm::vec4 CenterRadius{0.0f};
    glm::ivec4 Metadata{0};
};

struct BvhBuildResult {
    std::vector<GpuSphere> Spheres;
    std::vector<GpuBvhNode> Nodes;
};

class Scene;

class BvhBuilder
{
public:
    [[nodiscard]] static BvhBuildResult Build(
        const Scene& scene, std::uint32_t leafSize = 8);
};

#endif //NB_RENDERER_BVHBUILDER_H
