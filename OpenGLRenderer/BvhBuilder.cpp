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

    const float PADDING = 0.0001f;

    struct Bounds {
        glm::vec3 Min{std::numeric_limits<float>::max()};
        glm::vec3 Max{std::numeric_limits<float>::lowest()};

        void Expand(const glm::vec3 &point) {
            Min = glm::min(Min, point);
            Max = glm::max(Max, point);
        }

        void Expand(const Bounds &bounds) {
            Expand(bounds.Min);
            Expand(bounds.Max);
        }
    };

    struct PrimitiveReference {
        GpuPrimitiveType Type = GpuPrimitiveType::Sphere;
        std::uint32_t PrimitiveIndex = 0;
        Bounds BoundingBox;
        glm::vec3 Centroid{0.0, 0.0, 0.0};
    };

    Bounds GetSphereBounds(const SceneSphere &sphere) {
        const glm::vec3 radius{
            std::abs(sphere.radius)
        };
        Bounds bounds;
        bounds.Min = sphere.center - radius;
        bounds.Max = sphere.center + radius;
        return bounds;
    }

    Bounds GetQuadBounds(const SceneQuad &quad) {
        Bounds bounds;
        const glm::vec3 Q = quad.Q;
        const glm::vec3 u = quad.Q + quad.u;
        const glm::vec3 v = quad.Q + quad.v;
        const glm::vec3 w = quad.Q + quad.u  + quad.v;

        bounds.Expand(Q);
        bounds.Expand(u);
        bounds.Expand(v);
        bounds.Expand(w);

        bounds.Min -= glm::vec3(PADDING);
        bounds.Max += glm::vec3(PADDING);
        return bounds;
    }

    Bounds GetTriBounds(const SceneTri &tri) {
        Bounds bounds;
        bounds.Expand(tri.Q);
        bounds.Expand(tri.U);
        bounds.Expand(tri.V);

        bounds.Min -= glm::vec3(PADDING);
        bounds.Max += glm::vec3(PADDING);
        return bounds;
    }

    std::uint32_t BuildBvhNode(std::vector<PrimitiveReference> &references,
    std::vector<GpuBvhNode> &nodes,std::uint32_t begin,
    std::uint32_t end,std::uint32_t leafSize) {
    const std::uint32_t nodeIndex = static_cast<std::uint32_t>(nodes.size());
    nodes.emplace_back();

    Bounds nodeBounds;
    Bounds centroidBounds;

    for (std::uint32_t index = begin; index < end; ++index) {
        nodeBounds.Expand(references[index].BoundingBox);
        centroidBounds.Expand(references[index].Centroid);
    }

    const std::uint32_t primitiveCount = end - begin;

    const glm::vec3 centroidExtent = centroidBounds.Max - centroidBounds.Min;

    const float largestCentroidExtent = glm::max(centroidExtent.x,
                glm::max(centroidExtent.y, centroidExtent.z));

    if (primitiveCount <= leafSize || largestCentroidExtent < 1e-6f) {
        GpuBvhNode &node =nodes[nodeIndex];
        node.BoundsMin = glm::vec4(nodeBounds.Min,0.0f);
        node.BoundsMax = glm::vec4(nodeBounds.Max,0.0f);
        node.Metadata = glm::ivec4(static_cast<int>(begin),
            static_cast<int>(primitiveCount),1,0);
        return nodeIndex;
    }

    int splitAxis = 0;

    if (centroidExtent.y > centroidExtent.x && centroidExtent.y >= centroidExtent.z) {
        splitAxis = 1;
    } else if (centroidExtent.z > centroidExtent.x && centroidExtent.z > centroidExtent.y) {
        splitAxis = 2;
    }

    const std::uint32_t middle = begin + primitiveCount / 2;

    std::nth_element(references.begin() + begin, references.begin() + middle,
        references.begin() + end,
        [splitAxis](const PrimitiveReference &left, const PrimitiveReference &right) {
            return left.Centroid[splitAxis] < right.Centroid[splitAxis];
        });

    const std::uint32_t leftChild = BuildBvhNode(references,nodes, begin,middle, leafSize);
    const std::uint32_t rightChild = BuildBvhNode(references,nodes,middle,end, leafSize);

    GpuBvhNode &node = nodes[nodeIndex];
    node.BoundsMin = glm::vec4(nodeBounds.Min,0.0f);
    node.BoundsMax = glm::vec4(nodeBounds.Max,0.0f);
    node.Metadata = glm::ivec4(static_cast<int>(leftChild),static_cast<int>(rightChild),0,0);\
    return nodeIndex;
}
}


