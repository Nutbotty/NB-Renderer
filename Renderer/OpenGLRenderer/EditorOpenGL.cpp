//
// Created by Nutbotty on 7/17/2026.
//
//TODO Refactor all of this
#include "EditorOpenGL.h"
#include <iostream>
#include "Shaders/shader.h"
#include "../../Editor/editorcamera.h"
#include "../../external/glm/glm/gtc/matrix_transform.hpp"
#include "../../Core/Window.h"
#include "../../Core/Input.h"
#include "../../Core/BvhBuilder.h"

namespace {
// settings
    const unsigned int SCR_WIDTH = 2560;
    const unsigned int SCR_HEIGHT = 1440;
    const unsigned int RENDER_WIDTH = 1280;
    const unsigned int RENDER_HEIGHT = 720;
    bool TRACE = true;
    constexpr float EPSILON = 1e-8f;

    constexpr GLuint MaterialBufferBinding = 1;
    constexpr GLuint SphereBufferBinding = 2;
    constexpr GLuint TlasBufferBinding = 3;
    constexpr GLuint QuadBufferBinding = 4;
    constexpr GLuint TriangleBufferBinding = 5;
    constexpr GLuint PrimitiveRefBufferBinding = 6;
    constexpr GLuint TransformBufferBinding = 7;
    constexpr GLuint MeshVertexBufferBinding   = 8;
    constexpr GLuint MeshTriangleBufferBinding = 9;
    constexpr GLuint MeshBufferBinding         = 10;
    constexpr GLuint BlasBufferBinding         = 11;
    constexpr std::uint32_t TlasLeafSize = 8;
    constexpr std::uint32_t BlasLeafSize = 4;
    constexpr unsigned int ComputeLocalSizeX = 16;
    constexpr unsigned int ComputeLocalSizeY = 16;
    const GLuint groupCountX = (RENDER_WIDTH + ComputeLocalSizeX - 1) / ComputeLocalSizeX;
    const GLuint groupCountY = (RENDER_HEIGHT + ComputeLocalSizeY - 1)/ ComputeLocalSizeY;

    // timing
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    std::vector<GpuMaterial> BuildGpuMaterials(const Scene& scene) {
        std::vector<GpuMaterial> result;
        result.reserve(scene.GetMaterials().size());

        for (const SceneMaterial& material :scene.GetMaterials()) {
            GpuMaterial gpuMaterial;
            gpuMaterial.BaseColor = material.baseColor;
            gpuMaterial.Surface = glm::vec4(material.metallic, material.roughness, material.indexOfRefraction, material.transmission);
            gpuMaterial.Emission = glm::vec4(material.emission, material.emissionStrength);
            gpuMaterial.Metadata = glm::ivec4(static_cast<int>(material.type),0,0,0);
            gpuMaterial.TextureIndices = glm::ivec4(
                material.baseColorTexture == InvalidTextureId ? -1 : static_cast<int>(material.baseColorTexture),
                material.metallicRoughnessTexture== InvalidTextureId ? -1 : static_cast<int>(material.metallicRoughnessTexture), -1, -1);

            gpuMaterial.AlbedoFuzz = glm::vec4(material.albedo,material.fuzz);
            gpuMaterial.Optical = glm::vec4(material.indexOfRefraction,0.0f,0.0f,0.0f);
            result.push_back(gpuMaterial);
        }
        return result;
    }

    GLuint CreateStorageBuffer(GLuint binding, const void* data, GLsizeiptr size) {
    GLuint buffer = 0;
    glGenBuffers(1,&buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER,size,data,GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,binding,buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);
    return buffer;
}

