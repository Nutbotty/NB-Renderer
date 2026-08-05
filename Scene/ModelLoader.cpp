//
// Created by Nutbotty on 8/3/2026.
// This file was created with the assistance of generative AI and is not my own work
//

#include "ModelLoader.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>


// This file was created with the assitance of generative AI and is not my own work
namespace
{
    glm::vec3 ToGlmVec3(
        const fastgltf::math::nvec3& value
    )
    {
        return glm::vec3(
            static_cast<float>(value[0]),
            static_cast<float>(value[1]),
            static_cast<float>(value[2])
        );
    }

    glm::vec3 ToGlmVec3(
        const fastgltf::math::nvec4& value
    )
    {
        return glm::vec3(
            static_cast<float>(value[0]),
            static_cast<float>(value[1]),
            static_cast<float>(value[2])
        );
    }

    glm::mat4 ToGlmMatrix(
        const fastgltf::math::fmat4x4& matrix
    )
    {
        glm::mat4 result{1.0f};

        /*
         * fastgltf and GLM both use column-major matrix
         * indexing here: matrix[column][row].
         */
        for (std::size_t column = 0;
             column < 4;
             ++column)
        {
            for (std::size_t row = 0;
                 row < 4;
                 ++row)
            {
                result[column][row] =
                    matrix[column][row];
            }
        }

        return result;
    }

    MaterialId LoadDefaultMaterial(
        Scene& scene
    )
    {
        return scene.addMaterial(
            MaterialType::Lambertian,
            glm::vec3(0.8f),
            0.0f,
            1.0f
        );
    }

    std::vector<MaterialId> LoadMaterials(
        const fastgltf::Asset& asset,
        Scene& scene
    )
    {
        std::vector<MaterialId> materialMap;

        materialMap.reserve(
            asset.materials.size()
        );

        for (
            const fastgltf::Material& gltfMaterial :
            asset.materials
        )
        {
            const auto& pbr =
                gltfMaterial.pbrData;

            const glm::vec3 albedo =
                ToGlmVec3(
                    pbr.baseColorFactor
                );

            const glm::vec3 emission =
                ToGlmVec3(
                    gltfMaterial.emissiveFactor
                );

            const float metallic =
                std::clamp(
                    static_cast<float>(
                        pbr.metallicFactor
                    ),
                    0.0f,
                    1.0f
                );

            const float roughness =
                std::clamp(
                    static_cast<float>(
                        pbr.roughnessFactor
                    ),
                    0.0f,
                    1.0f
                );

            MaterialType materialType =
                MaterialType::Lambertian;

            /*
             * Your renderer has a simpler material model than
             * glTF's metallic-roughness PBR model, so this is
             * necessarily an approximation.
             */
            if (
                gltfMaterial.transmission
                && gltfMaterial.transmission
                       ->transmissionFactor > 0.5f
            )
            {
                materialType =
                    MaterialType::Dielectric;
            }
            else if (metallic > 0.5f)
            {
                materialType =
                    MaterialType::Metal;
            }

            const MaterialId materialId =
                scene.addMaterial(
                    materialType,
                    albedo,
                    roughness,
                    static_cast<float>(
                        gltfMaterial.ior
                    ),
                    emission,
                    static_cast<float>(
                        gltfMaterial.emissiveStrength
                    )
                );

            materialMap.push_back(
                materialId
            );
        }

        return materialMap;
    }

    bool LoadPrimitivePositions(
        const fastgltf::Asset& asset,
        const fastgltf::Primitive& primitive,
        std::vector<SceneMeshVertex>& vertices,
        std::uint32_t& vertexOffset,
        std::size_t& vertexCount
    )
    {
        const auto* positionAttribute =
            primitive.findAttribute(
                "POSITION"
            );

        if (
            positionAttribute
            == primitive.attributes.end()
        )
        {
            std::cerr
                << "glTF primitive has no POSITION "
                   "attribute\n";

            return false;
        }

        const fastgltf::Accessor& positionAccessor =
            asset.accessors[
                positionAttribute->accessorIndex
            ];

        vertexOffset =
            static_cast<std::uint32_t>(
                vertices.size()
            );

        vertexCount =
            positionAccessor.count;

        vertices.resize(
            vertices.size()
            + vertexCount
        );

        fastgltf::iterateAccessorWithIndex<
            glm::vec3
        >(
            asset,
            positionAccessor,
            [&](
                const glm::vec3& position,
                std::size_t index
            )
            {
                vertices[
                    vertexOffset + index
                ].position = position;
            }
        );

        return true;
    }

