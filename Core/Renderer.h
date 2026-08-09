//
// Created by Nutbotty on 8/9/2026.
//

#ifndef NB_RENDERER_RENDERER_H
#define NB_RENDERER_RENDERER_H
#include <cstdint>

enum class RenderBackend {
    OpenGL,
    Vulkan
};

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual void Initialize(Window& window) = 0;
    virtual void Exit() = 0;
    virtual void SetScene(const Scene& scene) = 0;
    virtual void Render(const Scene& scene, const EditorCamera& camera) = 0;
    virtual void Resize(std::uint32_t width, std::uint32_t height) = 0;
    virtual void ResetAccumulation();

    [[nodiscard]] virtual RenderBackend GetBackend() const;
};

#endif //NB_RENDERER_RENDERER_H
