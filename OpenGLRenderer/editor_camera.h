//
// Created by Nutbotty on 7/18/2026.
//

#ifndef NB_RENDERER_EDITOR_CAMERA_H
#define NB_RENDERER_EDITOR_CAMERA_H

#pragma once

#include <glm/glm.hpp>

struct EditorCameraState
{
    glm::vec3 position;
    glm::vec3 target;
    glm::vec3 up;

    float fov;
};

#endif //NB_RENDERER_EDITOR_CAMERA_H
