#include "RawInputCollector.hpp"
#include <device/Device.hpp>

namespace KS::detail
{

KS::RawInputCollector* GetUser(GLFWwindow* window)
{
    return static_cast<KS::RawInputCollector*>(glfwGetWindowUserPointer(window));
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (auto* data = GetUser(window))
    {
        RawInput::Code code_horizontal { RawInput::Source::MOUSE_POSITION, RawInput::Direction::HORIZONTAL };
        RawInput::Code code_vertical { RawInput::Source::MOUSE_POSITION, RawInput::Direction::VERTICAL };

        RawInput::Value horizontal_val { static_cast<float>(xpos) };
        RawInput::Value vertical_val { static_cast<float>(ypos) };

        data->AddRawInput({ code_horizontal, horizontal_val });
        data->AddRawInput({ code_vertical, vertical_val });
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (auto* data = GetUser(window))
    {
        RawInput::Code code_horizontal { RawInput::Source::MOUSE_SCROLL, RawInput::Direction::HORIZONTAL };
        RawInput::Code code_vertical { RawInput::Source::MOUSE_SCROLL, RawInput::Direction::VERTICAL };

        RawInput::Value horizontal_val { static_cast<float>(xoffset) };
        RawInput::Value vertical_val { static_cast<float>(yoffset) };

        data->AddRawInput({ code_horizontal, horizontal_val });
        data->AddRawInput({ code_vertical, vertical_val });
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (auto* data = GetUser(window))
    {
        RawInput::Code code { RawInput::Source::KEYBOARD, key };

        if (action == GLFW_PRESS)
        {
            data->AddRawInput({ code, RawInput::Value(RawInput::State::PRESS_DOWN) });
        }

        if (action == GLFW_RELEASE)
        {
            data->AddRawInput({ code, RawInput::Value(RawInput::State::PRESS_UP) });
        }
    }
}

void mousebutton_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (auto* data = GetUser(window))
    {
        RawInput::Code code { RawInput::Source::MOUSE_BUTTONS, button };

        if (action == GLFW_PRESS)
        {
            data->AddRawInput({ code, RawInput::Value(RawInput::State::PRESS_DOWN) });
        }

        if (action == GLFW_RELEASE)
        {
            data->AddRawInput({ code, RawInput::Value(RawInput::State::PRESS_UP) });
        }
    }
}

}

KS::RawInputCollector::RawInputCollector(const Device& device)
{
    GLFWwindow* window_handle = static_cast<GLFWwindow*>(device.GetWindowHandle());

    glfwSetWindowUserPointer(window_handle, this);
    glfwSetCursorPosCallback(window_handle, detail::cursor_position_callback);
    glfwSetKeyCallback(window_handle, detail::key_callback);
    glfwSetScrollCallback(window_handle, detail::scroll_callback);
    glfwSetMouseButtonCallback(window_handle, detail::mousebutton_callback);

    if (glfwRawMouseMotionSupported())
    {
        glfwSetInputMode(window_handle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
}

void KS::RawInputCollector::AddRawInput(RawInput::Data input)
{
    collected.emplace(input);

    if (std::holds_alternative<RawInput::State>(input.second))
    {
        auto state = std::get<RawInput::State>(input.second);

        if (state == RawInput::State::PRESS_DOWN)
        {
            press_cache[input.first] = true;
        }
        else if (state == RawInput::State::PRESS_UP)
        {
            press_cache[input.first] = false;
        }
    }
}

std::queue<KS::RawInput::Data> KS::RawInputCollector::ProcessInput()
{
    glfwPollEvents();
    auto dump = std::move(collected);

    for (auto& [key, pressed] : press_cache)
    {
        if (pressed)
        {
            RawInput::Value val = RawInput::State::ACTIVE;
            dump.emplace(key, val);
        }
    }

    collected = {};

    return dump;
}
