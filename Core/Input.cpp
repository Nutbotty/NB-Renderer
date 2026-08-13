//
// Created by Nutbotty on 7/20/2026.
//

#include "../Core/Input.h"

Input::Input(EditorCamera& camera) : m_Camera(camera) {}

void Input::Initialize(GLFWwindow* window) {
    m_Window = window;
    glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, MouseCallback);
    glfwSetScrollCallback(window, ScrollCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void Input::Update(float deltaTime, bool viewportHovered, bool viewportFocused) {
    const bool rightMouseDown = glfwGetMouseButton(m_Window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    if (!m_CameraLookActive && rightMouseDown && viewportHovered) {
        BeginCameraLook();
    }
    if (m_CameraLookActive && !rightMouseDown) {
        EndCameraLook();
    }
    if (m_CameraLookActive) {
        if (m_MouseDeltaX != 0.0f || m_MouseDeltaY != 0.0f) {
            m_Camera.ProcessMouseMovement(m_MouseDeltaX, m_MouseDeltaY);
        }
        if (glfwGetKey(m_Window, GLFW_KEY_W) == GLFW_PRESS) {
            m_Camera.ProcessKeyboard(FORWARD, deltaTime);
        }
        if (glfwGetKey(m_Window, GLFW_KEY_S) == GLFW_PRESS) {
            m_Camera.ProcessKeyboard(BACKWARD, deltaTime);
        }
        if (glfwGetKey(m_Window, GLFW_KEY_A) == GLFW_PRESS) {
            m_Camera.ProcessKeyboard(LEFT, deltaTime);
        }
        if (glfwGetKey(m_Window, GLFW_KEY_D) == GLFW_PRESS) {
            m_Camera.ProcessKeyboard(RIGHT, deltaTime);
        }
    }
    if (viewportHovered && m_ScrollDelta != 0.0f) {
        m_Camera.ProcessMouseScroll(m_ScrollDelta);
    }
    m_MouseDeltaX = 0.0f;
    m_MouseDeltaY = 0.0f;
    m_ScrollDelta = 0.0f;
}

void Input::MouseCallback(GLFWwindow* window, double xposIn, double yposIn) {
    auto* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
    if (!input) {
        return;
    }
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (!input->m_CameraLookActive) {
        input->m_LastX = xpos;
        input->m_LastY = ypos;
        input->m_FirstMouse = true;
        return;
    }
    if (input->m_FirstMouse) {
        input->m_LastX = xpos;
        input->m_LastY = ypos;
        input->m_FirstMouse = false;
    }

    float xoffset = xpos - input->m_LastX;
    float yoffset = input->m_LastY - ypos;

    input->m_LastX = xpos;
    input->m_LastY = ypos;
    input->m_MouseDeltaX += xoffset;
    input->m_MouseDeltaY += yoffset;
}

void Input::ScrollCallback(GLFWwindow* window, double, double yoffset) {
    auto* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
    if (!input) {
        return;
    }
    input->m_ScrollDelta += static_cast<float>(yoffset);
}

void Input::BeginCameraLook() {
    if (m_CameraLookActive) {
        return;
    }
    m_CameraLookActive = true;
    m_MouseDeltaX = 0.0f;
    m_MouseDeltaY = 0.0f;
    m_FirstMouse = true;
    glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void Input::EndCameraLook() {
    if (!m_CameraLookActive) {
        return;
    }
    m_CameraLookActive = false;
    m_MouseDeltaX = 0.0f;
    m_MouseDeltaY = 0.0f;
    m_FirstMouse = true;
    glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}
