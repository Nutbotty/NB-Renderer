//
// Created by Nutbotty on 8/9/2026.
//

#ifndef NB_RENDERER_APPLICATION_H
#define NB_RENDERER_APPLICATION_H

#include <memory>

#include "../Scene/scene.h"
#include "../Renderer/Renderer.h"
#include "../Editor/Editor.h"

class Application {
public:
    Application();
    ~Application();

    void Run();

private:
    Window m_Window;
    Scene m_Scene;

    std::unique_ptr<Renderer> m_Renderer;

    // EditorLayer m_Editor;

    bool m_Running = true;
};

#endif //NB_RENDERER_APPLICATION_H
