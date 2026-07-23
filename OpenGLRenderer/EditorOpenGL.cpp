//
// Created by Nutbotty on 7/17/2026.
//
//TODO Refactor all of this
#include "EditorOpenGL.h"
#include <iostream>
#include "Shaders/shader.h"
#include "editorcamera.h"
#include "../external/glm/glm/gtc/matrix_transform.hpp"
#include "Window.h"
#include "Input.h"

namespace {
// settings
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;
bool TRACE = true;

constexpr GLuint MaterialBufferBinding = 1;
constexpr GLuint SphereBufferBinding = 2;
constexpr unsigned int ComputeLocalSizeX = 16;
constexpr unsigned int ComputeLocalSizeY = 16;
const GLuint groupCountX = (SCR_WIDTH + ComputeLocalSizeX - 1) / ComputeLocalSizeX;
const GLuint groupCountY = (SCR_HEIGHT + ComputeLocalSizeY - 1)/ ComputeLocalSizeY;
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

// camera
EditorCamera camera(
    glm::vec3(13.0f, 2.0f, 3.0f),
    glm::vec3(0,1,0),
    -167.0f,
    -5.0f,
    10.0,
    4.0
);

// timing
float deltaTime = 0.0f;	// time between current frame and last frame
float lastFrame = 0.0f;

struct IcoSphereMesh
{
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
};
IcoSphereMesh GenerateIcoSphere(int subdivisions);

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
}

EditorCamera viewport(const Scene& scene) {

    const std::vector<GpuMaterial> gpuMaterials =
    BuildGpuMaterials(scene);

    const std::vector<GpuSphere> gpuSpheres =
        BuildGpuSpheres(scene);

    Window window(SCR_WIDTH, SCR_HEIGHT, "Viewport");

    if (window.Initialize() != 0)
        return camera;

    Input input(camera);
    input.Initialize(window.GetNativeWindow());
    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);
    // build and compile our shader program
    // ------------------------------------
    Shader ourShader("OpenGLRenderer/Shaders/shader.vs", "OpenGLRenderer/Shaders/shader.fs"); // you can name your shader files however you like
    Shader computeShader("OpenGLRenderer/Shaders/editor.comp");
    Shader fullscreenShader("OpenGLRenderer/Shaders/render.vs","OpenGLRenderer/Shaders/render.fs");
    const GLuint materialBuffer = CreateStorageBuffer(
        MaterialBufferBinding,
        gpuMaterials.data(),
        gpuMaterials.size() * sizeof(GpuMaterial));

    const GLuint sphereBuffer = CreateStorageBuffer(
        SphereBufferBinding,
        gpuSpheres.data(),
        gpuSpheres.size() * sizeof(GpuSphere));

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
    glTexStorage2D(
    GL_TEXTURE_2D,
    1,
    GL_RGBA32F,
    SCR_WIDTH,
    SCR_HEIGHT
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

    while (!window.ShouldClose())
    {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        // input
        // -----
        input.Update(deltaTime);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (TRACE) {

            computeShader.use();
            computeShader.setVec3("uCameraLookFrom",camera.Position);
            computeShader.setVec3("uCameraLookAt",camera.Position + camera.Front);
            computeShader.setVec3("uCameraVUp",camera.Up);
            computeShader.setFloat("uCameraVerticalFov",camera.Zoom);
            computeShader.setFloat("uCameraFocusDistance", camera.FocusDistance);
            computeShader.setFloat("uCameraDefocusAngle", camera.DefocusAngle);

            glDispatchCompute(groupCountX, groupCountY, 1);

            glMemoryBarrier(
                GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                GL_TEXTURE_FETCH_BARRIER_BIT);

            fullscreenShader.use();
            fullscreenShader.setInt("outputTexture", 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, outputTexture);
            glBindVertexArray(fullscreenVAO);
            glDrawArrays(GL_TRIANGLES,0,3);
        }
        else if (!TRACE) {
            // render
            // ------
            glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // render the triangle
            ourShader.use();

            // pass projection matrix to shader (note that in this case it could change every frame)
            glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
            ourShader.setMat4("projection", projection);

            // camera/view transformation
            glm::mat4 view = camera.GetViewMatrix();
            ourShader.setMat4("view", view);
            ourShader.setVec3("cameraPosition",camera.Position);
            ourShader.setFloat("focusDistance",camera.FocusDistance);
            ourShader.setFloat("defocusAngle",camera.DefocusAngle);
            ourShader.setVec3("cameraFront",camera.Front);
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
                glDrawElements(GL_TRIANGLES, 60, GL_UNSIGNED_INT, 0);
            }
        }
        window.SwapBuffers();
        window.PollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &fullscreenVAO);
    glDeleteTextures(1, &outputTexture);
    glDeleteBuffers(1, &materialBuffer);
    glDeleteBuffers(1, &sphereBuffer);
    glDeleteProgram(computeShader.ID);
    glDeleteProgram(fullscreenShader.ID);

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    return camera;
}