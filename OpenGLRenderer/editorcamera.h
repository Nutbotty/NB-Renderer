//
// Created by Nutbotty on 7/17/2026.
//

#ifndef NB_RENDERER_EDITORCAMERA_H
#define NB_RENDERER_EDITORCAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../Scene/Scene.h"

enum CameraMovement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

class EditorCamera {
public:

    glm::vec3 LookFrom{0.0f, 0.0f, 0.0f};
    glm::vec3 LookAt{0.0f, 0.0f, -1.0f};
    glm::vec3 VUp{0.0f, 1.0f, 0.0f};

    float VerticalFov = 40.0f;
    float DefocusAngle = 0.0f;
    float FocusDistance = 1.0f;

    float MovementSpeed = 10.0f;
    float MouseSensitivity = 0.1f;


    EditorCamera() = default;
    explicit EditorCamera(const SceneCamera& camera);
    void SetFromSceneCamera(const SceneCamera& camera);

    [[nodiscard]] SceneCamera ToSceneCamera() const;
    [[nodiscard]] glm::mat4 GetViewMatrix() const;

    void ProcessKeyboard(CameraMovement direction,float deltaTime);
    void ProcessMouseMovement(float xOffset, float yOffset);
    void ProcessMouseScroll(float yOffset);

    [[nodiscard]] const glm::vec3& GetFront() const {
        return Front;
    }
    [[nodiscard]] const glm::vec3& GetRight() const {
        return Right;
    }

    [[nodiscard]] const glm::vec3& GetCameraUp() const{
        return CameraUp;
    }

private:
    glm::vec3 Front{0.0f, 0.0f, -1.0f};
    glm::vec3 Right{1.0f, 0.0f, 0.0f};
    glm::vec3 CameraUp{0.0f, 1.0f, 0.0f};

    float yaw = -90.0;
    float pitch = 0.0f;

     // lookFrom - lookAt
    float TargetDistance = 1.0f;

    void UpdateAnglesFromTarget();
    void UpdateVectorsFromAngles();
};

#endif //NB_RENDERER_CAMERA_H
