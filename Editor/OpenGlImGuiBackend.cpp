//
// Created by Nutbotty on 8/9/2026.
//

#include "OpenGLImGuiBackend.h"
#include "../external/imgui/backends/imgui_impl_glfw.h"
#include "../external/imgui/backends/imgui_impl_opengl3.h"

#include "../Core/Window.h"

void OpenGLImGuiBackend::Initialize(Window& window) {
    ImGui_ImplGlfw_InitForOpenGL(window.GetNativeWindow(), true);
    ImGui_ImplOpenGL3_Init("#version 430");
}

void OpenGLImGuiBackend::BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
}

void OpenGLImGuiBackend::Render(ImDrawData* drawData) {
    ImGui_ImplOpenGL3_RenderDrawData(drawData);
}

void OpenGLImGuiBackend::Shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
}