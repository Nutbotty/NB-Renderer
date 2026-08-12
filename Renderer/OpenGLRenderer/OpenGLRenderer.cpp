//
// Created by Nutbotty on 7/20/2026.
//
#include "OpenGLRenderer.h"
#include "Shaders/shader.h"
#include "../../Core/Window.h"
#include <iostream>
#include <algorithm>
#include <limits>
#include <vector>
#include "../../Core/BvhBuilder.h"

namespace {
    constexpr GLuint MaterialBufferBinding = 1;
    constexpr GLuint SphereBufferBinding = 2;
    constexpr GLuint TlasBufferBinding = 3;
    constexpr GLuint QuadBufferBinding = 4;
    constexpr GLuint TriangleBufferBinding = 5;
    constexpr GLuint PrimitiveRefBufferBinding = 6;
    constexpr GLuint TransformBufferBinding = 7;
    constexpr GLuint MeshVertexBufferBinding = 8;
    constexpr GLuint MeshTriangleBufferBinding = 9;
    constexpr GLuint MeshBufferBinding = 10;
    constexpr GLuint BlasBufferBinding = 11;

    constexpr std::uint32_t TlasLeafSize = 8;
    constexpr std::uint32_t BlasLeafSize = 4;

    constexpr std::uint32_t ComputeLocalSizeX = 16;
    constexpr std::uint32_t ComputeLocalSizeY = 16;

    std::vector<GpuMaterial> BuildGpuMaterials(const Scene& scene) {
        std::vector<GpuMaterial> result;
        result.reserve(scene.GetMaterials().size());

        for (const SceneMaterial& material :scene.GetMaterials()) {
            GpuMaterial gpuMaterial;
            gpuMaterial.AlbedoFuzz = glm::vec4(material.albedo,material.fuzz);
            gpuMaterial.Optical = glm::vec4(material.indexOfRefraction,0.0f,0.0f,0.0f);
            gpuMaterial.Emission = glm::vec4(material.emission, material.emissionStrength);
            gpuMaterial.Metadata = glm::ivec4(static_cast<int>(material.type),0,0,0);
            result.push_back(gpuMaterial);
        }
        return result;
    }
}

void OpenGLRenderer::Initialize(Window& window) {
    m_Window = &window;
    glDisable(GL_DEPTH_TEST);
    m_EditorCompShader = std::make_unique<Shader>("Renderer/OpenGLRenderer/Shaders/editor.comp");
    m_FinalCompShader = std::make_unique<Shader>("Renderer/OpenGLRenderer/Shaders/FinalRender.comp");
    m_EditorCompShader->use();
    m_EditorCompShader->setInt("uEnvironmentMap", 1);
    m_FinalCompShader->use();
    m_FinalCompShader->setInt("uEnvironmentMap", 1);
}

void OpenGLRenderer::SetScene(const Scene& scene) {
    DestroySceneResources();
    UploadScene(scene);
    UploadEnvironment(scene);
    m_SceneUploaded = true;
    ResetAccumulation();
}

void OpenGLRenderer::UploadScene(const Scene& scene) {
    const std::vector<GpuMaterial> gpuMaterials = BuildGpuMaterials(scene);
    m_GpuScene = BvhBuilder::Build(scene, TlasLeafSize, BlasLeafSize);
    m_MaterialBuffer =CreateStorageBuffer(MaterialBufferBinding,
            gpuMaterials.data(), gpuMaterials.size() * sizeof(GpuMaterial));
    m_SphereBuffer = CreateStorageBuffer(SphereBufferBinding,
            m_GpuScene.Spheres.data(), m_GpuScene.Spheres.size() * sizeof(GpuSphere));
    m_TlasBuffer = CreateStorageBuffer(TlasBufferBinding,
            m_GpuScene.TlasNodes.data(), m_GpuScene.TlasNodes.size() * sizeof(GpuBvhNode));
    m_QuadBuffer = CreateStorageBuffer(QuadBufferBinding,
            m_GpuScene.Quads.data(), m_GpuScene.Quads.size() * sizeof(GpuQuad));
    m_TriangleBuffer = CreateStorageBuffer(TriangleBufferBinding,
            m_GpuScene.Tris.data(),m_GpuScene.Tris.size() * sizeof(GpuTri));
    m_PrimitiveRefBuffer = CreateStorageBuffer(PrimitiveRefBufferBinding,
            m_GpuScene.PrimitiveRefs.data(), m_GpuScene.PrimitiveRefs.size() * sizeof(GpuPrimitiveRef));
    m_TransformBuffer = CreateStorageBuffer(TransformBufferBinding,
            m_GpuScene.Transforms.data(), m_GpuScene.Transforms.size() * sizeof(GpuTransform));
    m_MeshVertexBuffer = CreateStorageBuffer(MeshVertexBufferBinding,
        m_GpuScene.MeshVertices.data(), m_GpuScene.MeshVertices.size() * sizeof(GpuMeshVertex));
    m_MeshTriangleBuffer = CreateStorageBuffer(MeshTriangleBufferBinding,
        m_GpuScene.MeshTriangles.data(), m_GpuScene.MeshTriangles.size() * sizeof(GpuMeshTriangle));
    m_MeshBuffer = CreateStorageBuffer(MeshBufferBinding,
        m_GpuScene.Meshes.data(), m_GpuScene.Meshes.size() * sizeof(GpuMesh));
    m_BlasBuffer = CreateStorageBuffer(BlasBufferBinding,
        m_GpuScene.BlasNodes.data(), m_GpuScene.BlasNodes.size() * sizeof(GpuBvhNode));
}

