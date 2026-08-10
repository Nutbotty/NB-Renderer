//
// Created by Nutbotty on 8/9/2026.
//

#ifndef NB_RENDERER_EDITOR_H
#define NB_RENDERER_EDITOR_H
#include "editorcamera.h"

class EditorLayer {
public:
    void Initialize(Window& window, Renderer& renderer);

    [[nodiscard]] EditorCamera GetEditorCamera() const {
        return m_camera;
    }

    void Shutdown();
    void BeginFrame();
    void Update(Scene& scene, Renderer& renderer, float deltaTime);
    void Render(Renderer& renderer);

private:
    void DrawMenu(Scene& scene, Renderer& renderer);
    void DrawScenePanel(Scene& scene);
    void DrawViewport(Renderer& renderer);

    EditorCamera m_camera;
};

#endif //NB_RENDERER_EDITOR_H
