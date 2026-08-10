//
// Created by Nutbotty on 8/9/2026.
//
#include "Application.h"


void Application::Run() {
    while (
        !m_Window.ShouldClose()
        && m_Running
    )
    {
        m_Window.PollEvents();

        // float deltaTime = CalculateDeltaTime();

        // m_Editor.BeginFrame();
        //
        // m_Editor.Update(
        //     m_Scene,
        //     *m_Renderer,
        //     deltaTime
        // );
        //
        // m_Renderer->Render(
        //     m_Scene,
        //     m_Editor.GetCamera()
        // );
        //
        // m_Editor.Render(
        //     *m_Renderer
        // );
        //
        // m_Window.Present();
    }
}