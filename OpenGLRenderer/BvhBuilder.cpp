//
// Created by Nutbotty on 7/23/2026.
//

#include "BvhBuilder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "../Scene/Scene.h"

namespace {
    struct Bounds {
        glm::vec3 Min{
            std::numeric_limits<float>::max()
        };

        glm::vec3 Max{
            std::numeric_limits<float>::lowest()
        };

        void Expand(const glm::vec3 &point) {
            Min = glm::min(Min, point);
            Max = glm::max(Max, point);
        }

        void Expand(const Bounds &bounds) {
            Expand(bounds.Min);
            Expand(bounds.Max);
        }
    };

    struct SphereReference {
        std::uint32_t SceneSphereIndex = 0;

        Bounds BoundingBox;

        glm::vec3 Centroid{0.0f};
    };
    Bounds GetSphereBounds(const SceneSphere &sphere) {
        const glm::vec3 radius{
            std::abs(sphere.radius)
        };

        Bounds bounds;

        bounds.Min =
                sphere.center - radius;

        bounds.Max =
                sphere.center + radius;

        return bounds;
    }

    std::uint32_t BuildBvhNode(
    std::vector<SphereReference> &references,
    std::vector<GpuBvhNode> &nodes,
    std::uint32_t begin,
    std::uint32_t end,
    std::uint32_t leafSize
) {
    const std::uint32_t nodeIndex =
            static_cast<std::uint32_t>(
                nodes.size()
            );

    /*
     * Reserve this node before recursively creating its children.
     * This guarantees the first node is the root at index zero.
     */
    nodes.emplace_back();

    Bounds nodeBounds;
    Bounds centroidBounds;

    for (
        std::uint32_t index = begin;
        index < end;
        ++index
    ) {
        nodeBounds.Expand(
            references[index].BoundingBox
        );

        centroidBounds.Expand(
            references[index].Centroid
        );
    }

    const std::uint32_t sphereCount =
            end - begin;

    const glm::vec3 centroidExtent =
            centroidBounds.Max
            - centroidBounds.Min;

    const float largestCentroidExtent =
            glm::max(
                centroidExtent.x,
                glm::max(
                    centroidExtent.y,
                    centroidExtent.z
                )
            );

    /*
     * Make a leaf if it is sufficiently small or all centroids
     * occupy essentially the same location.
     */
    if (
        sphereCount <= leafSize ||
        largestCentroidExtent < 1e-6f
    ) {
        GpuBvhNode &node =
                nodes[nodeIndex];

        node.BoundsMin =
                glm::vec4(
                    nodeBounds.Min,
                    0.0f
                );

        node.BoundsMax =
                glm::vec4(
                    nodeBounds.Max,
                    0.0f
                );

        node.Metadata =
                glm::ivec4(
                    static_cast<int>(begin),
                    static_cast<int>(sphereCount),
                    1,
                    0
                );

        return nodeIndex;
    }

    int splitAxis = 0;

    if (
        centroidExtent.y > centroidExtent.x &&
        centroidExtent.y >= centroidExtent.z
    ) {
        splitAxis = 1;
    } else if (
        centroidExtent.z > centroidExtent.x &&
        centroidExtent.z > centroidExtent.y
    ) {
        splitAxis = 2;
    }

    const std::uint32_t middle =
            begin + sphereCount / 2;

    std::nth_element(
        references.begin() + begin,
        references.begin() + middle,
        references.begin() + end,
        [splitAxis](
    const SphereReference &left,
    const SphereReference &right
) {
            return left.Centroid[splitAxis]
                   < right.Centroid[splitAxis];
        }
    );

    const std::uint32_t leftChild =
            BuildBvhNode(
                references,
                nodes,
                begin,
                middle,
                leafSize
            );

    const std::uint32_t rightChild =
            BuildBvhNode(
                references,
                nodes,
                middle,
                end,
                leafSize
            );

    GpuBvhNode &node =
            nodes[nodeIndex];

    node.BoundsMin =
            glm::vec4(
                nodeBounds.Min,
                0.0f
            );

    node.BoundsMax =
            glm::vec4(
                nodeBounds.Max,
                0.0f
            );

    node.Metadata =
            glm::ivec4(
                static_cast<int>(leftChild),
                static_cast<int>(rightChild),
                0,
                0
            );

    return nodeIndex;
}
}


BvhBuildResult BvhBuilder::Build(const Scene &scene, std::uint32_t leafSize) {
    BvhBuildResult result;

    const auto &sceneSpheres = scene.GetSpheres();

    std::vector<SphereReference> references;

    references.reserve(sceneSpheres.size());

    for (std::uint32_t sphereIndex = 0; sphereIndex < sceneSpheres.size(); ++sphereIndex) {
        const SceneSphere &sphere = sceneSpheres[sphereIndex];
        SphereReference reference;
        reference.SceneSphereIndex = sphereIndex;
        reference.BoundingBox = GetSphereBounds(sphere);
        reference.Centroid = sphere.center;
        references.push_back(reference);
    }

    if (references.empty()) {
        return result;
    }
    result.Nodes.reserve(references.size() * 2);

    BuildBvhNode(references, result.Nodes, 0, static_cast<std::uint32_t>(references.size()), leafSize);

    result.Spheres.reserve(references.size());

    for (const SphereReference &reference : references) {
        const SceneSphere &sphere = sceneSpheres[reference.SceneSphereIndex];

        GpuSphere gpuSphere;
        gpuSphere.CenterRadius = glm::vec4(sphere.center,sphere.radius);
        gpuSphere.Metadata = glm::ivec4(static_cast<int>(sphere.material),0,0,0);
        result.Spheres.push_back(gpuSphere);
    }
    return result;
}