BvhBuildResult BvhBuilder::Build(const Scene &scene, std::uint32_t leafSize) {
    BvhBuildResult result;
    std::vector<PrimitiveReference> references;
    references.reserve(scene.GetSpheres().size() + scene.GetQuads().size() + scene.GetTris().size());

    const auto &sceneSpheres = scene.GetSpheres();
    for (std::uint32_t sphereIndex = 0; sphereIndex < sceneSpheres.size(); ++sphereIndex) {
        const SceneSphere& sphere = sceneSpheres[sphereIndex];
        GpuSphere gpuSphere;
        gpuSphere.CenterRadius = glm::vec4(sphere.center, sphere.radius);
        gpuSphere.Metadata = glm::ivec4(sphere.material, 0, 0, 0);
        result.Spheres.push_back(gpuSphere);


        PrimitiveReference reference;
        reference.Type = GpuPrimitiveType::Sphere;
        reference.PrimitiveIndex = sphereIndex;
        reference.BoundingBox = GetSphereBounds(sphere);
        reference.Centroid = sphere.center;
        references.push_back(reference);
    }

    const auto &sceneQuads = scene.GetQuads();
    for (std::uint32_t quadIndex = 0; quadIndex < sceneSpheres.size(); ++quadIndex) {
        const SceneQuad& quad = sceneQuads[quadIndex];
        GpuQuad gpuQuad;
        gpuQuad.Q = glm::vec4(quad.Q, 0);
        gpuQuad.u = glm::vec4(quad.u, 0);
        gpuQuad.v = glm::vec4(quad.v, 0);
        gpuQuad.Metadata = glm::vec4(quad.material, 0, 0, 0);
        result.Quads.push_back(gpuQuad);


        PrimitiveReference reference;
        const Bounds bounds = GetQuadBounds(quad);
        reference.Type = GpuPrimitiveType::Quad;
        reference.PrimitiveIndex = quadIndex;
        reference.BoundingBox = bounds;
        reference.Centroid = (bounds.Min + bounds.Max) * 0.5f;
        references.push_back(reference);
    }

    const auto &sceneTris = scene.GetTris();
    for (std::uint32_t triIndex = 0; triIndex < sceneTris.size(); ++triIndex) {
        const SceneTri& tri = sceneTris[triIndex];
        GpuTri gpuTri;
        gpuTri.Q = glm::vec4(tri.Q, 0);
        gpuTri.U = glm::vec4(tri.U, 0);
        gpuTri.V = glm::vec4(tri.V, 0);
        gpuTri.Metadata = glm::vec4(tri.material, 0, 0, 0);
        result.Tris.push_back(gpuTri);

        PrimitiveReference reference;
        const Bounds bounds = GetTriBounds(tri);
        reference.Type = GpuPrimitiveType::Triangle;
        reference.PrimitiveIndex = triIndex;
        reference.BoundingBox = bounds;
        reference.Centroid = (tri.Q + tri.U + tri.V) / 3.0f;
        references.push_back(reference);
    }




    if (references.empty()) {
        return result;
    }
    leafSize = std::max(leafSize,std::uint32_t{1});
    result.Nodes.reserve(references.size() * 2);

    BuildBvhNode(references, result.Nodes, 0, static_cast<std::uint32_t>(references.size()), leafSize);

    result.PrimitiveRefs.reserve(references.size());
    for (const PrimitiveReference& reference :references) {
        GpuPrimitiveRef gpuReference;
        gpuReference.Metadata = glm::ivec4(static_cast<int>(reference.Type),
            static_cast<int>(reference.PrimitiveIndex),0,0);
        result.PrimitiveRefs.push_back(gpuReference);
    }
    return result;
}