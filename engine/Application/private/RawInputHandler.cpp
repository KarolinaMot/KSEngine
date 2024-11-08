#include <RawInputHandler.hpp>
#include <Window.hpp>
#include <glfw/glfw3.h>

RawInputHandler::RawInputHandler(GLFWwindow* window_handle)
{
    bound_window = window_handle;

    glfwSetCursorPosCallback(bound_window, RawInputHandler::cursor_position_callback);
    glfwSetKeyCallback(bound_window, RawInputHandler::key_callback);
    glfwSetScrollCallback(bound_window, RawInputHandler::scroll_callback);
    glfwSetMouseButtonCallback(bound_window, RawInputHandler::mousebutton_callback);

    if (glfwRawMouseMotionSupported())
    {
        glfwSetInputMode(bound_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
}

RawInputHandler::~RawInputHandler()
{
    glfwSetCursorPosCallback(bound_window, nullptr);
    glfwSetKeyCallback(bound_window, nullptr);
    glfwSetScrollCallback(bound_window, nullptr);
    glfwSetMouseButtonCallback(bound_window, nullptr);
}

void RawInputHandler::UpdateInputState()
{
    // update keyboard key states
    for (auto&& [key, state] : keys)
    {

        if (state == InputState::Down)
        {
            state = InputState::Pressed;
        }

        if (state == InputState::Release)
        {
            state = InputState::None;
        }
    }

    // update mouse button states
    for (auto&& [key, state] : mouse_buttons)
    {

        if (state == InputState::Down)
        {
            state = InputState::Pressed;
        }

        if (state == InputState::Release)
        {
            state = InputState::None;
        }
    }

    prev_mouse_coords = mouse_coords;
}

InputState RawInputHandler::GetKeyboard(KeyboardKey key) const
{
    if (auto it = keys.find(key); it != keys.end())
    {
        return it->second;
    }
    return InputState::None;
}

InputState RawInputHandler::GetMouseButton(MouseButton button) const
{
    if (auto it = mouse_buttons.find(button); it != mouse_buttons.end())
    {
        return it->second;
    }
    return InputState::None;
}

glm::vec2 RawInputHandler::GetMousePos() const
{
    return mouse_coords;
}

glm::vec2 RawInputHandler::GetMouseDelta() const
{
    return mouse_coords - prev_mouse_coords;
}

RawInputHandler* RawInputHandler::get_handler(GLFWwindow* window)
{
    if (auto* owner = static_cast<Window*>(glfwGetWindowUserPointer(window)))
    {
        return &owner->GetInputHandler();
    }
    return nullptr;
}

void RawInputHandler::cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (auto* data = RawInputHandler::get_handler(window))
    {
        data->mouse_coords = glm::vec2 { static_cast<float>(xpos), static_cast<float>(ypos) };
    }
}

void RawInputHandler::scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{

    if (auto* data = RawInputHandler::get_handler(window))
    {
        data->mouse_scroll += glm::vec2 { static_cast<float>(xoffset), static_cast<float>(yoffset) };
    }
}

void RawInputHandler::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (auto* data = RawInputHandler::get_handler(window))
    {
        auto& key_entry = data->keys[static_cast<KeyboardKey>(key)];

        if (action == GLFW_PRESS)
        {
            key_entry = InputState::Down;
        }

        if (action == GLFW_RELEASE)
        {
            key_entry = InputState::Release;
        }
    }
}

void RawInputHandler::mousebutton_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (auto* data = RawInputHandler::get_handler(window))
    {
        auto& button_entry = data->mouse_buttons[static_cast<MouseButton>(button)];

        if (action == GLFW_PRESS)
        {
            button_entry = InputState::Down;
        }

        if (action == GLFW_RELEASE)
        {
            button_entry = InputState::Release;
        }
    }
}