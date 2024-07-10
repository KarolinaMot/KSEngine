#include "RawInputCollector.hpp"

#include <device

class KS::RawInput::Impl
{
public:
    std::unordered_map<KeyboardKey, InputState> keys;
    std::unordered_map<MouseButton, InputState> mouse_buttons;

    float mouseX {}, mouseY {};
    float deltaX {}, deltaY {};
    float mouseScrollX {}, mouseScrollY {};
};

namespace KS::detail
{

KS::RawInput::Impl* GetUser(GLFWwindow* window)
{
    return static_cast<KS::RawInput::Impl*>(glfwGetWindowUserPointer(window));
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (auto* data = GetUser(window))
    {
        data->mouseX = static_cast<float>(xpos);
        data->mouseY = static_cast<float>(ypos);
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{

    if (auto* data = GetUser(window))
    {
        data->mouseScrollX += static_cast<float>(xoffset);
        data->mouseScrollY += static_cast<float>(yoffset);
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (auto* data = GetUser(window))
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

void mousebutton_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (auto* data = GetUser(window))
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

}

KS::RawInput::RawInput(const Device& device)
{
    m_Impl = std::make_unique<Impl>();
    m_Device = device;

    GLFWwindow* window_handle = static_cast<GLFWwindow*>(device->GetWindowHandle());

    glfwSetWindowUserPointer(window_handle, m_Impl.get());
    glfwSetCursorPosCallback(window_handle, detail::cursor_position_callback);
    glfwSetKeyCallback(window_handle, detail::key_callback);
    glfwSetScrollCallback(window_handle, detail::scroll_callback);
    glfwSetMouseButtonCallback(window_handle, detail::mousebutton_callback);

    if (glfwRawMouseMotionSupported())
    {
        glfwSetInputMode(window_handle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
}

KS::RawInput::~RawInput()
    KS::RawInput::RawInput(const Device& device)
{
}
{
}

std::queue<KS::RawInputValue> KS::RawInput::ProcessInput()
{
    // Down keys -> Pressed keys
    // Pressed Keys ->
    // for (auto&& [key, state] : m_Impl->keys) {

    //     if (state == InputState::Down) {
    //         state = InputState::Pressed;
    //     }

    //     if (state == InputState::Release) {
    //         state = InputState::None;
    //     }
    // }

    // // Press
    // for (auto&& [key, state] : m_Impl->mouse_buttons) {

    //     if (state == InputState::Down) {
    //         state = InputState::Pressed;
    //     }

    //     if (state == InputState::Release) {
    //         state = InputState::None;
    //     }
    // }

    // auto prevX = m_Impl->mouseX;
    // auto prevY = m_Impl->mouseY;

    // m_Impl->deltaX = m_Impl->mouseX - prevX;
    // m_Impl->deltaY = m_Impl->mouseY - prevY;
}

KS::InputState KS::RawInput::GetKeyboard(KeyboardKey key) const
{
    return m_Impl->keys[key];
}

KS::InputState KS::RawInput::GetMouseButton(MouseButton button) const
{
    return m_Impl->mouse_buttons[button];
}

std::pair<float, float> KS::RawInput::GetMousePos() const
{
    return { m_Impl->mouseX, m_Impl->mouseY };
}

std::pair<float, float> KS::RawInput::GetMouseDelta() const
{
    return { m_Impl->deltaX, m_Impl->deltaY };
}
