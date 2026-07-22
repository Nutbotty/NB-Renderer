//
// Created by Nutbotty on 7/17/2026.
//
#include "openglviewport.h"
#include <iostream>
#include "Shaders/shader.h"
#include "editorcamera.h"
#include "../external/glm/glm/gtc/matrix_transform.hpp"
#include "Window.h"
#include "Input.h"


// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

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

EditorCamera viewport(const Scene& scene) {

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



        window.SwapBuffers();
        window.PollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    return camera;
}