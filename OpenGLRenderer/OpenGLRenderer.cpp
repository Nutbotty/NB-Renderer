//
// Created by Nutbotty on 7/20/2026.
//
#include "OpenGLRenderer.h"
#include "Shaders/shader.h"

#include <iostream>

namespace
{
    constexpr int RenderWidth = 2560;
    constexpr int RenderHeight = 1440;

    constexpr unsigned int ComputeLocalSizeX = 16;
    constexpr unsigned int ComputeLocalSizeY = 16;
}

int Renderer(const Scene& scene)
{
    // The scene is not needed for this first compute-shader test.
    (void)scene;

    if (glfwInit() == GLFW_FALSE)
    {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(
        RenderWidth,
        RenderHeight,
        "Compute Shader Test",
        nullptr,
        nullptr
    );

    if (window == nullptr)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader(
            reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
    {
        std::cerr << "Failed to initialize GLAD\n";

        glfwDestroyWindow(window);
        glfwTerminate();

        return -1;
    }

    Shader computeShader(
        "OpenGLRenderer/Shaders/render.comp"
    );

    Shader fullscreenShader(
        "OpenGLRenderer/Shaders/render.vs",
        "OpenGLRenderer/Shaders/render.fs"
    );

    /*
     * Create the texture that the compute shader will write into.
     *
     * RGBA32F is convenient during early ray tracer development because
     * every channel is stored as a 32-bit floating-point value.
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

    /*
     * Bind the texture to image unit 0.
     *
     * This matches:
     *
     * layout(rgba32f, binding = 0) uniform writeonly image2D outputImage;
     *
     * in basic.comp.
     */
    glBindImageTexture(
        0,
        outputTexture,
        0,
        GL_FALSE,
        0,
        GL_WRITE_ONLY,
        GL_RGBA32F
    );

    /*
     * Run the compute shader once.
     *
     * The shader uses 16x16 local workgroups, so round the number of
     * workgroups upward to cover the entire image.
     */
    const GLuint groupCountX =
        (RenderWidth + ComputeLocalSizeX - 1)
        / ComputeLocalSizeX;

    const GLuint groupCountY =
        (RenderHeight + ComputeLocalSizeY - 1)
        / ComputeLocalSizeY;

    computeShader.use();

    glDispatchCompute(
        groupCountX,
        groupCountY,
        1
    );

    /*
     * Ensure that the image writes performed by the compute shader are
     * visible when the texture is sampled by the fragment shader.
     */
    glMemoryBarrier(
        GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
        GL_TEXTURE_FETCH_BARRIER_BIT
    );

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

    std::cerr << "Compute valid: "
              << glIsProgram(computeShader.ID)
              << '\n';

    std::cerr << "Fullscreen valid: "
              << glIsProgram(fullscreenShader.ID)
              << '\n';

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        int framebufferWidth = 0;
        int framebufferHeight = 0;

        glfwGetFramebufferSize(
            window,
            &framebufferWidth,
            &framebufferHeight
        );

        glViewport(
            0,
            0,
            framebufferWidth,
            framebufferHeight
        );

        glClearColor(
            0.0f,
            0.0f,
            0.0f,
            1.0f
        );

        glClear(GL_COLOR_BUFFER_BIT);

        fullscreenShader.use();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(
            GL_TEXTURE_2D,
            outputTexture
        );

        glBindVertexArray(fullscreenVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &fullscreenVAO);
    glDeleteTextures(1, &outputTexture);

    glDeleteProgram(computeShader.ID);
    glDeleteProgram(fullscreenShader.ID);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}