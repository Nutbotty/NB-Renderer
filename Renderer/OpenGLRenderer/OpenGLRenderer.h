//
// Created by Nutbotty on 7/20/2026.
//

#ifndef NB_RENDERER_OPENGLRENDERER_H
#define NB_RENDERER_OPENGLRENDERER_H

#include "../Renderer.h"
#include "../../Core/BvhBuilder.h"
#include "Shaders/shader.h"

#include <cstdint>
#include <memory>
#include <vector>

class OpenGLRenderer final : public Renderer {
public:
    OpenGLRenderer() = default;

    ~OpenGLRenderer() override = default;

    void Initialize(Window& window) override;
    void Shutdown() override;
    void SetScene(const Scene& scene) override;
    void UpdateMaterials(const Scene &scene) override;
    void UpdateObjectMaterials(const Scene &scene, std::size_t objectIndex) override;
    void UpdateObjectTransform(const Scene &scene, std::size_t objectIndex) override;
    void Render(const Scene& scene, const EditorCamera& camera, const RenderSettings& settings) override;
    void RenderFinal(const Scene& scene, const EditorCamera& camera,
            const RenderSettings& settings, const std::filesystem::path& outputPath) override;
    void ResetAccumulation() override;

    [[nodiscard]] std::uint32_t GetAccumulatedSamples() const override{
        return m_AccumulatedSamples;
    }
    [[nodiscard]] RenderBackend GetBackend() const override {
        return RenderBackend::OpenGL;
    }
    [[nodiscard]] void* GetViewportTexture() const override {
        return reinterpret_cast<void*>(static_cast<intptr_t>(m_OutputTexture));
    }
private:

    // GPU Resource Management
    void UploadScene(const Scene& scene);
    void UploadEnvironment(const Scene& scene);
    void DestroySceneResources();
    void CreateOutputTexture(std::uint32_t width, std::uint32_t height);
    void Present() override;
    void Resize(std::uint32_t width, std::uint32_t height) override;
    void DestroyOutputTexture();

    // Rendering
    void BindSceneBuffers();
    void SetCameraUniforms(const EditorCamera& camera);
    void SetEnvironmentUniforms(const Scene& scene);
    void DispatchCompute(const Scene& scene, const EditorCamera& camera, GLuint outputTexture,
    std::uint32_t width, std::uint32_t height,std::uint32_t maxDepth,
    std::uint32_t samplesPerDispatch, std::uint32_t accumulatedSamples);

    //textures
    void UploadMaterialTextures(const Scene& scene);
    GLuint CreatePbrTextureArray(const Scene& scene, const std::vector<TextureId>& layerTextures, GLenum internalFormat);
    GLuint CreateRenderTexture(std::uint32_t width, std::uint32_t height);
    void SaveRenderTexture(GLuint texture, std::uint32_t width, std::uint32_t height, const std::filesystem::path& outputPath);

    // Helpers
    GLuint CreateStorageBuffer(GLuint binding, const void* data, GLsizeiptr size);
    static unsigned int CreateHdrTexture(const SceneTexture& texture);

    Window* m_Window = nullptr;
    std::unique_ptr<Shader> m_CompShader;

    unsigned int m_OutputTexture = 0;
    std::uint32_t m_RenderWidth = 0;
    std::uint32_t m_RenderHeight = 0;

    //Scene buffers
    unsigned int m_MaterialBuffer = 0;
    unsigned int m_InstanceMaterialBuffer = 0;
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
    GLuint m_BaseColorTextureArray = 0;
    GLuint m_MetalRoughTextureArray = 0;
    std::vector<int> m_BaseColorLayerByTexture;
    std::vector<int> m_MetalRoughLayerByTexture;

    BvhBuildResult m_GpuScene;
    std::uint32_t m_AccumulatedSamples = 0;
    bool m_SceneUploaded = false;
};

#endif //NB_RENDERER_OPENGLRENDERER_H