    GLuint CreateHdrTexture(const SceneTexture& texture) {
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

    struct CameraState {
    glm::vec3 Position{0.0f};
    glm::vec3 Front{0.0f};
    glm::vec3 Up{0.0f};
    float Zoom = 20.0f;
    float FocusDistance = 0.0f;
    float DefocusAngle = 0.0f;
};

    CameraState CaptureCameraState(const EditorCamera& editorCamera) {
    CameraState state;
    state.Position = editorCamera.LookFrom;
    state.Front = editorCamera.LookAt;
    state.Up = editorCamera.VUp;
    state.Zoom = editorCamera.VerticalFov;
    state.FocusDistance = editorCamera.FocusDistance;
    state.DefocusAngle = editorCamera.DefocusAngle;
    return state;
}

    bool NearlyEqual(const glm::vec3& left, const glm::vec3& right) {
        const glm::vec3 difference = left - right;
        return glm::dot(difference,difference) <= EPSILON * EPSILON;
    }
    bool NearlyEqual(float left, float right) {
        return std::abs(left - right) <= EPSILON;
    }
    bool CameraStateChanged(const CameraState& previous, const CameraState& current) {
        return
            !NearlyEqual(previous.Position, current.Position) ||
            !NearlyEqual(previous.Front, current.Front) ||
            !NearlyEqual(previous.Up, current.Up) ||
            !NearlyEqual(previous.Zoom, current.Zoom) ||
            !NearlyEqual(previous.FocusDistance, current.FocusDistance) ||
            !NearlyEqual(previous.DefocusAngle,current.DefocusAngle);
    }

}

void viewport(const Scene& scene) {
    const std::vector<GpuMaterial> gpuMaterials = BuildGpuMaterials(scene);
    const BvhBuildResult gpuBvh = BvhBuilder::Build(scene,TlasLeafSize, BlasLeafSize);
    const std::vector<GpuSphere> &gpuSpheres = gpuBvh.Spheres;
    const std::vector<GpuBvhNode> &gpuTlasNodes = gpuBvh.TlasNodes;

    EditorCamera camera(scene.GetCamera());
    Window window(SCR_WIDTH, SCR_HEIGHT, "Viewport");
    if (window.Initialize() != 0) return;


    const SceneEnvironment& environment = scene.GetEnvironment();
    const auto& sceneTextures = scene.GetTextures();
    if (environment.texture>= sceneTextures.size()) {
        throw std::out_of_range("Scene environment references invalid TextureId");
    }
    const SceneTexture& environmentTextureData = sceneTextures[environment.texture];
    const GLuint environmentTexture = CreateHdrTexture(environmentTextureData);




    Input input(camera);
    input.Initialize(window.GetNativeWindow());
    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);
    // build and compile our shader program
    // ------------------------------------
    Shader ourShader("Renderer/OpenGLRenderer/Shaders/shader.vs", "Renderer/OpenGLRenderer/Shaders/shader.fs"); // you can name your shader files however you like
    Shader computeShader("Renderer/OpenGLRenderer/Shaders/editor.comp");
    Shader fullscreenShader("Renderer/OpenGLRenderer/Shaders/render.vs","Renderer/OpenGLRenderer/Shaders/render.fs");
    const GLuint materialBuffer = CreateStorageBuffer(MaterialBufferBinding, gpuMaterials.data(),
        gpuMaterials.size() * sizeof(GpuMaterial));
    const GLuint sphereBuffer = CreateStorageBuffer(SphereBufferBinding,gpuSpheres.data(),
        gpuSpheres.size() * sizeof(GpuSphere));
    const GLuint tlasBuffer = CreateStorageBuffer(TlasBufferBinding, gpuTlasNodes.data(),
            static_cast<GLsizeiptr>(gpuTlasNodes.size() * sizeof(GpuBvhNode)));
    const GLuint quadBuffer = CreateStorageBuffer(QuadBufferBinding,gpuBvh.Quads.data(),
        static_cast<GLsizeiptr>(gpuBvh.Quads.size()* sizeof(GpuQuad)));
    const GLuint triangleBuffer = CreateStorageBuffer(TriangleBufferBinding,
        gpuBvh.Tris.data(),static_cast<GLsizeiptr>(gpuBvh.Tris.size()* sizeof(GpuTri)));
    const GLuint primitiveRefBuffer = CreateStorageBuffer(PrimitiveRefBufferBinding,
        gpuBvh.PrimitiveRefs.data(),static_cast<GLsizeiptr>(gpuBvh.PrimitiveRefs.size()* sizeof(GpuPrimitiveRef)));
    const GLuint transformBuffer = CreateStorageBuffer(TransformBufferBinding,
        gpuBvh.Transforms.data(), static_cast<GLsizeiptr>(gpuBvh.Transforms.size() * sizeof(GpuTransform)));
    const GLuint meshVertexBuffer = CreateStorageBuffer(MeshVertexBufferBinding,
        gpuBvh.MeshVertices.data(), static_cast<GLsizeiptr>(gpuBvh.MeshVertices.size() * sizeof(GpuMeshVertex)));
    const GLuint meshTriangleBuffer = CreateStorageBuffer(MeshTriangleBufferBinding,
        gpuBvh.MeshTriangles.data(), static_cast<GLsizeiptr>(gpuBvh.MeshTriangles.size() * sizeof(GpuMeshTriangle)));
    const GLuint meshBuffer = CreateStorageBuffer(MeshBufferBinding,
        gpuBvh.Meshes.data(), static_cast<GLsizeiptr>(gpuBvh.Meshes.size() * sizeof(GpuMesh)));
    const GLuint blasBuffer = CreateStorageBuffer(BlasBufferBinding,
        gpuBvh.BlasNodes.data(), static_cast<GLsizeiptr>(gpuBvh.BlasNodes.size() * sizeof(GpuBvhNode)));

    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------

