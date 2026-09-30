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
#include <../stb_image.h>


// This file was created with the assitance of generative AI and is not considered my own work
namespace
{

    constexpr auto options =
    fastgltf::Options::LoadExternalBuffers
    |
    fastgltf::Options::LoadExternalImages
    |
    fastgltf::Options::GenerateMeshIndices;

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
    glm::vec4 ToGlmVec4(
    const fastgltf::math::nvec4& value
)
    {
        return glm::vec4(
            static_cast<float>(value[0]),
            static_cast<float>(value[1]),
            static_cast<float>(value[2]),
            static_cast<float>(value[3])
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


    std::vector<TextureId> LoadImages(const fastgltf::Asset& asset, Scene& scene, const std::filesystem::path& basePath) {
    std::vector<TextureId> imageMap;
    imageMap.reserve(asset.images.size());

    for (const fastgltf::Image& image : asset.images) {
        SceneTexture sceneTexture;
        sceneTexture.type = SceneTextureType::Image2D;

        bool loaded = false;

        auto loadFromMemory =
            [&](
                const unsigned char* bytes,
                std::size_t byteCount
            )
        {
            int width = 0;
            int height = 0;
            int channels = 0;

            unsigned char* pixels =
                stbi_load_from_memory(
                    bytes,
                    static_cast<int>(
                        byteCount
                    ),
                    &width,
                    &height,
                    &channels,
                    STBI_rgb_alpha
                );

            if (!pixels)
            {
                return;
            }

            sceneTexture.width =
                width;

            sceneTexture.height =
                height;

            sceneTexture.channels =
                4;

            const std::size_t size =
                static_cast<std::size_t>(
                    width
                )
                *
                static_cast<std::size_t>(
                    height
                )
                * 4;

            sceneTexture.pixels.assign(
                pixels,
                pixels + size
            );

            stbi_image_free(
                pixels
            );

            loaded = true;
        };

        std::visit(
            fastgltf::visitor{
                [&](const fastgltf::sources::URI& source)
                {
                    std::string relativePath(
                        source.uri.path().begin(),
                        source.uri.path().end()
                    );

                    const std::filesystem::path imagePath =
                        basePath
                        / relativePath;

                    int width = 0;
                    int height = 0;
                    int channels = 0;

                    unsigned char* pixels =
                        stbi_load(
                            imagePath.string().c_str(),
                            &width,
                            &height,
                            &channels,
                            STBI_rgb_alpha
                        );

                    if (!pixels)
                    {
                        return;
                    }

                    sceneTexture.width =
                        width;

                    sceneTexture.height =
                        height;

                    sceneTexture.channels =
                        4;

                    const std::size_t size =
                        static_cast<std::size_t>(
                            width
                        )
                        *
                        static_cast<std::size_t>(
                            height
                        )
                        * 4;

                    sceneTexture.pixels.assign(
                        pixels,
                        pixels + size
                    );

                    stbi_image_free(
                        pixels
                    );

                    loaded = true;
                },

                [&](const fastgltf::sources::Array& source)
                {
                    loadFromMemory(
                        reinterpret_cast<
                            const unsigned char*
                        >(source.bytes.data()), source.bytes.size());
                },
                [&](const fastgltf::sources::BufferView& source) {
                    const auto& view = asset.bufferViews[source.bufferViewIndex];
                    const auto& buffer =
                        asset.buffers[view.bufferIndex];
                    std::visit(fastgltf::visitor{
                            [&](const fastgltf::sources::Array& data) {
                                const auto* begin = reinterpret_cast<const unsigned char*>
                                (data.bytes.data()) + view.byteOffset;
                                loadFromMemory(begin, view.byteLength);
                            },
                            [](const auto&)
                            {}}, buffer.data);
                },
                [](const auto&){}},image.data);
        if (!loaded){
            throw std::runtime_error("Failed to decode glTF image");
        }
        imageMap.push_back(scene.addTexture(std::move(sceneTexture)));
    }
    return imageMap;
}
    TextureId ResolveTexture(const fastgltf::Asset& asset, const std::vector<TextureId>& imageMap, std::size_t textureIndex) {
        if (textureIndex >= asset.textures.size()) {
            return InvalidTextureId;
        }

        const fastgltf::Texture& texture =
            asset.textures[
                textureIndex
            ];

        if (!texture.imageIndex.has_value())
        {
            return InvalidTextureId;
        }

        const std::size_t imageIndex =
            texture.imageIndex.value();

        if (
            imageIndex
            >= imageMap.size()
        )
        {
            return InvalidTextureId;
        }

        return imageMap[
            imageIndex
        ];
    }



    bool LoadPrimitiveTangents(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive,
    std::vector<SceneMeshVertex>& vertices,std::uint32_t vertexOffset, std::size_t expectedCount) {
        const auto* tangentAttribute =
            primitive.findAttribute("TANGENT");

        if (tangentAttribute == primitive.attributes.end()) {
            return false;
        }

        const fastgltf::Accessor& accessor =
            asset.accessors[
                tangentAttribute->accessorIndex];

        if (accessor.count != expectedCount) {
            std::cerr
                << "glTF tangent count does not match position count\n";

            return false;
        }

        fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, accessor,
            [&](const glm::vec4& tangent, std::size_t index){
                glm::vec3 direction = glm::vec3(tangent);
                const float lengthSquared = glm::dot(direction, direction);
                if (lengthSquared > 1e-12f) {
                    direction =
                        glm::normalize(direction);
                }
                vertices[vertexOffset + index].tangent = glm::vec4(direction, tangent.w < 0.0f ? -1.0f : 1.0f);
            });
        return true;
    }
    MaterialId LoadDefaultMaterial(
        Scene& scene
    )
    {
        return scene.addMaterial(
            MaterialType::Lambertian,
            glm::vec3(0.f),
            0.0f,
            1.0f
        );
    }

    bool GeneratePrimitiveTangents(
    std::vector<SceneMeshVertex>& vertices,
    std::uint32_t vertexOffset,
    std::size_t vertexCount,
    const std::vector<std::uint32_t>& indices)
{
    if (vertexCount == 0 || indices.empty()) {
        return false;
    }

    std::vector<glm::vec3> tangentAccum(
        vertexCount,
        glm::vec3(0.0f));

    std::vector<glm::vec3> bitangentAccum(
        vertexCount,
        glm::vec3(0.0f));

    for (std::size_t i = 0; i < indices.size(); i += 3) {
        const std::uint32_t i0 = indices[i + 0];
        const std::uint32_t i1 = indices[i + 1];
        const std::uint32_t i2 = indices[i + 2];

        if (i0 >= vertexCount ||
            i1 >= vertexCount ||
            i2 >= vertexCount)
        {
            return false;
        }

        const SceneMeshVertex& v0 =
            vertices[vertexOffset + i0];

        const SceneMeshVertex& v1 =
            vertices[vertexOffset + i1];

        const SceneMeshVertex& v2 =
            vertices[vertexOffset + i2];

        const glm::vec3 edge1 =
            v1.position - v0.position;

        const glm::vec3 edge2 =
            v2.position - v0.position;

        const glm::vec2 deltaUv1 =
            v1.texCoord - v0.texCoord;

        const glm::vec2 deltaUv2 =
            v2.texCoord - v0.texCoord;

        const float determinant =
            deltaUv1.x * deltaUv2.y -
            deltaUv1.y * deltaUv2.x;

        if (std::abs(determinant) < 1e-8f) {
            continue;
        }

        const float inverseDeterminant =
            1.0f / determinant;

        const glm::vec3 tangent =
            (
                edge1 * deltaUv2.y -
                edge2 * deltaUv1.y
            ) * inverseDeterminant;

        const glm::vec3 bitangent =
            (
                edge2 * deltaUv1.x -
                edge1 * deltaUv2.x
            ) * inverseDeterminant;

        tangentAccum[i0] += tangent;
        tangentAccum[i1] += tangent;
        tangentAccum[i2] += tangent;

        bitangentAccum[i0] += bitangent;
        bitangentAccum[i1] += bitangent;
        bitangentAccum[i2] += bitangent;
    }

    for (std::size_t i = 0; i < vertexCount; ++i) {
        SceneMeshVertex& vertex =
            vertices[vertexOffset + i];

        glm::vec3 N =
            glm::normalize(vertex.normal);

        glm::vec3 T =
            tangentAccum[i];

        if (glm::dot(T, T) < 1e-12f) {
            vertex.tangent =
                glm::vec4(0.0f);

            continue;
        }

        // Gram-Schmidt: make tangent orthogonal to normal.
        T =
            T -
            N * glm::dot(N, T);

        if (glm::dot(T, T) < 1e-12f) {
            vertex.tangent =
                glm::vec4(0.0f);

            continue;
        }

        T = glm::normalize(T);

        const glm::vec3 B =
            bitangentAccum[i];

        const float handedness =
            glm::dot(
                glm::cross(N, T),
                B) < 0.0f
                ? -1.0f
                : 1.0f;

        vertex.tangent =
            glm::vec4(
                T,
                handedness);
    }

    return true;
}

    std::vector<MaterialId> LoadMaterials(const fastgltf::Asset& asset, const std::vector<TextureId>& imageMap, Scene& scene) {
        std::vector<MaterialId> materialMap;
        materialMap.reserve(asset.materials.size());
        for (const fastgltf::Material& gltfMaterial : asset.materials) {
            std::size_t materialIndex = 0;

            for (const fastgltf::Material& gltfMaterial : asset.materials) {
                const auto& pbr = gltfMaterial.pbrData;
                std::cerr << "Material " << materialIndex << ":\n";
                std::cerr << "  baseColorFactor = " << pbr.baseColorFactor[0] << ", "
                    << pbr.baseColorFactor[1] << ", " << pbr.baseColorFactor[2] << ", " << pbr.baseColorFactor[3] << '\n';
                std::cerr << "  metallicFactor = " << pbr.metallicFactor << '\n';
                std::cerr << "  roughnessFactor = " << pbr.roughnessFactor << '\n';
                if (pbr.baseColorTexture.has_value()) {
                    std::cerr << "  baseColorTexture = "
                        << pbr.baseColorTexture->textureIndex<< '\n';
                } else {
                    std::cerr << "  baseColorTexture = NONE\n";
                }
                if (pbr.metallicRoughnessTexture.has_value()) {
                    std::cerr << "  metallicRoughnessTexture = "
                        << pbr.metallicRoughnessTexture->textureIndex << '\n';
                } else {
                    std::cerr << "  metallicRoughnessTexture = NONE\n";
                }
                ++materialIndex;
            }
            const auto& pbr = gltfMaterial.pbrData;

            SceneMaterial material;
            material.type = MaterialType::PbrMetalRough;
            material.baseColor = ToGlmVec4(pbr.baseColorFactor);
            material.metallic = std::clamp(static_cast<float>(pbr.metallicFactor), 0.0f, 1.0f);
            material.roughness = std::clamp(static_cast<float>(pbr.roughnessFactor),0.0f, 1.0f);
            material.emission = ToGlmVec3(gltfMaterial.emissiveFactor);
            material.emissionStrength = static_cast<float>(gltfMaterial.emissiveStrength);
            material.indexOfRefraction = static_cast<float>(gltfMaterial.ior);
            if (gltfMaterial.transmission) {
                material.transmission = std::clamp(static_cast<float>(
                            gltfMaterial.transmission->transmissionFactor), 0.0f, 1.0f);
            }
            if (pbr.baseColorTexture.has_value()) {
                const auto& texture = *pbr.baseColorTexture;
                material.baseColorTexture = ResolveTexture(asset, imageMap, texture.textureIndex);
                material.baseColorTexCoord = static_cast<std::uint32_t>(texture.texCoordIndex);
            }
            if (pbr.metallicRoughnessTexture.has_value()) {
                const auto& texture = *pbr.metallicRoughnessTexture;
                material.metallicRoughnessTexture = ResolveTexture(asset, imageMap, texture.textureIndex);
                material.metallicRoughnessTexCoord = static_cast<std::uint32_t>(texture.texCoordIndex);
            }
            if (gltfMaterial.emissiveTexture.has_value()) {
                const auto& texture = *gltfMaterial.emissiveTexture;
                material.emissiveTexture = ResolveTexture(asset, imageMap, texture.textureIndex);
                material.emissiveTexCoord = static_cast<std::uint32_t>(texture.texCoordIndex);
            }
            if (gltfMaterial.normalTexture.has_value()) {
                const auto& texture = *gltfMaterial.normalTexture;
                material.normalTexture = ResolveTexture(asset, imageMap, texture.textureIndex);
                material.normalTexCoord = static_cast<std::uint32_t>(texture.texCoordIndex);
                material.normalScale = static_cast<float>(texture.scale);
            }
            if (material.baseColorTexCoord != 0 || material.metallicRoughnessTexCoord != 0 || material.emissiveTexCoord != 0 || material.normalTexCoord != 0) {
                std::cerr << "Warning: only TEXCOORD_0 " "is currently supported\n";
            }
            const MaterialId materialId = scene.addMaterial(material);
            materialMap.push_back(materialId);
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

    bool LoadMesh(const fastgltf::Asset& asset, const fastgltf::Mesh& gltfMesh,
        const std::vector<MaterialId>& materialMap, MaterialId defaultMaterial, Scene& scene, MeshId& outputMesh) {
        std::vector<SceneMeshVertex> vertices;
        std::vector<SceneMeshTriangle> triangles;
        std::vector<MaterialId> meshMaterials;

        bool loadedAnyPrimitive = false;
        bool allPrimitivesHaveNormals = true;
        bool allPrimitivesHaveTexCoords = true;

        for (const fastgltf::Primitive& primitive : gltfMesh.primitives) {
            if (primitive.type != fastgltf::PrimitiveType::Triangles) {
                std::cerr << "Skipping non-triangle glTF " "primitive\n";
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
            const bool hasTangents = LoadPrimitiveTangents(asset, primitive, vertices, vertexOffset, vertexCount);
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

            bool tangentReady =
    hasTangents;

            if (!tangentReady &&
                hasNormals &&
                hasTexCoords)
            {
                tangentReady =
                    GeneratePrimitiveTangents(
                        vertices,
                        vertexOffset,
                        vertexCount,
                        indices);

                if (tangentReady) {
                    std::cerr
                        << "Generated tangents for glTF primitive\n";
                }
            }

            if (indices.size() % 3 != 0)
            {
                std::cerr
                    << "Triangle primitive index count "
                       "is not divisible by three\n";

                return false;
            }

            MaterialId material = defaultMaterial;
            if (primitive.materialIndex.has_value()) {
                const std::size_t gltfMaterialIndex = primitive.materialIndex.value();
                if (gltfMaterialIndex >= materialMap.size()) {
                    std::cerr << "Primitive contains an invalid " "material index\n";
                    return false;
                }
                material = materialMap[gltfMaterialIndex];
            }
            // Register the material as one of this mesh's slots.
            if (std::find(meshMaterials.begin(), meshMaterials.end(), material) == meshMaterials.end()) {
                meshMaterials.push_back(material);
            }
            for (std::size_t index = 0; index < indices.size(); index += 3) {
                const std::uint32_t localIndex0 = indices[index + 0];
                const std::uint32_t localIndex1 = indices[index + 1];
                const std::uint32_t localIndex2 = indices[index + 2];
                if (localIndex0 >= vertexCount || localIndex1 >= vertexCount || localIndex2 >= vertexCount) {
                    std::cerr << "Primitive contains an index " "outside its vertex range\n";
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

        outputMesh = scene.addMesh(std::move(vertices), std::move(triangles), std::move(meshMaterials), allPrimitivesHaveNormals, allPrimitivesHaveTexCoords);
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

    constexpr auto options = fastgltf::Options::LoadExternalBuffers
        | fastgltf::Options::GenerateMeshIndices
        | fastgltf::Options::GenerateMeshIndices;

    auto loadedAsset = parser.loadGltf(data.get(), path.parent_path(), options);

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
    const MaterialId defaultMaterial = LoadDefaultMaterial(scene);
    const auto imageMap = LoadImages(asset, scene, path.parent_path());
    const auto materialMap = LoadMaterials(asset, imageMap, scene);

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
            )) {
            std::cerr << "Failed to load glTF mesh " << meshIndex << '\n';
            return false;
        }
        meshMap[meshIndex] = sceneMesh;
    }

    std::size_t instanceCount = 0;
    if (!asset.scenes.empty()) {
        const std::size_t sceneIndex = asset.defaultScene.value_or(0);
        if (sceneIndex >= asset.scenes.size()) {
            std::cerr << "glTF default scene index is invalid\n";
            return false;
        }
        /*
         * iterateSceneNodes recursively walks the node hierarchy
         * and gives us each node's accumulated world transform.
         */
        fastgltf::iterateSceneNodes(asset, sceneIndex, fastgltf::math::fmat4x4(1.0f),
            [&](fastgltf::Node& node, const fastgltf::math::fmat4x4& nodeTransform) {
                if (!node.meshIndex.has_value()) {
                    return;
                }
                const std::size_t gltfMeshIndex = node.meshIndex.value();
                if (gltfMeshIndex >= meshMap.size()) {
                    std::cerr << "Node references an invalid " "mesh index\n";
                    return;
                }
                const glm::mat4 objectToWorld = rootTransform * ToGlmMatrix(nodeTransform);
                const MeshId meshId = meshMap[gltfMeshIndex];
                scene.addMeshInstance(meshId, objectToWorld);
                ++instanceCount;
            });
    } else {
        /*
         * This fallback is not a complete replacement for a
         * glTF scene hierarchy, but lets simple scene-less assets
         * remain loadable.
         */
        std::cerr << "glTF has no scene; creating one identity " "instance per mesh\n";
        for (const MeshId meshId : meshMap) {
            scene.addMeshInstance(meshId, rootTransform);
            ++instanceCount;
        }
    }
    std::cerr << "Imported model:\n" << "  Scene meshes:    " << meshMap.size() << '\n' << "  Mesh instances:  " << instanceCount << '\n';
    return true;
}