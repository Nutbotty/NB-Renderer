//
// Created by Nutbotty on 7/23/2026.
//

#include "BvhBuilder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include "../Scene/Scene.h"

namespace {
    constexpr float PADDING = 0.0001f;
    constexpr float EPSILON = 1e-8f;
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
        std::uint32_t TransformIndex = 0;
        Bounds BoundingBox;
        glm::vec3 Centroid{0.0, 0.0, 0.0};
    };


    void PadDegenerateAxes(Bounds& bounds) {
        for (int axis = 0; axis < 3; ++axis) {
            const float axisExtent = bounds.Max[axis] - bounds.Min[axis];
            if (axisExtent < PADDING) {
                const float axisPadding = 0.5f * (PADDING - axisExtent);
                bounds.Min[axis] -= axisPadding;
                bounds.Max[axis] += axisPadding;
            }
        }
    }

    Bounds GetSphereBounds(const SceneSphere &sphere) {
        const glm::vec3 radius{std::abs(sphere.radius)};
        Bounds bounds;
        bounds.Min = sphere.center - radius;
        bounds.Max = sphere.center + radius;
        return bounds;
    }

    Bounds GetQuadBounds(const SceneQuad &quad) {
        Bounds bounds;
        bounds.Expand(quad.Q);
        bounds.Expand(quad.Q + quad.u);
        bounds.Expand(quad.Q + quad.v);
        bounds.Expand(quad.Q + quad.u  + quad.v);
        PadDegenerateAxes(bounds);
        return bounds;
    }

    Bounds GetTriBounds(const SceneTri &tri) {
        Bounds bounds;
        bounds.Expand(tri.Q);
        bounds.Expand(tri.Q + tri.U);
        bounds.Expand(tri.Q + tri.V);
        PadDegenerateAxes(bounds);
        return bounds;
    }

    GpuPrimitiveType ToGpuPrimitiveType(ScenePrimitiveType type)
    {
        switch (type)
        {
            case ScenePrimitiveType::Sphere:
                return GpuPrimitiveType::Sphere;

            case ScenePrimitiveType::Quad:
                return GpuPrimitiveType::Quad;

            case ScenePrimitiveType::Triangle:
                return GpuPrimitiveType::Triangle;
        }
    }

    Bounds GetPrimitiveLocalBounds(const Scene& scene, const ScenePrimitiveRecord& primitive) {
        switch (primitive.type) {
            case ScenePrimitiveType::Sphere: {
                return GetSphereBounds(scene.GetSpheres().at(primitive.index));
            }
            case ScenePrimitiveType::Quad: {
                return GetQuadBounds(scene.GetQuads().at(primitive.index));
            }
            case ScenePrimitiveType::Triangle: {
                return GetTriBounds(scene.GetTris().at(primitive.index));
            }
        }
    }

    glm::vec3 GetPrimitiveLocalCentroid(const Scene& scene,const ScenePrimitiveRecord& primitive) {
        switch (primitive.type) {
            case ScenePrimitiveType::Sphere: {
                const SceneSphere& sphere = scene.GetSpheres().at(primitive.index);
                return sphere.center;
            }
            case ScenePrimitiveType::Quad: {
                const SceneQuad& quad = scene.GetQuads().at(primitive.index);
                return quad.Q + 0.5f * quad.u + 0.5f * quad.v;
            }
            case ScenePrimitiveType::Triangle: {
                const SceneTri& tri = scene.GetTris().at(primitive.index);
                return (tri.Q + tri.U + tri.V) / 3.0f;
            }
        }
    }

    glm::vec3 TransformPoint(const glm::mat4& transform, const glm::vec3& point) {
        const glm::vec4 transformed = transform * glm::vec4(point,1.0f);
        return glm::vec3(transformed);
    }

    Bounds TransformBounds(const Bounds& localBounds,const glm::mat4& objectToWorld) {
        Bounds worldBounds;
        for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {
            const glm::vec3 localCorner(
                (cornerIndex & 1) ? localBounds.Max.x : localBounds.Min.x,
                (cornerIndex & 2) ? localBounds.Max.y : localBounds.Min.y,
                (cornerIndex & 4) ? localBounds.Max.z: localBounds.Min.z);
            worldBounds.Expand(TransformPoint(objectToWorld, localCorner));
        }
        PadDegenerateAxes(worldBounds);
        return worldBounds;
    }

    std::uint32_t BuildBvhNode(std::vector<PrimitiveReference> &references,
    std::vector<GpuBvhNode> &nodes,
    std::uint32_t begin,
    std::uint32_t end,
    std::uint32_t leafSize) {
    const auto nodeIndex = static_cast<std::uint32_t>(nodes.size());
    nodes.emplace_back();

    Bounds nodeBounds;
    Bounds centroidBounds;

    for (std::uint32_t index = begin; index < end; ++index) {
        nodeBounds.Expand(references[index].BoundingBox);
        centroidBounds.Expand(references[index].Centroid);
    }

    const std::uint32_t primitiveCount = end - begin;
    const glm::vec3 centroidExtent = centroidBounds.Max - centroidBounds.Min;
    const float largestCentroidExtent = glm::max(centroidExtent.x,glm::max(centroidExtent.y, centroidExtent.z));

    if (primitiveCount <= leafSize || largestCentroidExtent < EPSILON) {
        GpuBvhNode& node = nodes[nodeIndex];
        node.BoundsMin = glm::vec4(nodeBounds.Min,0.0f);
        node.BoundsMax = glm::vec4(nodeBounds.Max,0.0f);
        node.Metadata = glm::ivec4(static_cast<int>(begin), static_cast<int>(primitiveCount),1,0);
        return nodeIndex;
    }

    int splitAxis = 0;
    if (centroidExtent.y > centroidExtent.x && centroidExtent.y >= centroidExtent.z) {
        splitAxis = 1;
    } else if (centroidExtent.z > centroidExtent.x && centroidExtent.z > centroidExtent.y) {
        splitAxis = 2;
    }

    const std::uint32_t middle = begin + primitiveCount / 2;

    std::nth_element(references.begin() + begin, references.begin() + middle, references.begin() + end,
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
    const auto& sceneSpheres = scene.GetSpheres();
    const auto& sceneQuads = scene.GetQuads();
    const auto& sceneTris = scene.GetTris();
    const auto& sceneInstances = scene.GetInstances();

    result.Spheres.reserve(sceneSpheres.size());
    result.Quads.reserve(sceneQuads.size());
    result.Tris.reserve(sceneTris.size());

    for (const SceneSphere& sphere : sceneSpheres) {
        GpuSphere gpuSphere;
        gpuSphere.CenterRadius = glm::vec4(sphere.center, sphere.radius);
        gpuSphere.Metadata = glm::ivec4(sphere.material, 0, 0, 0);
        result.Spheres.push_back(gpuSphere);
    }

    for (const SceneQuad& quad : sceneQuads) {
        GpuQuad gpuQuad;
        gpuQuad.Q = glm::vec4(quad.Q, 0);
        gpuQuad.u = glm::vec4(quad.u, 0);
        gpuQuad.v = glm::vec4(quad.v, 0);
        gpuQuad.Metadata = glm::vec4(quad.material, 0, 0, 0);
        result.Quads.push_back(gpuQuad);
    }

    for (const SceneTri& tri : sceneTris) {
        GpuTri gpuTri;
        gpuTri.Q = glm::vec4(tri.Q, 0);
        gpuTri.U = glm::vec4(tri.Q + tri.U, 0);
        gpuTri.V = glm::vec4(tri.Q + tri.V, 0);
        gpuTri.Metadata = glm::vec4(tri.material, 0, 0, 0);
        result.Tris.push_back(gpuTri);
    }

    GpuTransform identityTransform;
    identityTransform.ObjectToWorld = glm::mat4(1.0f);
    identityTransform.WorldToObject = glm::mat4(1.0f);
    result.Transforms.reserve(sceneInstances.size() + 1);
    result.Transforms.push_back(identityTransform);

    std::vector<PrimitiveReference> references;
    references.reserve(sceneSpheres.size() + sceneQuads.size() + sceneTris.size() + sceneInstances.size());

    auto addReference = [&](const ScenePrimitiveRecord& primitive,
        std::uint32_t transformIndex, const glm::mat4& objectToWorld) {
            const Bounds localBounds = GetPrimitiveLocalBounds(scene, primitive);
            const glm::vec3 localCentroid = GetPrimitiveLocalCentroid(scene, primitive);

            PrimitiveReference reference;
            reference.Type = ToGpuPrimitiveType(primitive.type);
            reference.PrimitiveIndex = primitive.index;
            reference.TransformIndex = transformIndex;
            reference.BoundingBox = TransformBounds(localBounds, objectToWorld);
            reference.Centroid = TransformPoint(objectToWorld, localCentroid);
            references.push_back(reference);
    };

    const glm::mat4 identity{1.0f};
    for (std::uint32_t sphereIndex = 0; sphereIndex < sceneSpheres.size(); ++sphereIndex) {
        addReference(ScenePrimitiveRecord{
                ScenePrimitiveType::Sphere,sphereIndex},0,identity);
    }
    for (std::uint32_t quadIndex = 0; quadIndex < sceneQuads.size(); ++quadIndex) {
        addReference(ScenePrimitiveRecord{
                ScenePrimitiveType::Quad,quadIndex},0,identity);
    }
    for (std::uint32_t triIndex = 0; triIndex < sceneTris.size(); ++triIndex) {
        addReference(ScenePrimitiveRecord{
            ScenePrimitiveType::Triangle,triIndex},0, identity);
    }
    for (const SceneInstance& instance : sceneInstances) {
        const std::uint32_t transformIndex = static_cast<std::uint32_t>(result.Transforms.size());
        GpuTransform gpuTransform;
        gpuTransform.ObjectToWorld = instance.objectToWorld;
        gpuTransform.WorldToObject = glm::inverse(instance.objectToWorld);
        result.Transforms.push_back(gpuTransform);
        addReference(instance.primitive, transformIndex, instance.objectToWorld);
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