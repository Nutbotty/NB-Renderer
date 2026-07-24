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

enum class MaterialType : std::uint32_t
{
    Lambertian = 0,
    Metal = 1,
    Dielectric = 2
};
struct SceneMaterial {
    MaterialType type = MaterialType::Lambertian;
    glm::vec3 albedo{1.0f, 1.0f, 1.0f};
    float fuzz = 0.0f;
    float indexOfRefraction = 1.0f;
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
    glm::vec3 u{1.0f, 0.0f, 0.0f};
    glm::vec3 v{0.0f, 1.0f, 0.0f};
    MaterialId material = 0;
};
struct SceneCamera {
    glm::vec3 lookFrom{0.0f, 0.0f, 0.0f};
    glm::vec3 lookAt{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float verticalFovDegrees = 40.0f;
    float defocusAngleDegrees = 0.0f;
    float focusDistance = 1.0f;
};


class Scene
{
public:
    Scene() = default;

    MaterialId addMaterial(MaterialType type, const glm::vec3& albedo = glm::vec3(1.0f), float fuzz = 0.0f,float indexOfRefraction = 1.0f){
        SceneMaterial material;
        material.type = type;
        material.albedo = albedo;
        material.fuzz = fuzz;
        material.indexOfRefraction =indexOfRefraction;
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
        return id;
    }
    TriId addTri(const glm::vec3& Q, const glm::vec3& u, const glm::vec3& v, MaterialId material) {
        SceneTri tri;
        tri.Q = Q;
        tri.u = u;
        tri.v = v;
        tri.material = material;
        const auto id = static_cast<TriId>(m_Tris.size());
        m_Tris.push_back(tri);
        return id;
    }
    void SetCamera(const SceneCamera& camera) {
        m_Camera = camera;
        MarkDirty();
    }

    [[nodiscard]] const std::vector<SceneMaterial>& GetMaterials() const
    {
        return m_Materials;
    }
    [[nodiscard]] const std::vector<SceneSphere>& GetSpheres() const
    {
        return m_Spheres;
    }
    [[nodiscard]] const std::vector<SceneQuad>& GetQuads() const
    {
        return m_Quads;
    }
    [[nodiscard]] const std::vector<SceneTri>& GetTris() const
    {
        return m_Tris;
    }
    [[nodiscard]] const SceneCamera& GetCamera() const
    {
        return m_Camera;
    }

    void Clear(){
        m_Spheres.clear();
        m_Quads.clear();
        m_Tris.clear();
        m_Materials.clear();

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
    SceneCamera m_Camera;

    bool m_Dirty = true;
};

#endif //NB_RENDERER_SCENE_H
