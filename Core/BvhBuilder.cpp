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
#include <cassert>

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
        std::int32_t MaterialOffset = -1;
        std::uint32_t MeshInstanceIndex = std::numeric_limits<std::uint32_t>::max();
        Bounds BoundingBox;
        glm::vec3 Centroid{0.0, 0.0, 0.0};
    };
    struct MeshTriangleReference {
        GpuMeshTriangle Triangle;
        Bounds BoundingBox;
        glm::vec3 Centroid{0.0f};
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
                return tri.Q + (tri.U + tri.V) / 3.0f;
            }
        }
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

    Bounds GetMeshTriangeBounds(const SceneMesh& mesh, const SceneMeshTriangle& tri) {
        const glm::vec3& a = mesh.vertices[tri.index0].position;
        const glm::vec3& b = mesh.vertices[tri.index1].position;
        const glm::vec3& c = mesh.vertices[tri.index2].position;
        Bounds bounds;
        bounds.Expand(a);
        bounds.Expand(b);
        bounds.Expand(c);
        PadDegenerateAxes(bounds);
        return bounds;
    }
    glm::vec3 GetMeshTriangleCentroid(const SceneMesh& mesh, const SceneMeshTriangle& triangle) {
        const glm::vec3& a = mesh.vertices[triangle.index0].position;
        const glm::vec3& b = mesh.vertices[triangle.index1].position;
        const glm::vec3& c = mesh.vertices[triangle.index2].position;
        return (a + b + c) / 3.0f;
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

    std::int32_t GetMeshMaterialSlot(const SceneMesh& mesh, MaterialId material) {
        const auto mesh_iterator = std::find(mesh.materials.begin(), mesh.materials.end(), material);
        return static_cast<std::int32_t>(std::distance(mesh.materials.begin(), mesh_iterator));
    }

    std::uint32_t BuildBvhNode(std::vector<PrimitiveReference> &references, std::vector<GpuBvhNode> &nodes,
        std::uint32_t begin, std::uint32_t end, std::uint32_t leafSize) {
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

        GpuBvhNode& node = nodes[nodeIndex];
        node.BoundsMin = glm::vec4(nodeBounds.Min,0.0f);
        node.BoundsMax = glm::vec4(nodeBounds.Max,0.0f);
        node.Metadata = glm::ivec4(static_cast<int>(leftChild),static_cast<int>(rightChild),0,0);\
        return nodeIndex;
    }
    std::uint32_t BuildMeshBvhNode(std::vector<MeshTriangleReference>& references, std::vector<GpuBvhNode>& nodes,
        std::uint32_t begin,std::uint32_t end, std::uint32_t leafSize, std::uint32_t firstOutputTriangle) {
        const std::uint32_t nodeIndex = static_cast<std::uint32_t>(nodes.size());
        nodes.emplace_back();

        Bounds nodeBounds;
        Bounds centroidBounds;

        for (std::uint32_t index = begin; index < end; ++index) {
            nodeBounds.Expand(references[index].BoundingBox);
            centroidBounds.Expand(references[index].Centroid);
        }

        const uint32_t triCount = end - begin;
        const glm::vec3 centroidExtent = centroidBounds.Max - centroidBounds.Min;
        const float largestExtent = glm::max(centroidExtent.x, glm::max(centroidExtent.y, centroidExtent.z));
        // BLAS leaf
        if (triCount <= leafSize || largestExtent < EPSILON) {
            GpuBvhNode& node = nodes[nodeIndex];
            node.BoundsMin = glm::vec4(nodeBounds.Min,0.0f);
            node.BoundsMax = glm::vec4(nodeBounds.Max,0.0f);
            node.Metadata = glm::ivec4(static_cast<int>(firstOutputTriangle + begin),
                static_cast<int>(triCount), 1, 0);
            return nodeIndex;
        }

        int splitAxis = 0;
        if (centroidExtent.y > centroidExtent.x && centroidExtent.y >= centroidExtent.z) {
            splitAxis = 1;
        } else if (centroidExtent.z > centroidExtent.x && centroidExtent.z > centroidExtent.y) {
            splitAxis = 2;
        }
        const std::uint32_t middle = begin + triCount / 2;

        std::nth_element(references.begin() + begin, references.begin() + middle, references.begin() + end,
            [splitAxis](const MeshTriangleReference &left, const MeshTriangleReference &right) {
                return left.Centroid[splitAxis] < right.Centroid[splitAxis];
            });

        const std::uint32_t leftChild = BuildMeshBvhNode(
            references,nodes, begin,middle, leafSize, firstOutputTriangle);
        const std::uint32_t rightChild = BuildMeshBvhNode(
            references,nodes,middle,end, leafSize, firstOutputTriangle);
        GpuBvhNode& node = nodes[nodeIndex];
        node.BoundsMin = glm::vec4(nodeBounds.Min, 0.0f);
        node.BoundsMax = glm::vec4(nodeBounds.Max, 0.0f);
        node.Metadata = glm::ivec4(static_cast<int>(leftChild), static_cast<int>(rightChild), 0, 0);
        return nodeIndex;
    }

    void refitTLAS(BvhBuildResult& result) {
        if (result.TlasNodes.empty()) return;

        for (std::size_t i = result.TlasNodes.size(); i > 0; i--) {
            GpuBvhNode& node = result.TlasNodes[i];
            const bool isLeaf = node.Metadata.z == 1;
            if (isLeaf) {
                const auto first = static_cast<std::uint32_t>(node.Metadata.x);
                const auto count = static_cast<std::uint32_t>(node.Metadata.y);

                Bounds bounds;
                for (std::uint32_t j = 0; j < count; ++j) {
                    const BvhPrimitiveBounds& primitiveBounds = result.PrimitiveBounds[first + j];
                    bounds.Expand(primitiveBounds.Min);
                    bounds.Expand(primitiveBounds.Max);
                }
                node.BoundsMin = glm::vec4(bounds.Min, 0.0f);
                node.BoundsMax = glm::vec4(bounds.Max, 0.0f);
            } else {
                const auto left = static_cast<std::uint32_t>(node.Metadata.x);
                const auto right = static_cast<std::uint32_t>(node.Metadata.y);
                node.BoundsMin = glm::min(result.TlasNodes[left].BoundsMin, result.TlasNodes[right].BoundsMin);
                node.BoundsMax = glm::max(result.TlasNodes[left].BoundsMax, result.TlasNodes[right].BoundsMax);
            }
        }
    }
}

