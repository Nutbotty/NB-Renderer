//
// Created by Nutbotty on 7/17/2026.
//

#ifndef NB_RENDERER_SCENE_H
#define NB_RENDERER_SCENE_H

#include <vector>
#include <cstdint>
#include <string>

#include "../external/glm/glm/glm.hpp"

using MaterialId = std::uint32_t;
using SphereId = std::uint32_t;
using QuadId = std::uint32_t;
using TriId = std::uint32_t;
using InstanceId = std::uint32_t;
using MeshId = std::uint32_t;
using MeshInstanceId = std::uint32_t;
using TextureId = std::uint32_t;

constexpr MaterialId InvalidMaterialId = std::numeric_limits<MaterialId>::max();
constexpr TextureId InvalidTextureId = std::numeric_limits<TextureId>::max();


enum class SceneTextureType {
    Image2D,
    HDR
};
enum class MaterialType : std::uint32_t {
    Lambertian = 0,
    Metal = 1,
    Dielectric = 2,
    volumetric = 3,
    PbrMetalRough = 4
};
struct SceneTexture {
    SceneTextureType type;
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<std::uint8_t> pixels;
    std::vector<float> hdrPixels;
};
struct SceneEnvironment {
    TextureId texture;
    bool HDRI = false;
    float intensity = 1.0f;
    glm::vec3 rotation{0.0f, 0.0f, 0.0f};
    glm::vec3 color1{1.0, 1.0, 1.0};
    glm::vec3 color2{0.5, 0.7, 1.0};
};
struct SceneMaterial {
    MaterialType type = MaterialType::Lambertian;
    glm::vec4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float indexOfRefraction = 1.5f;
    float transmission = 0.0;

    glm::vec3 emission{0.0, 0.0, 0.0};
    float emissionStrength = 0.0;

    TextureId baseColorTexture = InvalidTextureId;
    TextureId metallicRoughnessTexture = InvalidTextureId;
    std::uint32_t baseColorTexCoord = 0;
    std::uint32_t metallicRoughnessTexCoord = 0;

    glm::vec3 albedo{1.0f, 1.0f, 1.0f};
    float fuzz = 0.0f;
};

//primitive
enum class ScenePrimitiveType : std::uint32_t {
    Sphere = 0,
    Quad = 1,
    Triangle = 2
};
struct SceneSphere {
    glm::vec3 center{0.0f};
    float radius = 0.5f;
    MaterialId material = 0;
};
struct SceneQuad {
    glm::vec3 Q{0.0f, 0.0f, 0.0f};
    glm::vec3 u{1.0f, 0.0f, 0.0f};
    glm::vec3 v{0.0f, 1.0f, 0.0f};
    MaterialId material = 0;
};
struct SceneTri {
    glm::vec3 Q{0.0f, 0.0f, 0.0f};
    glm::vec3 U{1.0f, 0.0f, 0.0f};
    glm::vec3 V{0.0f, 1.0f, 0.0f};
    MaterialId material = 0;
};
struct ScenePrimitiveRecord {
    ScenePrimitiveType type;
    std::uint32_t index;
};
struct SceneInstance {
    ScenePrimitiveRecord primitive;
    glm::mat4 objectToWorld{1.0f};
};

//meshes
struct SceneMeshVertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f};
    glm::vec2 texCoord{0.0f};
};
struct SceneMeshTriangle {
    std::uint32_t index0 = 0;
    std::uint32_t index1 = 0;
    std::uint32_t index2 = 0;
    MaterialId material = 0;
};
struct SceneMesh {
    std::vector<SceneMeshVertex> vertices;
    std::vector<SceneMeshTriangle> triangles;
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
    bool hasNormals = false;
    bool hasTexCoords = false;
};
struct SceneMeshInstance {
    MeshId mesh = 0;
    glm::mat4 objectToWorld{1.0f};
    MaterialId materialOverride = InvalidMaterialId;
};

//camera
struct SceneCamera {
    glm::vec3 lookFrom{0.0f, 0.0f, 0.0f};
    glm::vec3 lookAt{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float verticalFovDegrees = 40.0f;
    float defocusAngleDegrees = 0.0f;
    float focusDistance = 1.0f;
};

class Scene {
public:
    Scene() = default;

    TextureId addTexture(SceneTexture texture) {
        const TextureId id = static_cast<TextureId>(m_Textures.size());
        m_Textures.push_back(std::move(texture));
        MarkDirty();
        return id;
    }

    MaterialId addMaterial(const SceneMaterial& material) {
        const MaterialId id =
            static_cast<MaterialId>(
                m_Materials.size()
            );

        m_Materials.push_back(
            material
        );

        return id;
    }
    MaterialId addMaterial(MaterialType type, const glm::vec3& albedo, float fuzz = 0.0f, float indexOfRefraction = 1.5f,
    const glm::vec3& emission = glm::vec3(0.0f), float emissionStrength = 0.0f) {
        SceneMaterial material;
        material.type = type;
        material.baseColor = glm::vec4(albedo, 1.0f);
        material.fuzz = fuzz;
        material.indexOfRefraction = indexOfRefraction;
        material.emission = emission;
        material.emissionStrength = emissionStrength;

        switch (type) {
            case MaterialType::Lambertian: {
                material.metallic = 0.0f;
                material.roughness = 1.0f;
                material.transmission = 0.0f;
                break;
            }
            case MaterialType::Metal: {
                material.metallic = 1.0f;
                material.roughness = 0.0f;
                material.transmission = 0.0f;
                break;
            }
            case MaterialType::Dielectric: {
                material.metallic = 0.0f;
                material.roughness = 0.0f;
                material.transmission = 1.0f;
                break;
            }
            case MaterialType::PbrMetalRough: {
                break;
            }
        }
        return addMaterial(material);
    }

