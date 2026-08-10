//
// Created by Nutbotty on 7/20/2026.
//

#include "../Core/Input.h"

Input::Input(EditorCamera& camera) : m_Camera(camera) {}

void Input::Initialize(GLFWwindow* window)
{
    m_Window = window;

    glfwSetWindowUserPointer(window, this);

    glfwSetCursorPosCallback(window, MouseCallback);
    glfwSetScrollCallback(window, ScrollCallback);

    glfwSetInputMode(
        window,
        GLFW_CURSOR,
        GLFW_CURSOR_DISABLED
    );
}

void Input::Update(float deltaTime)
{
    if (glfwGetKey(m_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(m_Window, true);

    if (glfwGetKey(m_Window, GLFW_KEY_W) == GLFW_PRESS)
        m_Camera.ProcessKeyboard(FORWARD, deltaTime);

    if (glfwGetKey(m_Window, GLFW_KEY_S) == GLFW_PRESS)
        m_Camera.ProcessKeyboard(BACKWARD, deltaTime);

    if (glfwGetKey(m_Window, GLFW_KEY_A) == GLFW_PRESS)
        m_Camera.ProcessKeyboard(LEFT, deltaTime);

    if (glfwGetKey(m_Window, GLFW_KEY_D) == GLFW_PRESS)
        m_Camera.ProcessKeyboard(RIGHT, deltaTime);
}

void Input::MouseCallback(
    GLFWwindow* window,
    double xposIn,
    double yposIn
)
{
    auto* input =
        static_cast<Input*>(glfwGetWindowUserPointer(window));

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (input->m_FirstMouse)
    {
        input->m_LastX = xpos;
        input->m_LastY = ypos;
        input->m_FirstMouse = false;
    }

    float xoffset = xpos - input->m_LastX;
    float yoffset = input->m_LastY - ypos;

    input->m_LastX = xpos;
    input->m_LastY = ypos;

    input->m_Camera.ProcessMouseMovement(
        xoffset,
        yoffset
    );
}

void Input::ScrollCallback(
    GLFWwindow* window,
    double,
    double yoffset
)
{
    auto* input =
        static_cast<Input*>(glfwGetWindowUserPointer(window));

    input->m_Camera.ProcessMouseScroll(
        static_cast<float>(yoffset)
    );
}