    bool LoadPrimitiveNormals(
        const fastgltf::Asset& asset,
        const fastgltf::Primitive& primitive,
        std::vector<SceneMeshVertex>& vertices,
        std::uint32_t vertexOffset,
        std::size_t expectedCount
    )
    {
        const auto* normalAttribute =
            primitive.findAttribute(
                "NORMAL"
            );

        if (
            normalAttribute
            == primitive.attributes.end()
        )
        {
            return false;
        }

        const fastgltf::Accessor& normalAccessor =
            asset.accessors[
                normalAttribute->accessorIndex
            ];

        if (
            normalAccessor.count
            != expectedCount
        )
        {
            std::cerr
                << "glTF normal count does not match "
                   "position count\n";

            return false;
        }

        fastgltf::iterateAccessorWithIndex<
            glm::vec3
        >(
            asset,
            normalAccessor,
            [&](
                const glm::vec3& normal,
                std::size_t index
            )
            {
                vertices[
                    vertexOffset + index
                ].normal =
                    glm::normalize(normal);
            }
        );

        return true;
    }

    bool LoadPrimitiveTexCoords(
        const fastgltf::Asset& asset,
        const fastgltf::Primitive& primitive,
        std::vector<SceneMeshVertex>& vertices,
        std::uint32_t vertexOffset,
        std::size_t expectedCount
    )
    {
        const auto* texCoordAttribute =
            primitive.findAttribute(
                "TEXCOORD_0"
            );

        if (
            texCoordAttribute
            == primitive.attributes.end()
        )
        {
            return false;
        }

        const fastgltf::Accessor& texCoordAccessor =
            asset.accessors[
                texCoordAttribute->accessorIndex
            ];

        if (
            texCoordAccessor.count
            != expectedCount
        )
        {
            std::cerr
                << "glTF texture-coordinate count does "
                   "not match position count\n";

            return false;
        }

        fastgltf::iterateAccessorWithIndex<
            glm::vec2
        >(
            asset,
            texCoordAccessor,
            [&](
                const glm::vec2& texCoord,
                std::size_t index
            )
            {
                vertices[
                    vertexOffset + index
                ].texCoord = texCoord;
            }
        );

        return true;
    }

    bool LoadPrimitiveIndices(
        const fastgltf::Asset& asset,
        const fastgltf::Primitive& primitive,
        std::vector<std::uint32_t>& indices
    )
    {
        /*
         * GenerateMeshIndices was enabled, so even a glTF
         * primitive that was originally non-indexed should now
         * have an index accessor.
         */
        if (!primitive.indicesAccessor.has_value())
        {
            std::cerr
                << "glTF primitive has no index accessor\n";

            return false;
        }

        const fastgltf::Accessor& indexAccessor =
            asset.accessors[
                primitive.indicesAccessor.value()
            ];

        indices.resize(
            indexAccessor.count
        );

        fastgltf::iterateAccessorWithIndex<
            std::uint32_t
        >(
            asset,
            indexAccessor,
            [&](
                std::uint32_t indexValue,
                std::size_t index
            )
            {
                indices[index] =
                    indexValue;
            }
        );

        return true;
    }

