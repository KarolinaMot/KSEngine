#include "Engine.hpp"
#include <ApplicationModule.hpp>
#include <Log.hpp>
#include <cassert>
#include <glfw/glfw3.h>
#include <memory>

void glfw_error_callback(int error_code, const char* error_message)
{
    Log("GLFW error {}: {}", error_code, error_message);
    assert(false && error_message);
}

void ApplicationModule::Initialize(Engine& e)
{
    glfwSetErrorCallback(glfw_error_callback);
    glfwInit();

    main_window = std::make_unique<Window>(Window::CreateParameters {});

    e.AddExecutionDelegate(this, &ApplicationModule::ProcessInput, ExecutionOrder::LAST);
}

void ApplicationModule::Shutdown(MAYBE_UNUSED Engine& e)
{
    main_window.reset();
    glfwTerminate();
}

void ApplicationModule::ProcessInput(Engine& e)
{
    if (main_window->ShouldClose())
    {
        e.SetExit(0);
        return;
    }

    main_window->GetInputHandler().UpdateInputState();
    glfwPollEvents();
}