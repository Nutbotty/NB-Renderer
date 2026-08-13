//
// Created by Nutbotty on 8/9/2026.
//

#pragma once

#include "Editor.h"

#include <cassert>
#include "ImGuiBackend.h"

namespace {
    constexpr float CameraEpsilon = 1e-8f;

    CameraState CaptureCameraState(const EditorCamera& camera) {
        CameraState state;
        state.Position = camera.LookFrom;
        state.LookAt = camera.LookAt;
        state.Up = camera.VUp;
        state.VerticalFov = camera.VerticalFov;
        state.FocusDistance = camera.FocusDistance;
        state.DefocusAngle = camera.DefocusAngle;
        return state;
    }
    bool NearlyEqual(const glm::vec3& a, const glm::vec3& b) {
        const glm::vec3 difference = a - b;
        return glm::dot(difference, difference) <= CameraEpsilon * CameraEpsilon;
    }
    bool NearlyEqual(float a, float b) {
        return std::abs(a - b) <= CameraEpsilon;
    }

    bool CameraStateChanged(const CameraState& before, const CameraState& after) {
        return !NearlyEqual(before.Position, after.Position) ||
            !NearlyEqual(before.LookAt, after.LookAt) ||
            !NearlyEqual(before.Up, after.Up) ||
            !NearlyEqual(before.VerticalFov, after.VerticalFov) ||
            !NearlyEqual(before.FocusDistance, after.FocusDistance) ||
            !NearlyEqual(before.DefocusAngle, after.DefocusAngle);
    }
}

void EditorLayer::Initialize(Window& window, Renderer& renderer, const Scene& scene) {
    assert(!m_Initialized);
    m_Camera = EditorCamera(scene.GetCamera());
    m_Input = std::make_unique<Input>(m_Camera);
    m_Input->Initialize(window.GetNativeWindow());

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    ImGui::StyleColorsDark();

    m_ImGuiBackend = ImGuiBackend::Create(renderer.GetBackend());
    m_ImGuiBackend->Initialize(window);
    m_PreviousCameraState = CaptureCameraState(m_Camera);
    m_Initialized = true;
}

void EditorLayer::Update(Scene& scene, Renderer& renderer, float deltaTime) {
    DrawMenu(scene, renderer);
    if (m_ShowScenePanel) {
        DrawScenePanel(scene);
    }
    if (m_ShowViewport) {
        DrawViewport(renderer);
    } else {
        m_ViewportHovered = false;
        m_ViewportFocused = false;
    }
    m_Input->Update(deltaTime, m_ViewportHovered, m_ViewportFocused);
    const CameraState currentCameraState = CaptureCameraState(m_Camera);
    if (CameraStateChanged(m_PreviousCameraState, currentCameraState)) {
        renderer.ResetAccumulation();
    }
    m_PreviousCameraState = currentCameraState;
}

void EditorLayer::BeginFrame() {
    assert(m_Initialized);
    m_ImGuiBackend->BeginFrame();
    ImGui::NewFrame();
    ImGui::DockSpaceOverViewport();
}

void EditorLayer::DrawMenu(Scene& scene, Renderer& renderer) {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open Scene...")) {
            // later
        }
        if (ImGui::MenuItem("Save Scene")) {
            // later
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) {
            //callback to close app?
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Scene", nullptr, &m_ShowScenePanel);
        ImGui::MenuItem("Viewport", nullptr, &m_ShowViewport);
        ImGui::MenuItem("Renderer", nullptr, &m_ShowRendererPanel);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Render")) {
        if (ImGui::MenuItem("Reset Accumulation")) {
            renderer.ResetAccumulation();
        }
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void EditorLayer::DrawScenePanel(Scene& scene) {
    ImGui::Begin("Scene", &m_ShowScenePanel);
    ImGui::Text("Materials: %zu", scene.GetMaterials().size());
    ImGui::Text("Spheres: %zu", scene.GetSpheres().size());
    ImGui::Text("Meshes: %zu", scene.GetMeshes().size());
    ImGui::Text("Mesh Instances: %zu", scene.GetMeshInstances().size() );
    ImGui::End();
}

void EditorLayer::DrawViewport(Renderer& renderer) {
    ImGui::Begin("Viewport", &m_ShowViewport);
    // m_ViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
    m_ViewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    const ImVec2 available = ImGui::GetContentRegionAvail();
    ImGui::Text("Viewport size: %.0f x %.0f", available.x, available.y);
     ImGui::Image(renderer.GetViewportTexture(), available,
          ImVec2(0.0f, 1.0f),ImVec2(1.0f, 0.0f));
    m_ViewportHovered = ImGui::IsItemHovered();
    ImGui::End();
}

void EditorLayer::Render() {
    assert(m_Initialized);
    ImGui::Render();
    m_ImGuiBackend->Render(ImGui::GetDrawData());
}
void EditorLayer::Shutdown() {
    if (!m_Initialized){
        return;
    }
    if (m_ImGuiBackend) {
        m_ImGuiBackend->Shutdown();
        m_ImGuiBackend.reset();
    }
    ImGui::DestroyContext();
    m_Initialized = false;
}