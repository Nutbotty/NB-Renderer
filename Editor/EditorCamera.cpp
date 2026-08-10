//
// Created by Nutbotty on 7/26/2026.
//

#include "editorcamera.h"

EditorCamera::EditorCamera(const SceneCamera &camera) {
    SetFromSceneCamera(camera);
}

void EditorCamera::SetFromSceneCamera(const SceneCamera &camera) {
    LookFrom = camera.lookFrom;
    LookAt = camera.lookAt;
    VUp = camera.up;
    VerticalFov = camera.verticalFovDegrees;
    DefocusAngle = camera.defocusAngleDegrees;
    FocusDistance = camera.focusDistance;
    UpdateAnglesFromTarget();
}

void EditorCamera::UpdateAnglesFromTarget() {
    glm::vec3 direction = LookAt - LookFrom;
    TargetDistance = glm::length(direction);

    if (TargetDistance < 1e-8f) {
        TargetDistance = 1.0f;
        direction = glm::vec3(0.0f,0.0f,-1.0f);
    } else {
        direction /= TargetDistance;
    }
    Front = direction;
    pitch = glm::degrees(std::asin(glm::clamp(Front.y,-1.0f,1.0f)));
    yaw = glm::degrees(std::atan2(Front.z,Front.x));
    Right = glm::normalize(glm::cross(Front,VUp));
    CameraUp = glm::normalize(glm::cross(Right,Front));
}

void EditorCamera::UpdateVectorsFromAngles() {
    glm::vec3 front;
    front.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
    front.y = std::sin(glm::radians(pitch));
    front.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
    Front = glm::normalize(front);
    Right = glm::normalize(glm::cross(Front, VUp));
    CameraUp = glm::normalize(glm::cross(Right, Front));
    LookAt = LookFrom + Front * TargetDistance;
}

void EditorCamera::ProcessMouseMovement(float xOffset,float yOffset){
    xOffset *= MouseSensitivity;
    yOffset *= MouseSensitivity;
    yaw += xOffset;
    pitch += yOffset;
    pitch = glm::clamp(pitch,-89.0f,89.0f);
    UpdateVectorsFromAngles();
}

void EditorCamera::ProcessKeyboard(CameraMovement direction, float deltaTime) {
    const float velocity = MovementSpeed * deltaTime;
    glm::vec3 movement{0.0f};

    switch (direction) {
        case CameraMovement::FORWARD:
            movement += Front * velocity;
            break;
        case CameraMovement::BACKWARD:
            movement -= Front * velocity;
            break;
        case CameraMovement::LEFT:
            movement -= Right * velocity;
            break;
        case CameraMovement::RIGHT:
            movement += Right * velocity;
            break;
        case CameraMovement::UP:
            movement += VUp * velocity;
            break;
        case CameraMovement::DOWN:
            movement -= VUp * velocity;
            break;
    }
    LookFrom += movement;
    LookAt += movement;
}

void EditorCamera::ProcessMouseScroll(float yOffset) {
    VerticalFov -= yOffset;
    VerticalFov = glm::clamp(VerticalFov,1.0f,120.0f);
}

glm::mat4 EditorCamera::GetViewMatrix() const {
    return glm::lookAt(LookFrom, LookAt, VUp);
}

SceneCamera EditorCamera::ToSceneCamera() const {
    SceneCamera result;
    result.lookFrom = LookFrom;
    result.lookAt = LookAt;
    result.up = VUp;
    result.verticalFovDegrees = VerticalFov;
    result.defocusAngleDegrees = DefocusAngle;
    result.focusDistance = FocusDistance;
    return result;
}
