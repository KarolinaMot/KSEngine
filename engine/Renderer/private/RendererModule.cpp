#include <ApplicationModule.hpp>
#include <DXBackendModule.hpp>
#include <FileIO.hpp>
#include <Log.hpp>
#include <RawInputHandler.hpp>
#include <RendererModule.hpp>
#include <SerializationCommon.hpp>
#include <TimeModule.hpp>
#include <constants/MeshContants.hpp>
#include <gpu_resources/DXResourceBuilder.hpp>
#include <resources/Mesh.hpp>
#include <resources/Model.hpp>
#include <shader/DXShaderInputs.hpp>

void RendererModule::Initialize(Engine& e)
{
    auto& application = e.GetModule<ApplicationModule>();
    auto& backend = e.GetModule<DXBackendModule>();

    auto& window = application.GetMainWindow();
    auto& dx_factory = backend.GetFactory();
    auto& dx_device = backend.GetDevice();

    HWND win_handle = static_cast<HWND>(window.GetNativeWindowHandle());

    auto swapchain = dx_factory.CreateSwapchainForWindow(dx_device.GetCommandQueue().Get(), win_handle, window.GetSize());
    main_swapchain = std::make_unique<DXSwapchain>(swapchain, dx_device.Get());

    auto& compiler = backend.GetShaderCompiler();
    forward_renderer = std::make_unique<ForwardRenderer>(dx_device, compiler, main_swapchain->GetResolution());

    e.AddExecutionDelegate(this, &RendererModule::RenderFrame, ExecutionOrder::POST_RENDER);
}

void RendererModule::Shutdown(Engine& e)
{
    e.GetModule<DXBackendModule>().GetDevice().GetCommandQueue().Flush();
}

void RendererModule::RenderFrame(Engine& e)
{
    auto& backend = e.GetModule<DXBackendModule>();
    forward_renderer->RenderFrame(set_camera, backend.GetDevice(), *main_swapchain);
}