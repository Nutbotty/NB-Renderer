//
// Created by Nutbotty on 7/20/2026.
//

#ifndef NB_RENDERER_INPUT_H
#define NB_RENDERER_INPUT_H
#include <GLFW/glfw3.h>
#include "../Editor/editorcamera.h"


class Input
{
public:
    explicit Input(EditorCamera& camera);

    void Initialize(GLFWwindow* window);
    void Update(float deltaTime);

private:
    static void MouseCallback(
        GLFWwindow* window,
        double xpos,
        double ypos
    );

    static void ScrollCallback(
        GLFWwindow* window,
        double xoffset,
        double yoffset
    );

private:
    EditorCamera& m_Camera;

    GLFWwindow* m_Window = nullptr;

    bool m_FirstMouse = true;

    float m_LastX = 0.0f;
    float m_LastY = 0.0f;
};

#endif //NB_RENDERER_INPUT_H
