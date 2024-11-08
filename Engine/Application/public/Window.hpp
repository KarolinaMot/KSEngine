#pragma once
#include <Common.hpp>
#include <RawInputHandler.hpp>
#include <glm/vec2.hpp>
#include <memory>
#include <string>

struct GLFWwindow;

// Wrapper class for handling one window
// TODO: lacks functionality for resizing and other window flags
class Window
{
public:
    static constexpr glm::uvec2 DEFAULT_RESOLUTION = { 1600, 900 };
    static constexpr auto DEFAULT_NAME = "KSEngine Game";

    struct CreateParameters
    {
        std::string name = DEFAULT_NAME;
        glm::uvec2 size = DEFAULT_RESOLUTION;
    };

    Window(const CreateParameters& params);
    ~Window();

    NON_COPYABLE(Window)
    NON_MOVABLE(Window)

    void SetVisibility(bool val);

    glm::uvec2 GetSize() const { return window_size; }
    bool IsVisible() const { return is_visible; };
    bool ShouldClose() const { return should_close; };

    RawInputHandler& GetInputHandler() { return *input_handler; }
    GLFWwindow* GetWindowHandle() const { return window_handle; }
    void* GetNativeWindowHandle() const;

private:
    glm::uvec2 window_size {};
    bool should_close = false;
    bool is_visible = true;

    GLFWwindow* window_handle = nullptr;
    std::unique_ptr<RawInputHandler> input_handler {};

    static void window_exit_callback(GLFWwindow* window);
};