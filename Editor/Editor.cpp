//
// Created by Nutbotty on 8/9/2026.
//

#pragma once

#include "Editor.h"

#include <cassert>
#include <iostream>

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
    m_DofEnabled = m_Camera.DefocusAngle > 0.0f;
    if (m_DofEnabled) {
        m_CameraDefocusAngle = m_Camera.DefocusAngle;
    }
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

    m_ViewportRenderSettings.mode = RenderMode::Editor;
    m_ViewportRenderSettings.width = 1280;
    m_ViewportRenderSettings.height = 720;
    m_ViewportRenderSettings.maxSamples = 24;
    m_FinalRenderSettings.mode = RenderMode::Final;
    m_FinalRenderSettings.width = 1920;
    m_FinalRenderSettings.height = 1080;
    m_FinalRenderSettings.maxSamples = 2048;

    m_Initialized = true;
}

void EditorLayer::Update(Scene& scene, Renderer& renderer, float deltaTime) {
    DrawMenu(scene, renderer);
    if (m_ShowRenderPanel) {
        DrawRenderPanel(renderer);
    }
    if (m_ShowScenePanel) {
        DrawScenePanel(scene, renderer);
    }
    if (m_ShowObjectPanel) {
        DrawObjectPanel(scene, renderer);
    }
    if (m_ShowCameraPanel) {
        DrawCameraPanel();
    }
    if (m_ShowViewport) {
        DrawViewport(renderer);
    } else {
        m_ViewportHovered = false;
        m_ViewportFocused = false;
    }
    if (m_ShowEnvironmentPanel) {
        DrawEnvironmentPanel(scene, renderer);
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

void EditorLayer::DrawRenderPanel(Renderer& renderer)
{
    if (!ImGui::Begin("Render", &m_ShowRenderPanel)) {
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader("Viewport", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool changed = false;
        int width = static_cast<int>(m_ViewportRenderSettings.width);
        int height = static_cast<int>(m_ViewportRenderSettings.height);
        int maxSamples = static_cast<int>(m_ViewportRenderSettings.maxSamples);
        changed |= ImGui::InputInt("Width##Viewport", &width);
        changed |= ImGui::InputInt("Height##Viewport", &height);
        changed |= ImGui::InputInt("Max Samples##Viewport", &maxSamples);
        width = std::max(width, 1);
        height = std::max(height, 1);
        maxSamples = std::max(maxSamples, 1);
        if (changed) {
            m_ViewportRenderSettings.width = static_cast<std::uint32_t>(width);
            m_ViewportRenderSettings.height = static_cast<std::uint32_t>(height);
            m_ViewportRenderSettings.maxSamples = static_cast<std::uint32_t>(maxSamples);
            renderer.ResetAccumulation();
        }
        ImGui::Text("Accumulation: %u / %u", renderer.GetAccumulatedSamples(), m_ViewportRenderSettings.maxSamples);
        const float progress = m_ViewportRenderSettings.maxSamples > 0 ? std::min(1.0f,
                static_cast<float>(renderer.GetAccumulatedSamples()) /
                static_cast<float>(m_ViewportRenderSettings.maxSamples)) : 0.0f;
        ImGui::ProgressBar(progress, ImVec2(-FLT_MIN, 0.0f));
        if (ImGui::Button("Reset Viewport Accumulation")) {
            renderer.ResetAccumulation();
        }
    }
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Final Render", ImGuiTreeNodeFlags_DefaultOpen)) {
        int width = static_cast<int>(m_FinalRenderSettings.width);
        int height = static_cast<int>(m_FinalRenderSettings.height);
        int maxSamples = static_cast<int>(m_FinalRenderSettings.maxSamples);
        if (ImGui::InputInt("Width##Final", &width)) {
            m_FinalRenderSettings.width = static_cast<std::uint32_t>(std::max(width, 1));
        }
        if (ImGui::InputInt("Height##Final", &height)) {
            m_FinalRenderSettings.height = static_cast<std::uint32_t>(std::max(height, 1));
        }
        if (ImGui::InputInt("Samples##Final", &maxSamples)) {
            m_FinalRenderSettings.maxSamples = static_cast<std::uint32_t>(std::max(maxSamples, 1));
        }
        ImGui::InputText("Output", m_FinalRenderPath, sizeof(m_FinalRenderPath));
        ImGui::Spacing();
        const ImVec2 available = ImGui::GetContentRegionAvail();
        if (ImGui::Button("Final Render", ImVec2(available.x, 35.0f))){
            m_FinalRenderRequested = true;
        }
    }
    ImGui::End();
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
        ImGui::MenuItem("Environment", nullptr, &m_ShowEnvironmentPanel);
        ImGui::MenuItem("Camera", nullptr, &m_ShowCameraPanel);
        ImGui::MenuItem("Objects", nullptr, &m_ShowObjectPanel);
        ImGui::MenuItem("Viewport", nullptr, &m_ShowViewport);
        ImGui::MenuItem("Renderer", nullptr, &m_ShowRenderPanel);
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

void EditorLayer::DrawScenePanel(Scene& scene, Renderer& renderer) {
    ImGui::Begin("Scene", &m_ShowScenePanel);
    auto& materials = scene.GetMaterials();
    ImGui::Text("Materials: %zu", scene.GetMaterials().size());
    ImGui::Text("Spheres: %zu", scene.GetSpheres().size());
    ImGui::Text("Meshes: %zu", scene.GetMeshes().size());
    ImGui::Text("Mesh Instances: %zu", scene.GetMeshInstances().size() );
    ImGui::Separator();

    //Materials
    if (!materials.empty()) {
        if (m_SelectedMaterial>= materials.size()) {
            m_SelectedMaterial = 0;
        }
        std::string preview = "Material " + std::to_string(m_SelectedMaterial);
        if (ImGui::BeginCombo("Material", preview.c_str())) {
            for (std::size_t i = 0; i < materials.size(); ++i) {
                const bool selected = i == m_SelectedMaterial;
                std::string label = "Material " + std::to_string(i);
                if (ImGui::Selectable(label.c_str(),selected)) {
                    m_SelectedMaterial = i;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::Separator();

        //Mat editor
        SceneMaterial& material = materials[m_SelectedMaterial];

        bool changed = false;
        changed |= ImGui::ColorEdit3("Base Color", &material.baseColor.x);
        changed |= ImGui::SliderFloat("Metallic", &material.metallic, 0.0f, 1.0f);
        changed |=ImGui::SliderFloat("Roughness", &material.roughness, 0.0f, 1.0f );
        changed |= ImGui::DragFloat("IOR", &material.indexOfRefraction,0.01f,1.0f, 3.0f);
        changed |= ImGui::SliderFloat("Transmission", &material.transmission, 0.0f, 1.0f);
        changed |= ImGui::ColorEdit3("Emission", &material.emission.x);
        changed |= ImGui::DragFloat("Emission Strength", &material.emissionStrength, 0.1f, 0.0f, 100.0f);
        if (material.type == MaterialType::Metal) {
            changed |= ImGui::SliderFloat("Fuzz", &material.fuzz, 0.0f, 1.0f);
        }
        if (changed) {
            renderer.UpdateMaterials(scene);
        }
    }
    ImGui::End();
}

void EditorLayer::DrawObjectPanel(Scene& scene, Renderer& renderer) {
    if (!ImGui::Begin("Objects", &m_ShowObjectPanel)) {
        ImGui::End();
        return;
    }

    auto& instances = scene.GetMeshInstances();
    if (instances.empty()) {
        ImGui::TextDisabled("No objects in scene.");
        ImGui::End();
        return;
    }
    if (m_SelectedObject >= instances.size()) {
        m_SelectedObject = 0;
    }

    if (ImGui::BeginListBox("##ObjectList", ImVec2(-FLT_MIN, 150.0f))) {
        for (std::size_t i = 0; i < instances.size(); ++i) {
            const bool selected = i == m_SelectedObject;
            const std::string label = "Mesh Instance " + std::to_string(i);
            if (ImGui::Selectable(label.c_str(), selected)) {
                m_SelectedObject = i;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndListBox();
    }
    ImGui::Separator();
    SceneMeshInstance& instance = instances[m_SelectedObject];
    bool selectedInstanceChanged = false;
    bool selectedMaterialChanged = false;

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        selectedInstanceChanged |= ImGui::DragFloat3("Position", &instance.transform.Position.x, 0.05f, -10000.0f, 10000.0f, "%.3f");
        selectedInstanceChanged |= ImGui::DragFloat3("Rotation", &instance.transform.Rotation.x, 0.5f, -360.0f, 360.0f, "%.1f deg");
        selectedInstanceChanged |= ImGui::DragFloat3("Scale", &instance.transform.Scale.x, 0.01f, 0.001f, 1000.0f, "%.3f");
    }
    if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& materials = scene.GetMaterials();
        const auto& meshes = scene.GetMeshes();
        const SceneMesh& mesh = meshes.at(static_cast<std::size_t>(instance.mesh));

    if (instance.materials.size() != mesh.materials.size()) {
        instance.materials = mesh.materials;
        selectedMaterialChanged = true;
    }
    for (std::size_t slot = 0; slot < instance.materials.size(); ++slot) {
        ImGui::PushID(static_cast<int>(slot));
        MaterialId& assignedMaterial = instance.materials[slot];

        std::string preview;
        if (assignedMaterial < materials.size()) {
            preview = "Material " + std::to_string(static_cast<std::size_t>(assignedMaterial));
        } else {
            preview = "Invalid Material";
        }

        const std::string slotLabel = "Slot " + std::to_string(slot);
        if (ImGui::BeginCombo(slotLabel.c_str(), preview.c_str())) {
            for (std::size_t materialIndex = 0; materialIndex < materials.size(); ++materialIndex) {
                const MaterialId materialId = static_cast<MaterialId>(materialIndex);
                const bool selected = assignedMaterial == materialId;
                const std::string materialLabel = "Material " + std::to_string(materialIndex);
                if (ImGui::Selectable(materialLabel.c_str(), selected)) {
                    assignedMaterial = materialId;
                    selectedMaterialChanged = true;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            assignedMaterial = mesh.materials[slot];
            selectedMaterialChanged = true;
        }
        ImGui::PopID();
    }
}

    if (selectedMaterialChanged) {
        renderer.UpdateObjectMaterials(scene, m_SelectedObject);
    }
    if (selectedInstanceChanged) {
        instance.UpdateObjectToWorld();
        renderer.UpdateObjectTransform(scene, m_SelectedObject);
    }

    ImGui::End();
}

void EditorLayer::DrawCameraPanel() {
    if (!ImGui::Begin("Camera", &m_ShowCameraPanel)) {
        ImGui::End();
        return;
    }
    if (ImGui::CollapsingHeader("Lens", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Field of View", &m_Camera.VerticalFov, 10.0f, 120.0f, "%.1f deg");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Vertical field of view");
        }
    }
    if (ImGui::CollapsingHeader("Depth of Field", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool dofChanged = ImGui::Checkbox("Enable DoF", &m_DofEnabled);
        if (dofChanged) {
            if (m_DofEnabled) {
                m_Camera.DefocusAngle = std::max(m_CameraDefocusAngle, 0.01f);
            } else {
                if (m_Camera.DefocusAngle > 0.0f) {
                    m_CameraDefocusAngle = m_Camera.DefocusAngle;
                }
                m_Camera.DefocusAngle = 0.0f;
            }
        }
        if (!m_DofEnabled) {
            ImGui::BeginDisabled();
        }
        ImGui::DragFloat("Focus Distance", &m_Camera.FocusDistance, 0.05f, 0.01f, 1000.0f, "%.2f");
        float displayedDefocusAngle = m_DofEnabled ? m_Camera.DefocusAngle : m_CameraDefocusAngle;
        if (ImGui::SliderFloat("Defocus Angle", &displayedDefocusAngle, 0.01f, 10.0f, "%.2f deg")) {
            m_CameraDefocusAngle = displayedDefocusAngle;
            if (m_DofEnabled) {
                m_Camera.DefocusAngle = displayedDefocusAngle;
            }
        }
        if (!m_DofEnabled) {
            ImGui::EndDisabled();
        }
    }
    if (ImGui::CollapsingHeader("Transform")) {
        ImGui::DragFloat3("Position", &m_Camera.LookFrom.x, 0.05f, -10000.0f, 10000.0f, "%.3f");
        ImGui::DragFloat3("Target", &m_Camera.LookAt.x, 0.05f, -10000.0f, 10000.0f, "%.3f");
        ImGui::DragFloat3("Up", &m_Camera.VUp.x, 0.01f, -1.0f, 1.0f, "%.3f");
    }
    ImGui::End();
}

void EditorLayer::DrawEnvironmentPanel(Scene& scene, Renderer& renderer) {
    ImGui::Begin("Environment", &m_ShowEnvironmentPanel);
    SceneEnvironment& environment = scene.GetEnvironment();
    bool changed = false;
    changed |= ImGui::Checkbox("Use HDRI", &environment.HDRI);
    ImGui::Separator();

    if (environment.HDRI) {
        ImGui::TextUnformatted("HDRI Environment");
        changed |= ImGui::DragFloat("Intensity", &environment.intensity,
            0.01f, 0.0f, 100.0f, "%.2f");
        changed |= ImGui::DragFloat3("Rotation", &environment.rotation.x,
            0.5f, -360.0f,360.0f, "%.1f deg");
    }
    else {
        ImGui::TextUnformatted("Procedural Sky");

        changed |= ImGui::ColorEdit3("Horizon Color", &environment.color1.x);
        changed |= ImGui::ColorEdit3("Sky Color", &environment.color2.x);
    }
    if (changed) {
        renderer.ResetAccumulation();
    }
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