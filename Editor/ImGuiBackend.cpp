//
// Created by Nutbotty on 8/9/2026.
//

#include "ImGuiBackend.h"

#include "OpenGLImGuiBackend.h"

#include <stdexcept>

std::unique_ptr<ImGuiBackend> ImGuiBackend::Create(RenderBackend backend) {
    switch (backend) {
        case RenderBackend::OpenGL: {
            return std::make_unique<OpenGLImGuiBackend>();
        }
        case RenderBackend::Vulkan: {
            throw std::runtime_error("Vulkan ImGui backend is not implemented yet");
        }
    }
    throw std::runtime_error("Unknown renderer backend");
}