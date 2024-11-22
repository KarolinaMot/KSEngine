#pragma once

#include <Common.hpp>
#include <InputCodes.hpp>
#include <glm/vec2.hpp>
#include <unordered_map>

struct GLFWwindow;

// Input handler that works for one window
/// @brief TODO: only supports keyboard and is PC only
class RawInputHandler
{
public:
    RawInputHandler(GLFWwindow* window);
    ~RawInputHandler();

    void UpdateInputState();

    InputState GetKeyboard(KeyboardKey key) const;
    InputState GetMouseButton(MouseButton button) const;
    glm::vec2 GetMousePos() const;
    glm::vec2 GetMouseDelta() const;

    NON_COPYABLE(RawInputHandler);
    NON_MOVABLE(RawInputHandler);

private:
    GLFWwindow* bound_window {};

    std::unordered_map<KeyboardKey, InputState> keys;
    std::unordered_map<MouseButton, InputState> mouse_buttons;

    glm::vec2 mouse_coords {};
    glm::vec2 prev_mouse_coords {};
    glm::vec2 mouse_scroll {};

    // CALLBACKS
    static RawInputHandler* get_handler(GLFWwindow* window);
    static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mousebutton_callback(GLFWwindow* window, int button, int action, int mods);
};