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
    constexpr GLuint InstanceMaterialBufferBinding = 12;

    constexpr GLint EnvironmentTextureUnit = 1;
    constexpr GLint BaseColorTextureUnit = 2;
    constexpr GLint MetalRoughTextureUnit = 3;
    constexpr GLsizei PbrTextureWidth = 1024;
    constexpr GLsizei PbrTextureHeight = 1024;


    constexpr std::uint32_t TlasLeafSize = 8;
    constexpr std::uint32_t BlasLeafSize = 4;

    constexpr std::uint32_t ComputeLocalSizeX = 16;
    constexpr std::uint32_t ComputeLocalSizeY = 16;

    std::vector<std::uint8_t>
ResizeRgba8Nearest(const SceneTexture& texture, int targetWidth, int targetHeight) {
    if (texture.width <= 0 || texture.height <= 0 || texture.channels != 4) {
        throw std::runtime_error("ResizeRgba8Nearest requires RGBA8 texture");
    }
    const std::size_t expectedSize = static_cast<std::size_t>(texture.width) * static_cast<std::size_t>(texture.height) * 4;
    if (texture.pixels.size() < expectedSize) {
        throw std::runtime_error("SceneTexture pixel data is incomplete");
    }
    if (texture.width == targetWidth && texture.height == targetHeight) {
        return texture.pixels;
    }
    std::vector<std::uint8_t> output(static_cast<std::size_t>(targetWidth) * static_cast<std::size_t>(targetHeight) * 4);
    for (int y = 0; y < targetHeight; ++y) {
        const int sourceY = std::min(texture.height - 1, y * texture.height / targetHeight);
        for (int x = 0; x < targetWidth; ++x) {
            const int sourceX = std::min(texture.width - 1, x * texture.width / targetWidth);
            const std::size_t src = (static_cast<std::size_t>(sourceY) * texture.width + sourceX) * 4;
            const std::size_t dst = (static_cast<std::size_t>(y) * targetWidth + x) * 4;
            output[dst + 0] = texture.pixels[src + 0];
            output[dst + 1] = texture.pixels[src + 1];
            output[dst + 2] = texture.pixels[src + 2];
            output[dst + 3] = texture.pixels[src + 3];
        }
    }

    return output;
}
    int GetTextureLayer(TextureId textureId, const std::vector<int>& layerMap) {
        if (textureId == InvalidTextureId) {
            return -1;
        }
        const std::size_t index = static_cast<std::size_t>(textureId);
        if (index >= layerMap.size()) {
            return -1;
        }
        return layerMap[index];
    }
    std::vector<GpuMaterial> BuildGpuMaterials(const Scene& scene, const std::vector<int>& baseColorLayers,
    const std::vector<int>& metalRoughLayers) {
        std::vector<GpuMaterial> result;
        const auto& materials = scene.GetMaterials();
        result.reserve(materials.size());
        for (const SceneMaterial& material : materials) {
            GpuMaterial gpuMat{};
            gpuMat.BaseColor = material.baseColor;
            gpuMat.Surface = glm::vec4(material.metallic, material.roughness, material.indexOfRefraction, material.transmission);
            gpuMat.Emission = glm::vec4(material.emission, material.emissionStrength);
            gpuMat.Metadata = glm::ivec4(static_cast<int>(material.type),0,0,0);
            gpuMat.TextureIndices = glm::ivec4(GetTextureLayer(material.baseColorTexture, baseColorLayers),
                GetTextureLayer(material.metallicRoughnessTexture, metalRoughLayers),-1, -1);
            const int baseLayer = GetTextureLayer(material.baseColorTexture, baseColorLayers);
            const int mrLayer = GetTextureLayer(material.metallicRoughnessTexture, metalRoughLayers);
            std::cerr << "Material " << result.size() << ": Scene base TextureId=" << (
                    material.baseColorTexture == InvalidTextureId ? -1 : static_cast<int>(material.baseColorTexture))
                << " -> GPU base layer=" << baseLayer << ", Scene MR TextureId="<< (
                    material.metallicRoughnessTexture == InvalidTextureId ? -1 : static_cast<int>(material.metallicRoughnessTexture))
            << " -> GPU MR layer=" << mrLayer << '\n';
            gpuMat.TextureIndices = glm::ivec4(baseLayer, mrLayer, -1, -1);
            std::cerr << "Material texture layers: base=" << gpuMat.TextureIndices.x << " mr=" << gpuMat.TextureIndices.y << '\n';
            result.push_back(gpuMat);
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
    m_EditorCompShader->setInt("uEnvironmentMap", EnvironmentTextureUnit);
    m_EditorCompShader->setInt("uBaseColorTextures", BaseColorTextureUnit);
    m_EditorCompShader->setInt("uMetalRoughTextures", MetalRoughTextureUnit);
    m_FinalCompShader->use();
    m_FinalCompShader->setInt("uEnvironmentMap", EnvironmentTextureUnit);
    m_FinalCompShader->setInt("uBaseColorTextures", BaseColorTextureUnit);
    m_FinalCompShader->setInt("uMetalRoughTextures", MetalRoughTextureUnit);
}

void OpenGLRenderer::SetScene(const Scene& scene) {
    DestroySceneResources();
    UploadMaterialTextures(scene);
    UploadScene(scene);
    UploadEnvironment(scene);
    m_SceneUploaded = true;
    ResetAccumulation();
}

void OpenGLRenderer::UploadScene(const Scene& scene) {
    const std::vector<GpuMaterial> gpuMaterials = BuildGpuMaterials(scene, m_BaseColorLayerByTexture, m_MetalRoughLayerByTexture);
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
    m_InstanceMaterialBuffer = CreateStorageBuffer(InstanceMaterialBufferBinding, m_GpuScene.InstanceMaterials.data(),
        static_cast<GLsizeiptr>(m_GpuScene.InstanceMaterials.size() * sizeof(std::int32_t)));
}

void OpenGLRenderer::UploadEnvironment(const Scene& scene) {
    const SceneEnvironment& environment =scene.GetEnvironment();

    const auto& textures = scene.GetTextures();

    if (environment.texture >= textures.size()) {
        throw std::out_of_range("Scene environment references invalid TextureId");
    }
    m_EnvironmentTexture = CreateHdrTexture(textures[environment.texture]);
}

void OpenGLRenderer::UpdateMaterials(const Scene& scene) {
    const std::vector<GpuMaterial> materials = BuildGpuMaterials(scene, m_BaseColorLayerByTexture, m_MetalRoughLayerByTexture);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_MaterialBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(materials.size() *
        sizeof(GpuMaterial) ), materials.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, MaterialBufferBinding, m_MaterialBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    ResetAccumulation();
}

void OpenGLRenderer::UpdateObjectMaterials(const Scene& scene, std::size_t objectIndex) {
    const auto& instances = scene.GetMeshInstances();
    const SceneMeshInstance& instance = instances[objectIndex];
    const SceneMesh& mesh = scene.GetMeshes().at(static_cast<std::size_t>(instance.mesh));
    const std::uint32_t materialOffset = m_GpuScene.MeshInstanceMaterialOffsets[objectIndex];

    std::vector<std::int32_t> gpuMaterials;
    gpuMaterials.reserve(instance.materials.size());
    for (const MaterialId material : instance.materials) {
        gpuMaterials.push_back(static_cast<std::int32_t>(material));
    }
    if (!gpuMaterials.empty()) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_InstanceMaterialBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, static_cast<GLintptr>(materialOffset * sizeof(std::int32_t)),
            static_cast<GLsizeiptr>(gpuMaterials.size() * sizeof(std::int32_t)), gpuMaterials.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    for (std::size_t slot = 0; slot < gpuMaterials.size(); ++slot) {
        m_GpuScene.InstanceMaterials[materialOffset + slot] = gpuMaterials[slot];
    }
    ResetAccumulation();
}

void OpenGLRenderer::UploadMaterialTextures(const Scene& scene) {
    const auto& textures = scene.GetTextures();
    m_BaseColorLayerByTexture.assign(textures.size(), -1);
    m_MetalRoughLayerByTexture.assign(textures.size(), -1);
    std::vector<TextureId> baseColorTextures;
    std::vector<TextureId> metalRoughTextures;

    auto registerTexture = [&](TextureId textureId, std::vector<int>& layerMap, std::vector<TextureId>& layers) {
            if (textureId == InvalidTextureId) {
                return;
            }
            const std::size_t index = static_cast<std::size_t>(textureId);
            if (index >= textures.size()) {
                throw std::out_of_range("Material references invalid TextureId");
            }
            if (layerMap[index] >= 0) {
                return;
            }
            const SceneTexture& texture = textures[index];
            if (texture.type != SceneTextureType::Image2D) {
                throw std::runtime_error("PBR material texture must be LDR");
            }
            const int layer = static_cast<int>(layers.size());
            layerMap[index] = layer;
            layers.push_back(textureId);
        };
    for (const SceneMaterial& material : scene.GetMaterials()) {
        registerTexture(material.baseColorTexture, m_BaseColorLayerByTexture, baseColorTextures);
        registerTexture(material.metallicRoughnessTexture, m_MetalRoughLayerByTexture, metalRoughTextures);
    }
    m_BaseColorTextureArray = CreatePbrTextureArray(scene, baseColorTextures, GL_SRGB8_ALPHA8);
    m_MetalRoughTextureArray = CreatePbrTextureArray(scene, metalRoughTextures, GL_RGBA8);
    std::cerr << "Uploaded PBR texture arrays:\n" << "  base color layers: " << baseColorTextures.size() << '\n'
        << "  metal/rough layers: " << metalRoughTextures.size() << '\n';
}

GLuint OpenGLRenderer::CreatePbrTextureArray(const Scene& scene, const std::vector<TextureId>& layerTextures, GLenum internalFormat) {
    if (layerTextures.empty()) {
        return 0;
    }
    const auto& textures = scene.GetTextures();
    GLint maxLayers = 0;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
    if (static_cast<GLint>(layerTextures.size()) > maxLayers) {
        throw std::runtime_error("Too many material textures for " "GL_TEXTURE_2D_ARRAY");
    }
    GLint maxTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    if (PbrTextureWidth > maxTextureSize || PbrTextureHeight > maxTextureSize) {
        throw std::runtime_error("PBR texture-array resolution exceeds " "GL_MAX_TEXTURE_SIZE");
    }
    GLuint arrayTexture = 0;
    glGenTextures(1, &arrayTexture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, arrayTexture);
     // fixed dimensions 1024 x 1024 for now. currently only one mip map layer
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, internalFormat, PbrTextureWidth,
        PbrTextureHeight, static_cast<GLsizei>(layerTextures.size()));
    GLint previousUnpackAlignment = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (std::size_t layer = 0; layer < layerTextures.size(); ++layer) {
        const TextureId textureId = layerTextures[layer];
        const std::size_t textureIndex = static_cast<std::size_t>(textureId);
        if (textureIndex >= textures.size()) {
            throw std::runtime_error("Invalid Scene TextureId while " "building texture array");
        }
        const SceneTexture& texture = textures[textureIndex];
        if (texture.type != SceneTextureType::Image2D) {
            throw std::runtime_error("Attempted to upload non-LDR texture " "to PBR texture array");
        }
        const auto pixels = ResizeRgba8Nearest(texture, PbrTextureWidth, PbrTextureHeight);
        // z-offset == array layer, depth == 1 means upload exactly one layer
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, static_cast<GLint>(layer),
            PbrTextureWidth, PbrTextureHeight, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    return arrayTexture;
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
    glClearColor(0.1f, 0.4f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
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
    glActiveTexture(GL_TEXTURE0 + EnvironmentTextureUnit);
    glBindTexture(GL_TEXTURE_2D, m_EnvironmentTexture);
    glActiveTexture(GL_TEXTURE0 + BaseColorTextureUnit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_BaseColorTextureArray);
    glActiveTexture(GL_TEXTURE0 + MetalRoughTextureUnit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_MetalRoughTextureArray);
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
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, InstanceMaterialBufferBinding, m_InstanceMaterialBuffer);
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
    shader.setBool("uUseHdri", environment.HDRI);
    shader.setFloat("uEnvironmentIntensity", environment.intensity);
    shader.setVec3("uEnvironmentRotation", glm::radians(environment.rotation));
    shader.setVec3("uSkyColor1", environment.color1);
    shader.setVec3("uSkyColor2", environment.color2);
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
    if (m_InstanceMaterialBuffer)
        glDeleteBuffers(1, &m_InstanceMaterialBuffer);
    if (m_EnvironmentTexture)
        glDeleteTextures(1, &m_EnvironmentTexture);
    if (m_BaseColorTextureArray)
        glDeleteTextures(1, &m_BaseColorTextureArray);
    if (m_MetalRoughTextureArray)
        glDeleteTextures(1, &m_MetalRoughTextureArray);
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
    m_InstanceMaterialBuffer = 0;
    m_EnvironmentTexture = 0;
    m_BaseColorTextureArray = 0;
    m_MetalRoughTextureArray = 0;
    m_BaseColorLayerByTexture.clear();
    m_MetalRoughLayerByTexture.clear();
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

void OpenGLRenderer::DestroyOutputTexture() {
    if (m_OutputTexture != 0) {
        glDeleteTextures(1, &m_OutputTexture);
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
