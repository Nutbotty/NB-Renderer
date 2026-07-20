//
// Created by Nutbotty on 7/20/2026.
//

#ifndef NB_RENDERER_WINDOW_H
#define NB_RENDERER_WINDOW_H

#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
//remove these include from viewport.h?


class Window {
public:
    Window(int width = 800, int height = 600, const std::string& title = "Viewport");
    ~Window();

    int Initialize();

    bool ShouldClose() const;
    void Close();

    void PollEvents() const;
    void SwapBuffers() const;

    GLFWwindow* GetNativeWindow() const;

    int GetWidth() const;
    int GetHeight() const;
private:
    static void FramebufferSizeCallback(
        GLFWwindow* window,
        int width,
        int height
    );

    GLFWwindow* m_Window = nullptr;

    int Width;
    int Height;
    std::string Title;
};



#endif //NB_RENDERER_WINDOW_H

