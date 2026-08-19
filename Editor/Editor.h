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
#include "../Core/Input.h"

struct CameraState {
    glm::vec3 Position{0.0f};
    glm::vec3 LookAt{0.0f};
    glm::vec3 Up{0.0f};

    float VerticalFov = 20.0f;
    float FocusDistance = 0.0f;
    float DefocusAngle = 0.0f;
};

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
    void DrawScenePanel(Scene& scene, Renderer& renderer);
    void DrawEnvironmentPanel(Scene& scene, Renderer& renderer);
    void DrawObjectPanel(Scene& scene, Renderer& renderer);
    void DrawCameraPanel();
    void DrawViewport(Renderer& renderer);


    EditorCamera m_Camera;
    CameraState m_PreviousCameraState;
    std::unique_ptr<Input> m_Input;
    std::unique_ptr<ImGuiBackend> m_ImGuiBackend;
    bool m_Initialized = false;
    bool m_ShowScenePanel = true;
    std::size_t m_SelectedMaterial = 0;
    bool m_ShowEnvironmentPanel = true;
    bool m_ViewportHovered = false;
    bool m_ViewportFocused = false;
    bool m_ShowViewport = true;
    bool m_ShowRendererPanel = true;
    bool m_ShowObjectPanel = true;
    std::size_t m_SelectedObject = 0;
    bool m_ShowCameraPanel = true;
    bool m_DofEnabled = false;
    float m_CameraDefocusAngle = 2.5f;
};

#endif //NB_RENDERER_EDITOR_H
