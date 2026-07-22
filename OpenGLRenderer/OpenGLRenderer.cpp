//
// Created by Nutbotty on 7/20/2026.
//
#include "OpenGLRenderer.h"
#include "Shaders/shader.h"
#include "Window.h"
#include <iostream>

namespace
{
    constexpr int RenderWidth = 640;
    constexpr int RenderHeight = 360;

    constexpr unsigned int ComputeLocalSizeX = 16;
    constexpr unsigned int ComputeLocalSizeY = 16;

    constexpr GLuint MaterialBufferBinding = 1;
    constexpr GLuint SphereBufferBinding = 2;

    struct alignas(16) GpuMaterial {
        glm::vec4 AlbedoFuzz{0.0f};
        glm::vec4 Optical{0.0f};
        glm::ivec4 Metadata{0};
    };

    struct alignas(16) GpuSphere {
        glm::vec4 CenterRadius{0.0f};
        glm::ivec4 Metadata{0};
    };

    static_assert(
        sizeof(GpuMaterial) == 48,
        "GpuMaterial layout does not match GLSL"
    );

    static_assert(
        sizeof(GpuSphere) == 32,
        "GpuSphere layout does not match GLSL"
    );
}

std::vector<GpuMaterial>
BuildGpuMaterials(const Scene& scene)
{
    std::vector<GpuMaterial> result;

    result.reserve(
        scene.GetMaterials().size()
    );

    for (
        const SceneMaterial& material :
        scene.GetMaterials()
    )
    {
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
std::vector<GpuSphere>
BuildGpuSpheres(const Scene& scene)
{
    std::vector<GpuSphere> result;

    result.reserve(
        scene.GetSpheres().size()
    );

    for (
        const SceneSphere& sphere :
        scene.GetSpheres()
    )
    {
        GpuSphere gpuSphere;

        gpuSphere.CenterRadius =
            glm::vec4(
                sphere.center,
                sphere.radius
            );

        gpuSphere.Metadata =
            glm::ivec4(
                static_cast<int>(sphere.material),
                0,
                0,
                0
            );

        result.push_back(gpuSphere);
    }

    return result;
}
GLuint CreateStorageBuffer(
    GLuint binding,
    const void* data,
    GLsizeiptr size
)
{
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

int Renderer(const Scene& scene)
{
    // The scene is not needed for this first compute-shader test.
    // (void)scene;
    const std::vector<GpuMaterial> gpuMaterials =
    BuildGpuMaterials(scene);

    const std::vector<GpuSphere> gpuSpheres =
        BuildGpuSpheres(scene);

    Window window(RenderWidth, RenderHeight, "Viewport");

    if (window.Initialize() != 0)
        return -1;

    Shader computeShader("OpenGLRenderer/Shaders/render.comp");
    Shader fullscreenShader("OpenGLRenderer/Shaders/render.vs","OpenGLRenderer/Shaders/render.fs");
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
    /*
     * Create the texture that the compute shader will write into.
     */
    GLuint outputTexture = 0;

    glGenTextures(1, &outputTexture);
    glBindTexture(GL_TEXTURE_2D, outputTexture);

    glTexStorage2D(
        GL_TEXTURE_2D,
        1,
        GL_RGBA32F,
        RenderWidth,
        RenderHeight
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_NEAREST
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_NEAREST
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_EDGE
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE
    );

    glBindTexture(GL_TEXTURE_2D, 0);

    glBindImageTexture(0,outputTexture,0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA32F);

    const GLuint groupCountX = (RenderWidth + ComputeLocalSizeX - 1) / ComputeLocalSizeX;
    const GLuint groupCountY = (RenderHeight + ComputeLocalSizeY - 1)/ ComputeLocalSizeY;

    computeShader.use();
    const SceneCamera& camera = scene.GetCamera();
    computeShader.setVec3("uCameraLookFrom",camera.lookFrom);
    computeShader.setVec3("uCameraLookAt",camera.lookAt);
    computeShader.setVec3("uCameraVUp",camera.up);
    computeShader.setFloat("uCameraVerticalFov",camera.verticalFovDegrees);
    computeShader.setFloat("uCameraFocusDistance",camera.focusDistance);
    computeShader.setFloat("uCameraDefocusAngle",camera.defocusAngleDegrees);

    glDispatchCompute(groupCountX,groupCountY,1);

    /*
     * Ensure that the image writes performed by the compute shader are
     * visible when the texture is sampled by the fragment shader.
     */
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |GL_TEXTURE_FETCH_BARRIER_BIT);

    /*
     * A VAO is required in the OpenGL core profile, even though the
     * fullscreen triangle does not use a vertex buffer.
     *
     * Its vertices are generated using gl_VertexID.
     */
    GLuint fullscreenVAO = 0;
    glGenVertexArrays(1, &fullscreenVAO);

    fullscreenShader.use();
    fullscreenShader.setInt("outputTexture", 0);
    std::cerr << "Compute program: " << computeShader.ID << '\n';
    std::cerr << "Fullscreen program: " << fullscreenShader.ID << '\n';
    std::cerr << "Compute valid: "<< glIsProgram(computeShader.ID)<< '\n';
    std::cerr << "Fullscreen valid: "<< glIsProgram(fullscreenShader.ID)<< '\n';

    while (!window.ShouldClose()) {
        glfwPollEvents();

        int framebufferWidth = 0;
        int framebufferHeight = 0;

        glfwGetFramebufferSize(window.GetNativeWindow(), &framebufferWidth, &framebufferHeight);
        glViewport(0,0, framebufferWidth, framebufferHeight);
        glClearColor(0.0f,0.0f,0.0f,1.0f);

        glClear(GL_COLOR_BUFFER_BIT);

        fullscreenShader.use();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,outputTexture);

        glBindVertexArray(fullscreenVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window.GetNativeWindow());
    }

    glDeleteVertexArrays(1, &fullscreenVAO);
    glDeleteTextures(1, &outputTexture);
    glDeleteBuffers(1, &materialBuffer);
    glDeleteBuffers(1, &sphereBuffer);
    glDeleteProgram(computeShader.ID);
    glDeleteProgram(fullscreenShader.ID);
    glfwDestroyWindow(window.GetNativeWindow());
    glfwTerminate();

    return 0;
}