    bool LoadMesh(
        const fastgltf::Asset& asset,
        const fastgltf::Mesh& gltfMesh,
        const std::vector<MaterialId>& materialMap,
        MaterialId defaultMaterial,
        Scene& scene,
        MeshId& outputMesh
    )
    {
        std::vector<SceneMeshVertex> vertices;
        std::vector<SceneMeshTriangle> triangles;

        bool loadedAnyPrimitive = false;
        bool allPrimitivesHaveNormals = true;
        bool allPrimitivesHaveTexCoords = true;

        for (
            const fastgltf::Primitive& primitive :
            gltfMesh.primitives
        )
        {
            if (
                primitive.type
                != fastgltf::PrimitiveType::Triangles
            )
            {
                std::cerr
                    << "Skipping non-triangle glTF "
                       "primitive\n";

                continue;
            }

            std::uint32_t vertexOffset = 0;
            std::size_t vertexCount = 0;

            if (
                !LoadPrimitivePositions(
                    asset,
                    primitive,
                    vertices,
                    vertexOffset,
                    vertexCount
                )
            )
            {
                return false;
            }

            const bool hasNormals =
                LoadPrimitiveNormals(
                    asset,
                    primitive,
                    vertices,
                    vertexOffset,
                    vertexCount
                );

            const bool hasTexCoords =
                LoadPrimitiveTexCoords(
                    asset,
                    primitive,
                    vertices,
                    vertexOffset,
                    vertexCount
                );

            allPrimitivesHaveNormals &=
                hasNormals;

            allPrimitivesHaveTexCoords &=
                hasTexCoords;

            std::vector<std::uint32_t> indices;

            if (
                !LoadPrimitiveIndices(
                    asset,
                    primitive,
                    indices
                )
            )
            {
                return false;
            }

            if (indices.size() % 3 != 0)
            {
                std::cerr
                    << "Triangle primitive index count "
                       "is not divisible by three\n";

                return false;
            }

            MaterialId material =
                defaultMaterial;

            if (primitive.materialIndex.has_value())
            {
                const std::size_t gltfMaterialIndex =
                    primitive.materialIndex.value();

                if (
                    gltfMaterialIndex
                    >= materialMap.size()
                )
                {
                    std::cerr
                        << "Primitive contains an invalid "
                           "material index\n";

                    return false;
                }

                material =
                    materialMap[
                        gltfMaterialIndex
                    ];
            }

            for (
                std::size_t index = 0;
                index < indices.size();
                index += 3
            )
            {
                const std::uint32_t localIndex0 =
                    indices[index + 0];

                const std::uint32_t localIndex1 =
                    indices[index + 1];

                const std::uint32_t localIndex2 =
                    indices[index + 2];

                if (
                    localIndex0 >= vertexCount
                    || localIndex1 >= vertexCount
                    || localIndex2 >= vertexCount
                )
                {
                    std::cerr
                        << "Primitive contains an index "
                           "outside its vertex range\n";

                    return false;
                }

                SceneMeshTriangle triangle;

                triangle.index0 =
                    vertexOffset + localIndex0;

                triangle.index1 =
                    vertexOffset + localIndex1;

                triangle.index2 =
                    vertexOffset + localIndex2;

                triangle.material =
                    material;

                triangles.push_back(
                    triangle
                );
            }

            loadedAnyPrimitive = true;
        }

        if (
            !loadedAnyPrimitive
            || vertices.empty()
            || triangles.empty()
        )
        {
            std::cerr
                << "glTF mesh contains no supported "
                   "triangle geometry\n";

            return false;
        }

        outputMesh =
            scene.addMesh(
                std::move(vertices),
                std::move(triangles),
                allPrimitivesHaveNormals,
                allPrimitivesHaveTexCoords
            );

        return true;
    }
}

