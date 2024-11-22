#include <Window.hpp>

#include <glfw/glfw3.h>
#include <glfw/glfw3native.h>

void Window::window_exit_callback(GLFWwindow* window)
{
    auto* this_window = static_cast<Window*>(glfwGetWindowUserPointer(window));
    this_window->should_close = true;
}

Window::Window(const CreateParameters& params)
{
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window_handle = glfwCreateWindow(
        static_cast<int>(params.size.x),
        static_cast<int>(params.size.y),
        params.name.c_str(),
        nullptr, nullptr);

    glfwSetWindowUserPointer(window_handle, this);
    glfwSetWindowCloseCallback(window_handle, window_exit_callback);

    input_handler = std::make_unique<RawInputHandler>(window_handle);
    window_size = params.size;
}

Window::~Window()
{
    glfwDestroyWindow(window_handle);
}

bool Window::IsMinimized() const
{
    bool minimized = (bool)glfwGetWindowAttrib(window_handle, GLFW_ICONIFIED);
    return minimized;
}

bool Window::IsFocused() const
{
    bool minimized = (bool)glfwGetWindowAttrib(window_handle, GLFW_FOCUSED);
    return minimized;
}

void* Window::GetNativeWindowHandle() const
{
    return glfwGetWin32Window(window_handle);
}
