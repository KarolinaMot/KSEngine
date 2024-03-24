#include "device/DeviceModule.hpp"
#include "GLFW/glfw3.h"
#include "GLFW/glfw3.h"
#include "utils/Log.hpp"

namespace KSE {
	class DeviceModule::Impl {
    public:
		Impl() = default;

		GLFWwindow* mWindow;
		GLFWmonitor* mMonitor;
		unsigned int mFrameIndex = 0;
		bool mFullscreen = false;
        bool mIsWindowOpen = false;
		
	};
}

KSE::DeviceModule::DeviceModule(EngineClass& e) : IModule(e)
{
    InitializeWindow();
}

KSE::DeviceModule::~DeviceModule()
{
}

void KSE::DeviceModule::SwapWindowBuffers(ResourceHandle<Framebuffer> rendered_frame)
{
}

void KSE::DeviceModule::NewFrame()
{
}

void KSE::DeviceModule::EndFrame()
{
}

void KSE::DeviceModule::InitializeWindow()
{
    LOG(LogType::INFO, "Initializing GLFW");

    if (!glfwInit())
    {
        LOG(LogType::ERROR, "GLFW could not be initialized");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    impl->mMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(impl->mMonitor);

    auto maxScreenWidth = mode->width;
    auto maxScreenHeight = mode->height;

    if (impl->mFullscreen)
    {
        impl->mWindow = glfwCreateWindow(maxScreenWidth, maxScreenHeight, "KSEngine", impl->mMonitor, nullptr);
    }
    else
    {
        glfwWindowHint(GLFW_RESIZABLE, 1);
        impl->mWindow = glfwCreateWindow(1920, 1080, "KSEngine", nullptr, nullptr);
    }

    if (impl->mWindow == nullptr)
    {
        LOG(LogType::ERROR, "GLFW window could not be created");
    }

    glfwMakeContextCurrent(impl->mWindow);
    glfwShowWindow(impl->mWindow);
    impl->mIsWindowOpen = true;

}

void KSE::DeviceModule::InitializeDevice()
{
}
