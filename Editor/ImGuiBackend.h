//
// Created by Nutbotty on 8/9/2026.
//

#ifndef NB_RENDERER_IMGUIBACKEND_H
#define NB_RENDERER_IMGUIBACKEND_H

#pragma once

#include <memory>
#include "../Core/Window.h"
#include "../external/imgui/imgui.h"
#include "../Renderer/Renderer.h"


struct ImDrawData;

class ImGuiBackend {
public:
    virtual ~ImGuiBackend() = default;
    static std::unique_ptr<ImGuiBackend> Create(RenderBackend api);
    virtual void Initialize(Window& window) = 0;
    virtual void BeginFrame() = 0;
    virtual void Render(ImDrawData* drawData) = 0;
    virtual void Shutdown() = 0;
};

#endif //NB_RENDERER_IMGUIBACKEND_H
