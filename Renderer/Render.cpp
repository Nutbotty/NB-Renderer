//
// Created by Nutbotty on 8/11/2026.
//
#include <stdexcept>
#include "Renderer.h"
#include "OpenGLRenderer/OpenGLRenderer.h"

std::unique_ptr<Renderer> Renderer::Create(RenderBackend backend) {
    switch (backend)
    {
        case RenderBackend::OpenGL:
            return std::make_unique<OpenGLRenderer>();

        case RenderBackend::Vulkan:
            throw std::runtime_error(
                "Vulkan renderer is not implemented yet"
            );
    }
    throw std::runtime_error("Unknown render backend");
}