    SphereId addSphere(const glm::vec3& center, float radius, MaterialId material) {
        SceneSphere sphere;
        sphere.center = center;
        sphere.radius = radius;
        sphere.material = material;
        const auto id = static_cast<SphereId>(m_Spheres.size());
        m_Spheres.push_back(sphere);
        MarkDirty();
        return id;
    }
    QuadId addQuad(const glm::vec3& Q, const glm::vec3& u, const glm::vec3& v, MaterialId material) {
        SceneQuad quad;
        quad.Q = Q;
        quad.u = u;
        quad.v = v;
        quad.material = material;
        const auto id = static_cast<QuadId>(m_Quads.size());
        m_Quads.push_back(quad);
        MarkDirty();
        return id;
    }
    TriId addTri(const glm::vec3& Q, const glm::vec3& U, const glm::vec3& V, MaterialId material) {
        SceneTri tri;
        tri.Q = Q;
        tri.U = U;
        tri.V = V;
        tri.material = material;
        const auto id = static_cast<TriId>(m_Tris.size());
        m_Tris.push_back(tri);
        MarkDirty();
        return id;
    }
    InstanceId addInstance(ScenePrimitiveRecord primitive, const glm::mat4& transform) {
        const auto id = static_cast<InstanceId>(m_instances.size());
        m_instances.push_back(SceneInstance{primitive,transform});
        MarkDirty();
        return id;
    }

    MeshId addMesh(std::vector<SceneMeshVertex> vertices, std::vector<SceneMeshTriangle> triangles, bool hasNormals, bool hasTexCoords) {
        glm::vec3 boundsMin = vertices.front().position;
        glm::vec3 boundsMax = vertices.front().position;

        for (const SceneMeshVertex& vertex : vertices) {
            boundsMin = glm::min(boundsMin, vertex.position);
            boundsMax = glm::max(boundsMax, vertex.position);
        }

        const MeshId id = static_cast<MeshId>(m_Meshes.size());

        SceneMesh mesh;
        mesh.vertices = std::move(vertices);
        mesh.triangles = std::move(triangles);
        mesh.boundsMin = boundsMin;
        mesh.boundsMax = boundsMax;
        mesh.hasNormals = hasNormals;
        mesh.hasTexCoords = hasTexCoords;
        m_Meshes.push_back(std::move(mesh));
        MarkDirty();
        return id;
    }
    MeshInstanceId addMeshInstance(MeshId mesh, const glm::mat4& objectToWorld) {
        const MeshInstanceId id = static_cast<MeshInstanceId>(m_MeshInstances.size());
        m_MeshInstances.push_back(SceneMeshInstance{mesh, objectToWorld});
        MarkDirty();
        return id;
    }

    void SetCamera(const SceneCamera& camera) {
        m_Camera = camera;
        MarkDirty();
    }
    void SetEnvironment(const SceneEnvironment& environment) {
        m_Environment = environment;
        MarkDirty();
    }

    [[nodiscard]] const std::vector<SceneTexture>& GetTextures() const {
        return m_Textures;
    }

    std::vector<SceneMaterial>& GetMaterials() {
        return m_Materials;
    }
    [[nodiscard]] const std::vector<SceneMaterial>& GetMaterials() const {
        return m_Materials;
    }

    [[nodiscard]] const std::vector<SceneSphere>& GetSpheres() const {
        return m_Spheres;
    }
    [[nodiscard]] const std::vector<SceneQuad>& GetQuads() const {
        return m_Quads;
    }
    [[nodiscard]] const std::vector<SceneTri>& GetTris() const {
        return m_Tris;
    }
    [[nodiscard]] const std::vector<SceneInstance>& GetInstances() const {
        return m_instances;
    }
    [[nodiscard]] const std::vector<SceneMesh>& GetMeshes() const {
        return m_Meshes;
    }

    std::vector<SceneMeshInstance>& GetMeshInstances() {
        return m_MeshInstances;
    }
    [[nodiscard]]const std::vector<SceneMeshInstance>& GetMeshInstances() const {
        return m_MeshInstances;
    }


    [[nodiscard]]const SceneEnvironment& GetEnvironment() const {
        return m_Environment;
    }
    SceneEnvironment& GetEnvironment() {
        return m_Environment;
    }
    [[nodiscard]] const SceneCamera& GetCamera() const {
        return m_Camera;
    }

    void Clear() {
        m_Textures.clear();
        m_Materials.clear();
        m_Spheres.clear();
        m_Quads.clear();
        m_Tris.clear();
        m_instances.clear();
        m_Meshes.clear();
        m_MeshInstances.clear();
        MarkDirty();
    }
    [[nodiscard]] bool IsDirty() const {
        return m_Dirty;
    }
    void MarkDirty() {
        m_Dirty = true;
    }
    void ClearDirty() {
        m_Dirty = false;
    }
private:
    std::vector<SceneMaterial> m_Materials;
    std::vector<SceneSphere> m_Spheres;
    std::vector<SceneQuad> m_Quads;
    std::vector<SceneTri> m_Tris;
    std::vector<SceneInstance> m_instances;
    std::vector<SceneMesh> m_Meshes;
    std::vector<SceneMeshInstance> m_MeshInstances;

    std::vector<SceneTexture> m_Textures;
    SceneEnvironment m_Environment;
    SceneCamera m_Camera;

    bool m_Dirty = true;
};

#endif //NB_RENDERER_SCENE_H
