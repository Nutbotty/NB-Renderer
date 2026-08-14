//
// Created by Nutbotty on 8/9/2026.
//

#ifndef NB_RENDERER_RENDERER_H
#define NB_RENDERER_RENDERER_H
#include <cstdint>
#include <memory>
#include "../Core/Window.h"
#include "../Editor/editorcamera.h"

enum class RenderBackend {
    OpenGL,
    Vulkan
};
enum class RenderMode {
    Editor,
    Final
};

struct RenderSettings {
    RenderMode mode = RenderMode::Editor;
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    std::uint32_t maxSamples = 2048;
    bool useFinalShader = false;
};

class Renderer {
public:
    virtual ~Renderer() = default;
    static std::unique_ptr<Renderer> Create(RenderBackend backend);

    virtual void Initialize(Window& window) = 0;
    virtual void Shutdown() = 0;
    virtual void SetScene(const Scene& scene) = 0;
    virtual void UpdateMaterials(const Scene& scene) = 0;
    virtual void Render(const Scene& scene, const EditorCamera& camera, const RenderSettings& settings) = 0;
    virtual void Present() = 0;
    virtual void Resize(std::uint32_t width, std::uint32_t height) = 0;
    virtual void ResetAccumulation() = 0;

    [[nodiscard]] virtual std::uint32_t GetAccumulationSamples() const = 0;
    [[nodiscard]] virtual RenderBackend GetBackend() const = 0;
    [[nodiscard]] virtual void* GetViewportTexture() const = 0;
};

#endif //NB_RENDERER_RENDERER_H
