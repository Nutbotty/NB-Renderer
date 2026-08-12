//
// Created by Nutbotty on 8/9/2026.
//

#ifndef NB_RENDERER_OPENGLIMGUIBACKEND_H
#define NB_RENDERER_OPENGLIMGUIBACKEND_H

#pragma once

#include "ImGuiBackend.h"


class OpenGLImGuiBackend final : public ImGuiBackend {
public:
    void Initialize(Window& window) override;
    void BeginFrame() override;
    void Render(ImDrawData* drawData) override;
    void Shutdown() override;
};

#endif //NB_RENDERER_OPENGLIMGUIBACKEND_H