    const float X = 0.525731112119133606f;
    const float Z = 0.850650808352039932f;

    // Position (x,y,z) + Color (r,g,b)
    float vertices[] =
    {
        -X, 0,  Z,   1,0,0,
         X, 0,  Z,   0,1,0,
        -X, 0, -Z,   0,0,1,
         X, 0, -Z,   1,1,0,

         0,  Z,  X,  1,0,1,
         0,  Z, -X,  0,1,1,
         0, -Z,  X,  1,1,1,
         0, -Z, -X,  0.5f,0.5f,0.5f,

         Z,  X, 0,   1,0.5f,0,
        -Z,  X, 0,   0,0.5f,1,
         Z, -X, 0,   0.5f,1,0,
        -Z, -X, 0,   1,0,0.5f
    };

    unsigned int indices[] =
    {
        0,4,1,
        0,9,4,
        9,5,4,
        4,5,8,
        4,8,1,

        8,10,1,
        8,3,10,
        5,3,8,
        5,2,3,
        2,7,3,

        7,10,3,
        7,6,10,
        7,11,6,
        11,0,6,
        0,1,6,

        6,1,10,
        9,0,11,
        9,11,2,
        9,2,5,
        7,2,11
   };

    unsigned int VAO, VBO, EBO;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        6 * sizeof(float),
        (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE,
        6 * sizeof(float),
        (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    GLuint outputTexture;
    GLuint fullscreenVAO;
    glGenTextures(1, &outputTexture);
    glBindTexture(GL_TEXTURE_2D, outputTexture);
    glGenVertexArrays(1, &fullscreenVAO);


    glTexStorage2D(GL_TEXTURE_2D, 1,GL_RGBA32F, RENDER_WIDTH, RENDER_HEIGHT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindImageTexture(0,outputTexture,0,GL_FALSE,0,GL_READ_WRITE,GL_RGBA32F);

    fullscreenShader.use();
    fullscreenShader.setInt("outputTexture", 0);

    computeShader.use();
    computeShader.setInt("uEnvironmentMap",1);

    CameraState previousCameraState = CaptureCameraState(camera);
    bool previousTraceMode = TRACE;
    std::uint32_t accumulationFrame = 0;
    constexpr std::uint32_t MaxAccumulationFrames = 4096;

    while (!window.ShouldClose()) {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        // input.Update(deltaTime);

        const CameraState currentCameraState = CaptureCameraState(camera);
        const bool cameraChanged = CameraStateChanged(previousCameraState, currentCameraState);
        const bool traceModeChanged = TRACE != previousTraceMode;

        if (cameraChanged || traceModeChanged)
        {
            accumulationFrame = 0;
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (TRACE) {
            glDisable(GL_DEPTH_TEST);
            if (accumulationFrame< MaxAccumulationFrames) {
                computeShader.use();
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER,MaterialBufferBinding,materialBuffer);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER,SphereBufferBinding,sphereBuffer);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER,TlasBufferBinding,tlasBuffer);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER,QuadBufferBinding,quadBuffer);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER,TriangleBufferBinding,triangleBuffer);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER,PrimitiveRefBufferBinding,primitiveRefBuffer);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER,TransformBufferBinding,transformBuffer);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, MeshVertexBufferBinding, meshVertexBuffer);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, MeshTriangleBufferBinding, meshTriangleBuffer);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, MeshBufferBinding, meshBuffer);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BlasBufferBinding, blasBuffer);

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, environmentTexture);
                glBindImageTexture(0, outputTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

                computeShader.setUInt("uFrameIndex", accumulationFrame);
                computeShader.setInt("uTlasNodeCount", static_cast<int>(gpuTlasNodes.size()));
                computeShader.setVec3("uCameraLookFrom",camera.LookFrom);
                computeShader.setVec3("uCameraLookAt",camera.LookAt);
                computeShader.setVec3("uCameraVUp",camera.VUp);
                computeShader.setFloat("uCameraVerticalFov",camera.VerticalFov);
                computeShader.setFloat("uCameraFocusDistance", camera.FocusDistance);
                computeShader.setFloat("uCameraDefocusAngle", camera.DefocusAngle);
                computeShader.setFloat("uEnvironmentIntensity", environment.intensity);
                computeShader.setVec3("uEnvironmentRotation", glm::radians(environment.rotation));

                glDispatchCompute(groupCountX, groupCountY, 1);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
                ++accumulationFrame;
            }
            fullscreenShader.use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, outputTexture);
            glBindVertexArray(fullscreenVAO);
            glDrawArrays(GL_TRIANGLES,0,3);
            const CameraState currentCameraState =
            CaptureCameraState(camera);

            const bool cameraChanged =
                CameraStateChanged(
                    previousCameraState,
                    currentCameraState
                );

            const bool traceModeChanged =
                TRACE != previousTraceMode;
            if (cameraChanged || traceModeChanged)
            {
                accumulationFrame = 0;
            }
        }
        else if (!TRACE) {
            // render
            // ------
            glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // render the triangle
            ourShader.use();

            // pass projection matrix to shader (note that in this case it could change every frame)
            glm::mat4 projection = glm::perspective(glm::radians(camera.VerticalFov), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
            ourShader.setMat4("projection", projection);

            // camera/view transformation
            glm::mat4 view = camera.GetViewMatrix();
            ourShader.setMat4("view", view);
            ourShader.setVec3("cameraPosition",camera.LookFrom);
            ourShader.setFloat("focusDistance",camera.FocusDistance);
            ourShader.setFloat("defocusAngle",camera.DefocusAngle);
            ourShader.setVec3("cameraFront",camera.LookAt);
            // render boxes
            glBindVertexArray(VAO);

            const auto& spheres = scene.GetSpheres();

            const auto& materials = scene.GetMaterials();

            for (const SceneSphere& sphere : spheres) {
                glm::mat4 model{1.0f};
                model = glm::translate(model,sphere.center);
                model = glm::scale(model,glm::vec3(2.0f * sphere.radius));
                ourShader.setMat4("model",model);
                const SceneMaterial& material = materials[sphere.material];
                ourShader.setVec3("objectColor",material.albedo);
                glDrawArrays(GL_TRIANGLES, 0, 3);;
            }
        }
        previousCameraState =
        currentCameraState;

        previousTraceMode =
            TRACE;
        window.SwapBuffers();
        window.PollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &fullscreenVAO);
    glDeleteTextures(1, &outputTexture);
    glDeleteTextures(1, &environmentTexture);
    glDeleteBuffers(1, &materialBuffer);
    glDeleteBuffers(1, &sphereBuffer);
    glDeleteBuffers(1, &tlasBuffer);
    glDeleteBuffers(1, &quadBuffer);
    glDeleteBuffers(1, &triangleBuffer);
    glDeleteBuffers(1, &primitiveRefBuffer);
    glDeleteBuffers(1, &transformBuffer);
    glDeleteBuffers(1, &meshVertexBuffer);
    glDeleteBuffers(1, &meshTriangleBuffer);
    glDeleteBuffers(1, &meshBuffer);
    glDeleteBuffers(1, &blasBuffer);
    glDeleteProgram(computeShader.ID);
    glDeleteProgram(fullscreenShader.ID);
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    return;
}