void OpenGLRenderer::UploadEnvironment(const Scene& scene) {
    const SceneEnvironment& environment =scene.GetEnvironment();

    const auto& textures = scene.GetTextures();

    if (environment.texture >= textures.size()) {
        throw std::out_of_range("Scene environment references invalid TextureId");
    }
    m_EnvironmentTexture = CreateHdrTexture(textures[environment.texture]);
}

void OpenGLRenderer::CreateOutputTexture(std::uint32_t width, std::uint32_t height) {
    DestroyOutputTexture();
    m_RenderWidth = width;
    m_RenderHeight = height;
    glGenTextures(1, &m_OutputTexture);
    glBindTexture(GL_TEXTURE_2D, m_OutputTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    ResetAccumulation();
}

void OpenGLRenderer::Render(const Scene& scene, const EditorCamera& camera, const RenderSettings& settings) {
    if (!m_SceneUploaded) {
        SetScene(scene);
    }
    if (settings.width != m_RenderWidth || settings.height != m_RenderHeight) {
        CreateOutputTexture(settings.width, settings.height);
    }
    if (m_AccumulationSamples >= settings.maxSamples) {
        return;
    }
    DispatchCompute(scene, camera);
    ++m_AccumulationSamples;
}

void OpenGLRenderer::DispatchCompute(const Scene& scene, const EditorCamera& camera) {
    Shader& shader = *m_EditorCompShader;
    shader.use();
    BindSceneBuffers();
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_EnvironmentTexture);
    glBindImageTexture(0, m_OutputTexture, 0, GL_FALSE,
        0, GL_READ_WRITE, GL_RGBA32F);
    shader.setUInt("uFrameIndex", m_AccumulationSamples);
    shader.setInt("uTlasNodeCount", static_cast<int>(m_GpuScene.TlasNodes.size()));
    SetCameraUniforms(camera);
    SetEnvironmentUniforms(scene);
    const GLuint groupCountX = (m_RenderWidth + ComputeLocalSizeX - 1) / ComputeLocalSizeX;
    const GLuint groupCountY = (m_RenderHeight + ComputeLocalSizeY - 1) / ComputeLocalSizeY;
    glDispatchCompute(groupCountX, groupCountY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void OpenGLRenderer::BindSceneBuffers() {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, MaterialBufferBinding, m_MaterialBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SphereBufferBinding, m_SphereBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, TlasBufferBinding, m_TlasBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, QuadBufferBinding, m_QuadBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, TriangleBufferBinding, m_TriangleBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, PrimitiveRefBufferBinding, m_PrimitiveRefBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, TransformBufferBinding, m_TransformBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, MeshVertexBufferBinding, m_MeshVertexBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, MeshTriangleBufferBinding, m_MeshTriangleBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, MeshBufferBinding, m_MeshBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BlasBufferBinding, m_BlasBuffer);
}

void OpenGLRenderer::SetCameraUniforms(const EditorCamera& camera) {
    Shader& shader = *m_EditorCompShader;
    shader.setVec3("uCameraLookFrom", camera.LookFrom);
    shader.setVec3("uCameraLookAt", camera.LookAt);
    shader.setVec3("uCameraVUp", camera.VUp);
    shader.setFloat("uCameraVerticalFov", camera.VerticalFov);
    shader.setFloat("uCameraFocusDistance", camera.FocusDistance);
    shader.setFloat("uCameraDefocusAngle", camera.DefocusAngle);
}

void OpenGLRenderer::SetEnvironmentUniforms(const Scene &scene) {
    Shader& shader = *m_EditorCompShader;
    auto environment = scene.GetEnvironment();
    shader.setFloat("uEnvironmentIntensity", environment.intensity);
    shader.setVec3("uEnvironmentRotation", glm::radians(environment.rotation));
}

void OpenGLRenderer::ResetAccumulation() {
    m_AccumulationSamples = 0;
}

void OpenGLRenderer::Shutdown() {
    DestroySceneResources();
    m_EditorCompShader.reset();
    m_FinalCompShader.reset();
    m_Window = nullptr;
}

void OpenGLRenderer::DestroySceneResources() {
    if (m_MaterialBuffer)
        glDeleteBuffers(1, &m_MaterialBuffer);
    if (m_SphereBuffer)
        glDeleteBuffers(1, &m_SphereBuffer);
    if (m_TlasBuffer)
        glDeleteBuffers(1, &m_TlasBuffer);
    if (m_QuadBuffer)
        glDeleteBuffers(1, &m_QuadBuffer);
    if (m_TriangleBuffer)
        glDeleteBuffers(1, &m_TriangleBuffer);
    if (m_PrimitiveRefBuffer)
        glDeleteBuffers(1, &m_PrimitiveRefBuffer);
    if (m_TransformBuffer)
        glDeleteBuffers(1, &m_TransformBuffer);
    if (m_MeshVertexBuffer)
        glDeleteBuffers(1, &m_MeshVertexBuffer);
    if (m_MeshTriangleBuffer)
        glDeleteBuffers(1, &m_MeshTriangleBuffer);
    if (m_MeshBuffer)
        glDeleteBuffers(1, &m_MeshBuffer);
    if (m_BlasBuffer)
        glDeleteBuffers(1, &m_BlasBuffer);
    if (m_EnvironmentTexture)
        glDeleteTextures(1, &m_EnvironmentTexture);
    m_MaterialBuffer = 0;
    m_SphereBuffer = 0;
    m_TlasBuffer = 0;
    m_QuadBuffer = 0;
    m_TriangleBuffer = 0;
    m_PrimitiveRefBuffer = 0;
    m_TransformBuffer = 0;
    m_MeshVertexBuffer = 0;
    m_MeshTriangleBuffer = 0;
    m_MeshBuffer = 0;
    m_BlasBuffer = 0;
    m_EnvironmentTexture = 0;
    m_SceneUploaded = false;
}

void OpenGLRenderer::Present() {
    if (m_Window) {
        m_Window->SwapBuffers();
    }
}

void OpenGLRenderer::Resize(std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0) {
        return;
    }
    if (width == m_RenderWidth && height == m_RenderHeight) {
        return;
    }
    CreateOutputTexture(width, height);
}

