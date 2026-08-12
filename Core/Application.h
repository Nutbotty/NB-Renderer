//
// Created by Nutbotty on 8/9/2026.
//

#ifndef NB_RENDERER_APPLICATION_H
#define NB_RENDERER_APPLICATION_H

#include <chrono>
#include <memory>

#include "../Scene/scene.h"
#include "../Renderer/Renderer.h"
#include "../Editor/Editor.h"

class Application {
public:
    Application(Scene scene);
    ~Application();

    void Run();
    float CalculateDeltaTime() {
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<float> elapsed = now - m_LastFrameTime;
        m_LastFrameTime = now;
        return std::min(elapsed.count(), 0.1f);
    };

    Scene GetScene() {
        return m_Scene;
    }

private:
    Window m_Window;
    Scene m_Scene;
    std::unique_ptr<Renderer> m_Renderer;
    EditorLayer m_Editor;
    bool m_Running = true;
    std::chrono::steady_clock::time_point m_LastFrameTime;
};

#endif //NB_RENDERER_APPLICATION_H