BvhBuildResult BvhBuilder::Build(const Scene &scene, std::uint32_t tlasLeafSize, std::uint32_t blasLeafSize) {
    BvhBuildResult result;
    const auto& sceneSpheres = scene.GetSpheres();
    const auto& sceneQuads = scene.GetQuads();
    const auto& sceneTris = scene.GetTris();
    const auto& sceneInstances = scene.GetInstances();
    const auto& sceneMeshes = scene.GetMeshes();

    result.Spheres.reserve(sceneSpheres.size());
    result.Quads.reserve(sceneQuads.size());
    result.Tris.reserve(sceneTris.size());
    result.Meshes.reserve(sceneMeshes.size());

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

    constexpr glm::mat4 identity{1.0f};
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
        const auto transformIndex = static_cast<std::uint32_t>(result.Transforms.size());
        GpuTransform gpuTransform;
        gpuTransform.ObjectToWorld = instance.objectToWorld;
        gpuTransform.WorldToObject = glm::inverse(instance.objectToWorld);
        result.Transforms.push_back(gpuTransform);
        addReference(instance.primitive, transformIndex, instance.objectToWorld);
    }
    for (std::uint32_t meshIndex = 0; meshIndex < sceneMeshes.size(); ++ meshIndex) {
        const SceneMesh& mesh = sceneMeshes[meshIndex];
        const std::uint32_t firstVertex = static_cast<std::uint32_t>(result.MeshVertices.size());

        for (const SceneMeshVertex& vertex : mesh.vertices) {
            GpuMeshVertex gpuMeshVertex;
            gpuMeshVertex.Position = glm::vec4(vertex.position, 0.0f);
            gpuMeshVertex.Normal = glm::vec4(vertex.normal, 0.0f);
            gpuMeshVertex.TexCoord = glm::vec4(vertex.texCoord, 0.0f, 0.0f);
            result.MeshVertices.push_back(gpuMeshVertex);
        }

        std::vector<MeshTriangleReference> triangleReferences;
        triangleReferences.reserve(mesh.triangles.size());
        for (const SceneMeshTriangle& tri : mesh.triangles) {
            MeshTriangleReference reference;
            const std::int32_t materialSlot = GetMeshMaterialSlot(mesh, tri.material);
            reference.Triangle.MetaData = glm::ivec4(
                static_cast<int>(firstVertex + tri.index0),
                static_cast<int>(firstVertex + tri.index1),
                static_cast<int>(firstVertex + tri.index2),
                materialSlot);
            reference.BoundingBox = GetMeshTriangeBounds(mesh, tri);
            reference.Centroid = GetMeshTriangleCentroid(mesh, tri);
            triangleReferences.push_back(reference);
        }

        const std::uint32_t firstTriangle = static_cast<std::uint32_t>(result.MeshTriangles.size());
        const std::uint32_t firstNode = static_cast<std::uint32_t>(result.BlasNodes.size());
        const std::uint32_t rootNode = BuildMeshBvhNode(triangleReferences, result.BlasNodes, 0,
                                                        static_cast<std::uint32_t>(triangleReferences.size()),
                                                        std::max(blasLeafSize, std::uint32_t{1}), firstTriangle);
        for (const MeshTriangleReference& reference : triangleReferences) {
            result.MeshTriangles.push_back(reference.Triangle);
        }
        const std::uint32_t nodeCount = static_cast<std::uint32_t>(result.BlasNodes.size()) - firstNode;

        GpuMesh gpuMesh;
        gpuMesh.BoundsMin = glm::vec4(mesh.boundsMin, 0.0f);
        gpuMesh.BoundsMax = glm::vec4(mesh.boundsMax, 0.0f);
        gpuMesh.Metadata =  glm::ivec4(static_cast<int>(rootNode), static_cast<int>(nodeCount),
            static_cast<int>(firstTriangle), static_cast<int>(triangleReferences.size()));
        result.Meshes.push_back(gpuMesh);
    }

    const auto& meshInstances = scene.GetMeshInstances();
    result.MeshInstanceMaterialOffsets.reserve(meshInstances.size());
    for (std::uint32_t meshInstanceIndex = 0; meshInstanceIndex < meshInstances.size(); ++meshInstanceIndex) {
        const SceneMeshInstance& meshInstance = meshInstances[meshInstanceIndex];
        const auto transformIndex = static_cast<std::uint32_t>(result.Transforms.size());
        GpuTransform gpuTransform;
        gpuTransform.ObjectToWorld = meshInstance.objectToWorld;
        gpuTransform.WorldToObject = glm::inverse(meshInstance.objectToWorld);
        result.Transforms.push_back(gpuTransform);
        const SceneMesh& mesh = sceneMeshes.at(meshInstance.mesh);

        const std::uint32_t materialOffset = static_cast<std::uint32_t>(result.InstanceMaterials.size());
        result.MeshInstanceMaterialOffsets.push_back(materialOffset);
        for (const MaterialId material : meshInstance.materials) {
            result.InstanceMaterials.push_back(static_cast<std::int32_t>(material));
        }

        Bounds localBounds;
        localBounds.Min = mesh.boundsMin;
        localBounds.Max = mesh.boundsMax;

        PrimitiveReference reference;
        reference.Type = GpuPrimitiveType::Mesh;
        reference.PrimitiveIndex = meshInstance.mesh;
        reference.TransformIndex = transformIndex;
        reference.MeshInstanceIndex = meshInstanceIndex;
        reference.MaterialOffset = static_cast<std::int32_t>(materialOffset);
        reference.BoundingBox = TransformBounds(localBounds, meshInstance.objectToWorld);
        const glm::vec3 localCentroid = 0.5f * (mesh.boundsMin + mesh.boundsMax);
        reference.Centroid = TransformPoint(meshInstance.objectToWorld, localCentroid);
        references.push_back(reference);
    }

    if (references.empty()) {
        return result;
    }
    tlasLeafSize = std::max(tlasLeafSize, std::uint32_t{1});
    result.TlasNodes.reserve(references.size() * 2);
    const std::uint32_t rootNode = BuildBvhNode(references, result.TlasNodes, 0,
        static_cast<std::uint32_t>(references.size()), tlasLeafSize);
    assert(rootNode == 0);


    result.MeshInstancePrimitiveRefs.assign(meshInstances.size(), std::numeric_limits<std::uint32_t>::max());
    result.PrimitiveBounds.reserve(references.size());
    result.PrimitiveRefs.reserve(references.size());
    for (std::uint32_t referenceIndex = 0; referenceIndex < references.size(); ++referenceIndex) {
        const PrimitiveReference& reference = references[referenceIndex];
        GpuPrimitiveRef gpuReference;
        gpuReference.Metadata = glm::ivec4(
            static_cast<int>(reference.Type),
            static_cast<int>(reference.PrimitiveIndex),
            static_cast<int>(reference.TransformIndex),
            reference.MaterialOffset);
        result.PrimitiveRefs.push_back(gpuReference);

        result.PrimitiveBounds.push_back({reference.BoundingBox.Min, reference.BoundingBox.Max});
        if (reference.MeshInstanceIndex != std::numeric_limits<std::uint32_t>::max()) {
            result.MeshInstancePrimitiveRefs[reference.MeshInstanceIndex] = referenceIndex;
        }
    }
    return result;
}

void BvhBuilder::RefitMeshIntance(const Scene &scene, std::size_t meshInstanceIndex, BvhBuildResult &result) {
    const auto& instances = scene.GetMeshInstances();
    const std::uint32_t primitiveRefIndex = result.MeshInstancePrimitiveRefs[meshInstanceIndex];

    GpuPrimitiveRef& primitiveRef = result.PrimitiveRefs[primitiveRefIndex];
    const auto transformIndex = static_cast<std::uint32_t>(primitiveRef.Metadata.z);
    const SceneMeshInstance& instance = instances[meshInstanceIndex];
    const SceneMesh& mesh = scene.GetMeshes().at(instance.mesh);

    GpuTransform& gpuTransform = result.Transforms.at(transformIndex);
    gpuTransform.ObjectToWorld = instance.objectToWorld;
    gpuTransform.WorldToObject = glm::inverse(instance.objectToWorld);

    Bounds localBounds;
    localBounds.Min = mesh.boundsMin;
    localBounds.Max = mesh.boundsMax;
    const Bounds worldBounds = TransformBounds(localBounds, instance.objectToWorld);
    result.PrimitiveBounds[primitiveRefIndex] = {worldBounds.Min, worldBounds.Max};

    refitTLAS(result);
}