bool GltfLoader::Load(
    const std::filesystem::path& path,
    Scene& scene,
    const glm::mat4& rootTransform
)
{
    auto data =
        fastgltf::GltfDataBuffer::FromPath(
            path
        );

    if (
        data.error()
        != fastgltf::Error::None
    )
    {
        std::cerr
            << "Failed to read glTF file '"
            << path
            << "': "
            << fastgltf::getErrorMessage(
                data.error()
            )
            << '\n';

        return false;
    }

    /*
     * Enable only the extensions that this first loader
     * knows how to use.
     */
    constexpr auto supportedExtensions =
        fastgltf::Extensions::KHR_mesh_quantization
        | fastgltf::Extensions::KHR_materials_ior
        | fastgltf::Extensions::KHR_materials_transmission
        | fastgltf::Extensions::
            KHR_materials_emissive_strength;

    fastgltf::Parser parser(
        supportedExtensions
    );

    constexpr auto options =
        fastgltf::Options::LoadExternalBuffers
        | fastgltf::Options::GenerateMeshIndices;

    auto loadedAsset =
        parser.loadGltf(
            data.get(),
            path.parent_path(),
            options
        );

    if (
        loadedAsset.error()
        != fastgltf::Error::None
    )
    {
        std::cerr
            << "Failed to parse glTF file '"
            << path
            << "': "
            << fastgltf::getErrorMessage(
                loadedAsset.error()
            )
            << '\n';

        return false;
    }

    fastgltf::Asset& asset =
        loadedAsset.get();

    std::cerr
        << "Loaded glTF:\n"
        << "  meshes:    "
        << asset.meshes.size()
        << '\n'
        << "  nodes:     "
        << asset.nodes.size()
        << '\n'
        << "  materials: "
        << asset.materials.size()
        << '\n'
        << "  scenes:    "
        << asset.scenes.size()
        << '\n';

    /*
     * A glTF primitive may omit its material.
     */
    const MaterialId defaultMaterial =
        LoadDefaultMaterial(scene);

    const std::vector<MaterialId> materialMap =
        LoadMaterials(
            asset,
            scene
        );

    /*
     * Maps glTF mesh indices to your Scene MeshIds.
     *
     * Geometry is loaded once, even if multiple glTF nodes
     * reference the same mesh.
     */
    std::vector<MeshId> meshMap;

    meshMap.resize(
        asset.meshes.size()
    );

    for (
        std::size_t meshIndex = 0;
        meshIndex < asset.meshes.size();
        ++meshIndex
    )
    {
        MeshId sceneMesh = 0;

        if (
            !LoadMesh(
                asset,
                asset.meshes[meshIndex],
                materialMap,
                defaultMaterial,
                scene,
                sceneMesh
            )
        )
        {
            std::cerr
                << "Failed to load glTF mesh "
                << meshIndex
                << '\n';

            return false;
        }

        meshMap[meshIndex] =
            sceneMesh;
    }

    std::size_t instanceCount = 0;

    if (!asset.scenes.empty())
    {
        const std::size_t sceneIndex =
            asset.defaultScene.value_or(0);

        if (sceneIndex >= asset.scenes.size())
        {
            std::cerr
                << "glTF default scene index is invalid\n";

            return false;
        }

        /*
         * iterateSceneNodes recursively walks the node hierarchy
         * and gives us each node's accumulated world transform.
         */
        fastgltf::iterateSceneNodes(
            asset,
            sceneIndex,
            fastgltf::math::fmat4x4(1.0f),
            [&](
                fastgltf::Node& node,
                const fastgltf::math::fmat4x4&
                    nodeTransform
            )
            {
                if (!node.meshIndex.has_value())
                {
                    return;
                }

                const std::size_t gltfMeshIndex =
                    node.meshIndex.value();

                if (gltfMeshIndex >= meshMap.size())
                {
                    std::cerr
                        << "Node references an invalid "
                           "mesh index\n";

                    return;
                }

                const glm::mat4 objectToWorld =
                    rootTransform
                    * ToGlmMatrix(
                        nodeTransform
                    );

                scene.addMeshInstance(
                    meshMap[gltfMeshIndex],
                    objectToWorld
                );

                ++instanceCount;
            }
        );
    }
    else
    {
        /*
         * This fallback is not a complete replacement for a
         * glTF scene hierarchy, but lets simple scene-less assets
         * remain loadable.
         */
        std::cerr
            << "glTF has no scene; creating one identity "
               "instance per mesh\n";

        for (const MeshId mesh : meshMap)
        {
            scene.addMeshInstance(
                mesh,
                rootTransform
            );

            ++instanceCount;
        }
    }

    std::cerr
        << "Imported model:\n"
        << "  Scene meshes:    "
        << meshMap.size()
        << '\n'
        << "  Mesh instances:  "
        << instanceCount
        << '\n';

    return true;
}