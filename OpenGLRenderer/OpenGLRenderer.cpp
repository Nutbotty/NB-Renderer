//
// Created by Nutbotty on 7/20/2026.
//
#include "OpenGLRenderer.h"
#include "Shaders/shader.h"
#include "Window.h"
#include <iostream>
#include <algorithm>
#include <limits>
#include <vector>
#include "BvhBuilder.h"

namespace {
    constexpr int RenderWidth = 2560;
    constexpr int RenderHeight = 1440;

    constexpr unsigned int ComputeLocalSizeX = 16;
    constexpr unsigned int ComputeLocalSizeY = 16;

    constexpr GLuint MaterialBufferBinding = 1;
    constexpr GLuint SphereBufferBinding = 2;
    constexpr GLuint BvhBufferBinding = 3;

    /*
     * best results with size 4 so far, 8 good too
     */
    constexpr std::uint32_t BvhLeafSize = 8;
}

std::vector<GpuMaterial>
BuildGpuMaterials(const Scene &scene) {
    std::vector<GpuMaterial> result;

    result.reserve(scene.GetMaterials().size());

    for (const SceneMaterial &material: scene.GetMaterials()) {
        GpuMaterial gpuMaterial;

        gpuMaterial.AlbedoFuzz =
                glm::vec4(
                    material.albedo,
                    material.fuzz
                );

        gpuMaterial.Optical =
                glm::vec4(
                    material.indexOfRefraction,
                    0.0f,
                    0.0f,
                    0.0f
                );

        gpuMaterial.Metadata =
                glm::ivec4(
                    static_cast<int>(material.type),
                    0,
                    0,
                    0
                );

        result.push_back(gpuMaterial);
    }

    return result;
}

GLuint CreateStorageBuffer(GLuint binding, const void *data, GLsizeiptr size) {
    GLuint buffer = 0;

    glGenBuffers(
        1,
        &buffer
    );

    glBindBuffer(
        GL_SHADER_STORAGE_BUFFER,
        buffer
    );

    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        size,
        data,
        GL_STATIC_DRAW
    );

    glBindBufferBase(
        GL_SHADER_STORAGE_BUFFER,
        binding,
        buffer
    );

    glBindBuffer(
        GL_SHADER_STORAGE_BUFFER,
        0
    );

    return buffer;
}

int Renderer(const Scene &scene) {
    const std::vector<GpuMaterial> gpuMaterials = BuildGpuMaterials(scene);
    const BvhBuildResult gpuBvh = BvhBuilder::Build(scene,8);
    const std::vector<GpuSphere> &gpuSpheres = gpuBvh.Spheres;
    const std::vector<GpuBvhNode> &gpuBvhNodes = gpuBvh.Nodes;

    Window window(RenderWidth, RenderHeight, "Viewport");

    if (window.Initialize() != 0)
        return -1;

    Shader computeShader("OpenGLRenderer/Shaders/render.comp");
    Shader fullscreenShader("OpenGLRenderer/Shaders/render.vs", "OpenGLRenderer/Shaders/render.fs");
    const GLuint materialBuffer = CreateStorageBuffer(
        MaterialBufferBinding,
        gpuMaterials.data(),
        static_cast<GLsizeiptr>(
            gpuMaterials.size()
            * sizeof(GpuMaterial)
        )
    );
    const GLuint sphereBuffer = CreateStorageBuffer(
        SphereBufferBinding,
        gpuSpheres.data(),
        static_cast<GLsizeiptr>(
            gpuSpheres.size()
            * sizeof(GpuSphere)
        )
    );
    const GLuint bvhBuffer =
            CreateStorageBuffer(
                BvhBufferBinding,
                gpuBvhNodes.data(),
                static_cast<GLsizeiptr>(
                    gpuBvhNodes.size()
                    * sizeof(GpuBvhNode)
                )
            );
    /*
     * Create the texture that the compute shader will write into.
     */
    GLuint outputTexture = 0;

    glGenTextures(1, &outputTexture);
    glBindTexture(GL_TEXTURE_2D, outputTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1,GL_RGBA32F, RenderWidth, RenderHeight);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindImageTexture(0, outputTexture, 0,GL_FALSE, 0,GL_WRITE_ONLY,GL_RGBA32F);

    const GLuint groupCountX = (RenderWidth + ComputeLocalSizeX - 1) / ComputeLocalSizeX;
    const GLuint groupCountY = (RenderHeight + ComputeLocalSizeY - 1) / ComputeLocalSizeY;

    GLuint timerQuery = 0;
    glGenQueries(1, &timerQuery);

    const SceneCamera &camera = scene.GetCamera();
    computeShader.use();
    computeShader.setInt("uBvhNodeCount", static_cast<int>(gpuBvhNodes.size()));
    computeShader.setVec3("uCameraLookFrom", camera.lookFrom);
    computeShader.setVec3("uCameraLookAt", camera.lookAt);
    computeShader.setVec3("uCameraVUp", camera.up);
    computeShader.setFloat("uCameraVerticalFov", camera.verticalFovDegrees);
    computeShader.setFloat("uCameraFocusDistance", camera.focusDistance);
    computeShader.setFloat("uCameraDefocusAngle", camera.defocusAngleDegrees);


    glDispatchCompute(groupCountX, groupCountY, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    glFinish();

    /*
     * Measure only GPU execution.
     */
    glBeginQuery(GL_TIME_ELAPSED, timerQuery);
    glDispatchCompute(groupCountX, groupCountY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    glEndQuery(GL_TIME_ELAPSED);
    GLuint64 elapsedNanoseconds = 0;
    glGetQueryObjectui64v(timerQuery,GL_QUERY_RESULT, &elapsedNanoseconds);
    const double elapsedMilliseconds = static_cast<double>(elapsedNanoseconds) / 1'000'000.0;
    std::cerr << "Compute time: " << elapsedMilliseconds << " ms\n";
    glDeleteQueries(1, &timerQuery);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    GLuint fullscreenVAO = 0;
    glGenVertexArrays(1, &fullscreenVAO);

    fullscreenShader.use();
    fullscreenShader.setInt("outputTexture", 0);
    std::cerr << "Compute program: " << computeShader.ID << '\n';
    std::cerr << "Fullscreen program: " << fullscreenShader.ID << '\n';
    std::cerr << "Compute valid: " << glIsProgram(computeShader.ID) << '\n';
    std::cerr << "Fullscreen valid: " << glIsProgram(fullscreenShader.ID) << '\n';

    while (!window.ShouldClose()) {
        glfwPollEvents();

        int framebufferWidth = 0;
        int framebufferHeight = 0;

        glfwGetFramebufferSize(window.GetNativeWindow(), &framebufferWidth, &framebufferHeight);
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

        glClear(GL_COLOR_BUFFER_BIT);

        fullscreenShader.use();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, outputTexture);

        glBindVertexArray(fullscreenVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window.GetNativeWindow());
    }

    glDeleteVertexArrays(1, &fullscreenVAO);
    glDeleteTextures(1, &outputTexture);
    glDeleteBuffers(1, &materialBuffer);
    glDeleteBuffers(1, &sphereBuffer);
    glDeleteBuffers(1, &bvhBuffer);
    glDeleteProgram(computeShader.ID);
    glDeleteProgram(fullscreenShader.ID);
    glfwDestroyWindow(window.GetNativeWindow());
    glfwTerminate();

    return 0;
}
