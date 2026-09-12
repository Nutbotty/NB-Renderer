//
// Created by Nutbotty on 8/9/2026.
//
#include "Application.h"

#include <stdexcept>

#include "../Renderer/OpenGLRenderer/OpenGLRenderer.h"

Application::Application(Scene scene)
    : m_Window(2560, 1440, "NB Renderer"),
      m_LastFrameTime(std::chrono::steady_clock::now()) {
    if (m_Window.Initialize() != 0) {
        throw std::runtime_error("Failed to initialize window");
    }
    m_Scene = scene;
    m_Renderer = Renderer::Create(RenderBackend::OpenGL);
    if (!m_Renderer) {
        throw std::runtime_error("Failed to create renderer");
    }
    m_Renderer->Initialize(m_Window);
    m_Editor.Initialize(m_Window, *m_Renderer, m_Scene);
}

Application::~Application() {
    m_Editor.Shutdown();
    if (m_Renderer) {
        m_Renderer->Shutdown();
        m_Renderer.reset();
    }
    m_Window.Close();
}

void Application::Run() {
    while (!m_Window.ShouldClose() && m_Running) {
        m_Window.PollEvents();
        float deltaTime = CalculateDeltaTime();
        m_Editor.BeginFrame();
        m_Editor.Update(m_Scene, *m_Renderer, deltaTime);
        m_Renderer->Render(m_Scene, m_Editor.GetEditorCamera(), m_Editor.GetViewportRenderSettings());
        m_Editor.Render();
        m_Renderer->Present();
    }
}