//
// Created by Nutbotty on 7/20/2026.
//

#ifndef NB_RENDERER_OPENGLRENDERER_H
#define NB_RENDERER_OPENGLRENDERER_H

#include "../Renderer.h"
#include "../../Core/BvhBuilder.h"

#include <cstdint>
#include <memory>
#include <vector>

class Shader;

class OpenGLRenderer final : public Renderer {
public:
    OpenGLRenderer() = default;

    ~OpenGLRenderer() override = default;

    void Initialize(Window& window) override;
    void Shutdown() override;
    void SetScene(const Scene& scene) override;
    void Render(const Scene& scene, const EditorCamera& camera, const RenderSettings& settings) override;
    void ResetAccumulation() override;

    [[nodiscard]] std::uint32_t GetAccumulationFrame() const override{
        return m_AccumulationFrame;
    }
    [[nodiscard]] RenderBackend GetBackend() const override {
        return RenderBackend::OpenGL;
    }
private:

    // GPU Resource Management
    void UploadScene(const Scene& scene);
    void UploadEnvironment(const Scene& scene);
    void DestroySceneResources();
    void CreateOutputTexture(std::uint32_t width, std::uint32_t height);
    void DestroyOutputTexture();

    // Rendering
    void BindSceneBuffers();
    void SetCameraUniforms(const EditorCamera& camera);
    void SetEnvironmentUniforms(const Scene& scene);
    void DispatchCompute(const Scene& scene, const EditorCamera& camera);

    // Helpers
    static unsigned int CreateStorageBuffer(unsigned int binding, const void* data, std::size_t size);
    static unsigned int CreateHdrTexture(const SceneTexture& texture);

    Window* m_Window = nullptr;
    std::unique_ptr<Shader> m_EditorCompShader;
    std::unique_ptr<Shader> m_FinalCompShader;

    unsigned int m_OutputTexture = 0;
    std::uint32_t m_RenderWidth = 0;
    std::uint32_t m_RenderHeight = 0;

    //Scene buffers
    unsigned int m_MaterialBuffer = 0;
    unsigned int m_SphereBuffer = 0;
    unsigned int m_TlasBuffer = 0;
    unsigned int m_QuadBuffer = 0;
    unsigned int m_TriangleBuffer = 0;
    unsigned int m_PrimitiveRefBuffer = 0;
    unsigned int m_TransformBuffer = 0;
    unsigned int m_MeshVertexBuffer = 0;
    unsigned int m_MeshTriangleBuffer = 0;
    unsigned int m_MeshBuffer = 0;
    unsigned int m_BlasBuffer = 0;
    unsigned int m_EnvironmentTexture = 0;

    BvhBuildResult m_GpuScene;
    std::uint32_t m_AccumulationSamples = 0;
    bool m_SceneUploaded = false;
};

#endif //NB_RENDERER_OPENGLRENDERER_H