void OpenGLRenderer::DestroyOutputTexture()
{
    if (m_OutputTexture != 0)
    {
        glDeleteTextures(
            1,
            &m_OutputTexture
        );

        m_OutputTexture = 0;
    }

    m_RenderWidth = 0;
    m_RenderHeight = 0;
}

GLuint OpenGLRenderer::CreateStorageBuffer(GLuint binding, const void* data, GLsizeiptr size) {
    GLuint buffer = 0;
    glGenBuffers(1,&buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER,size,data,GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,binding,buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);
    return buffer;
}

GLuint OpenGLRenderer::CreateHdrTexture(const SceneTexture& texture) {
    if (texture.type != SceneTextureType::HDR) {
        throw std::invalid_argument("CreateHdrTexture requires an HDR SceneTexture");
    }
    if (texture.width <= 0 || texture.height <= 0 || texture.hdrPixels.empty()) {
        throw std::invalid_argument("HDR SceneTexture contains no image data");
    }
    GLenum format;
    GLenum internalFormat;
    switch (texture.channels) {
        case 3:
            format = GL_RGB;
            internalFormat = GL_RGB16F;
            break;
        case 4:
            format = GL_RGBA;
            internalFormat = GL_RGBA16F;
            break;
        default:
            throw std::invalid_argument("HDR environment must have 3 or 4 channels");
    }
    GLuint glTexture = 0;
    glGenTextures(1, &glTexture);
    glBindTexture(GL_TEXTURE_2D, glTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D,0,internalFormat, texture.width, texture.height,
        0, format, GL_FLOAT, texture.hdrPixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, 0);
    return glTexture;
}
