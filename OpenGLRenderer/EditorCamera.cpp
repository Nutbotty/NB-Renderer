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
    glm::vec3 direction =
            LookAt - LookFrom;

    TargetDistance =
            glm::length(direction);

    if (TargetDistance < 1e-6f) {
        TargetDistance = 1.0f;

        direction =
                glm::vec3(
                    0.0f,
                    0.0f,
                    -1.0f
                );
    } else {
        direction /=
                TargetDistance;
    }

    Front =
            direction;

    Pitch =
            glm::degrees(
                std::asin(
                    glm::clamp(
                        Front.y,
                        -1.0f,
                        1.0f
                    )
                )
            );

    Yaw =
            glm::degrees(
                std::atan2(
                    Front.z,
                    Front.x
                )
            );

    Right =
            glm::normalize(
                glm::cross(
                    Front,
                    VUp
                )
            );

    CameraUp =
            glm::normalize(
                glm::cross(
                    Right,
                    Front
                ));
}

glm::mat4 EditorCamera::GetViewMatrix() const {
}
