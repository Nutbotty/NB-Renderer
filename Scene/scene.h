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
    [[nodiscard]] const SceneCamera& GetCamera() const
    {
        return m_Camera;
    }

    void Clear(){
        m_Spheres.clear();
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
    SceneCamera m_Camera;

    bool m_Dirty = true;
};

#endif //NB_RENDERER_SCENE_H
