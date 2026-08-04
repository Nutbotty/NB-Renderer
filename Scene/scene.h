//
// Created by Nutbotty on 7/17/2026.
//

#ifndef NB_RENDERER_SCENE_H
#define NB_RENDERER_SCENE_H

#include <vector>
#include <cstdint>
#include "../external/glm/glm/glm.hpp"

using MaterialId = std::uint32_t;
using SphereId = std::uint32_t;
using QuadId = std::uint32_t;
using TriId = std::uint32_t;
using InstanceId = std::uint32_t;
using MeshId = std::uint32_t;
using MeshInstanceId = std::uint32_t;

//materials
enum class MaterialType : std::uint32_t {
    Lambertian = 0,
    Metal = 1,
    Dielectric = 2
};
struct SceneMaterial {
    MaterialType type = MaterialType::Lambertian;
    glm::vec3 albedo{1.0f, 1.0f, 1.0f};
    float fuzz = 0.0f;
    float indexOfRefraction = 1.0f;
    glm::vec3 emission{0.0, 0.0, 0.0};
    float emissionStrength = 0.0;
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

    MaterialId addMaterial(MaterialType type, const glm::vec3& albedo = glm::vec3(1.0f), float fuzz = 0.0f,float indexOfRefraction = 1.0f, const glm::vec3& emission = glm::vec3(0.0f),
    float emissionStrength = 0.0f){
        SceneMaterial material;
        material.type = type;
        material.albedo = albedo;
        material.fuzz = fuzz;
        material.indexOfRefraction = indexOfRefraction;
        material.emission = emission;
        material.emissionStrength = emissionStrength;
        const auto id = static_cast<MaterialId>(m_Materials.size());
        m_Materials.push_back(material);
        MarkDirty();
        return id;
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



    void SetCamera(const SceneCamera& camera) {
        m_Camera = camera;
        MarkDirty();
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
    [[nodiscard]] const SceneCamera& GetCamera() const {
        return m_Camera;
    }

    void Clear(){
        m_Spheres.clear();
        m_Quads.clear();
        m_Tris.clear();
        m_Materials.clear();
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

    SceneCamera m_Camera;

    bool m_Dirty = true;
};

#endif //NB_RENDERER_SCENE_H
