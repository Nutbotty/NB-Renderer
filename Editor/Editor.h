//
// Created by Nutbotty on 8/9/2026.
//

#ifndef NB_RENDERER_EDITOR_H
#define NB_RENDERER_EDITOR_H
#include <memory>

#include "editorcamera.h"
#include "ImGuiBackend.h"
#include "../Core/Window.h"
#include "../Renderer/Renderer.h"
#include "../Scene/scene.h"

class EditorLayer {
public:
    EditorLayer() = default;
    ~EditorLayer() = default;

    void Initialize(Window& window, Renderer& renderer, const Scene& scene);
    void Shutdown();
    void BeginFrame();
    void Update(Scene& scene, Renderer& renderer, float deltaTime);
    void Render();

    [[nodiscard]] const EditorCamera& GetEditorCamera() const {
        return m_Camera;
    }

private:
    void DrawMenu(Scene& scene, Renderer& renderer);
    void DrawScenePanel(Scene& scene);
    void DrawViewport(Renderer& renderer);

private:
    EditorCamera m_Camera;
    std::unique_ptr<ImGuiBackend> m_ImGuiBackend;
    bool m_Initialized = false;
    bool m_ShowScenePanel = true;
    bool m_ShowViewport = true;
    bool m_ShowRendererPanel = true;
};

#endif //NB_RENDERER_EDITOR_H
