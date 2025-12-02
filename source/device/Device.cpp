#include <device/Device.hpp>
#include <renderer/DX12/Helpers/DXDescHeap.hpp>
#include <renderer/DX12/Helpers/DXHeapHandle.hpp>
#include <renderer/DX12/Helpers/DXIncludes.hpp>
#include <renderer/DX12/Helpers/DXResource.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXCommandQueue.hpp>
#include <renderer/DX12/Helpers/DXCommandContextPool.hpp>
#include <renderer/InfoStructs.hpp>
#include <renderer/UploadArena.h>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputBlueprint.hpp>
#include <renderer/ShaderInputBlueprintBuilder.hpp>
#include <tools/Log.hpp>
#include "DX12/DXFactory.hpp"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_dx12.h>

#include <thread>
#include <vector>
#include "Device.hpp"

class KS::Device::Impl
{
public:
    void InitializeWindow(const DeviceInitParams& params);
    void InitializeDevice(const DeviceInitParams& params);
    void CreateSwapchain(uint32_t newWidth, uint32_t newHeight, DXFactory* factory, HWND HWNDwindow);
    UINT GetFramebufferIndex();

    // void BindSwapchainRT();
    void StartFrame(int cpuFrame);
    void EndFrame(int cpuFrame);

    enum DXResources
    {
        RT,
        DEPTH_STENCIL_RSC = FRAME_BUFFER_COUNT,
        NUM_RESOURCES
    };

    enum DXHeaps
    {
        RT_HEAP,
        DEPTH_HEAP,
        IMGUI_HEAP,
       // RESOURCE_HEAP,
        NUM_DESC_HEAPS
    };

    GLFWwindow* m_window;
    GLFWmonitor* m_monitor;

    ComPtr<ID3D12Device5> m_device;
    ComPtr<IDXGISwapChain3> m_swapchain;

    std::unique_ptr<DXCommandQueue> m_command_queue;
    std::shared_ptr<DXCommandContextPool> m_commandPool;
    DXGPUFuture m_fence_values[FRAME_BUFFER_COUNT];
    std::shared_ptr<UploadArena> m_uploadArena;

    std::shared_ptr<DXDescHeap> m_descriptor_heaps[NUM_DESC_HEAPS];
    const DXGI_FORMAT m_depth_format = DXGI_FORMAT_D32_FLOAT;
};

KS::Device::Device(const DeviceInitParams& params)
{
    m_impl = std::make_unique<Impl>();
    m_fullscreen = false;

    m_swapchainWidth = params.window_width;
    m_swapchainHeight = params.window_height;
    m_windowHeight = params.window_height;
    m_windowWidth = params.window_width;

    m_impl->InitializeWindow(params);
    m_window_open = true;
    m_clear_color = params.clear_color;
    m_impl->InitializeDevice(params);
    m_frame_index = 0;
}

KS::Device::~Device()
{
    glfwDestroyWindow(m_impl->m_window);
}

void* KS::Device::GetDevice() const
{
    return m_impl->m_device.Get(); }

DXCommandContext KS::Device::GetCommandContext() const
{ 
    return m_impl->m_commandPool->GetCommandSet(m_impl->m_device);
}

void* KS::Device::GetDepthHeap() const
{
    return m_impl->m_descriptor_heaps[Impl::DXHeaps::DEPTH_HEAP].get(); 
}

void* KS::Device::GetRenderTargetHeap() const
{
    return m_impl->m_descriptor_heaps[Impl::DXHeaps::RT_HEAP].get();
}

void* KS::Device::GetWindowHandle() const
{
    return m_impl->m_window;
}

void KS::Device::NewFrame()
{
    auto commandContext = m_impl->m_commandPool->GetCommandSet(m_impl->m_device);
    auto& commandList = commandContext.m_commandList;

    m_window_open = !glfwWindowShouldClose(m_impl->m_window);
    glfwGetWindowSize(m_impl->m_window, &m_windowWidth, &m_windowHeight);
    m_frame_index = m_impl->GetFramebufferIndex();
    if (m_windowWidth != m_swapchainWidth || m_windowHeight != m_swapchainHeight)
    {
        //EndFrame();
        int waitFrame = (m_frame_index + 1) % FRAME_BUFFER_COUNT;
        m_impl->m_fence_values[m_frame_index].Wait();
        m_impl->m_fence_values[waitFrame].Wait();
        m_impl->m_commandPool->RetireCompleted();

        ResizeSwapchain(m_windowWidth, m_windowHeight);
    }

    m_cpu_frame = (m_frame_index + 1) % FRAME_BUFFER_COUNT;
    m_impl->StartFrame(m_cpu_frame);

    m_swapchainRT->Bind(*commandList, m_cpu_frame, m_swapchainDS.get());
    m_swapchainRT->Clear(*commandList, m_cpu_frame);
    m_swapchainDS->Clear(*commandList);

    ImGui::GetIO().DisplaySize.x = static_cast<float>(m_windowWidth);
    ImGui::GetIO().DisplaySize.y = static_cast<float>(m_windowHeight);
    auto io = ImGui::GetIO();
    io.DisplayFramebufferScale = ImVec2(m_windowWidth / (float)m_swapchainWidth, m_windowHeight / (float)m_swapchainHeight);
    double mx, my;
    glfwGetCursorPos(m_impl->m_window, &mx, &my);
    ImGui::GetIO().MousePos = ImVec2((float)(mx), (float)(my));
    //ImGui::GetIO().MousePos = ImVec2(mouse_x * io.DisplayFramebufferScale.x, mouse_y * io.DisplayFramebufferScale.y);

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    commandContext.Close();

}

void KS::Device::EndFrame()
{
    auto commandContext = m_impl->m_commandPool->GetCommandSet(m_impl->m_device);
    auto& commandList = commandContext.m_commandList;

    ImGui::Render();

    auto resourceHeap = m_impl->m_descriptor_heaps[Impl::DXHeaps::IMGUI_HEAP].get();
    commandList->BindDescriptorHeaps(resourceHeap, nullptr, nullptr);

    m_swapchainRT->Bind(*commandList, m_cpu_frame, m_swapchainDS.get());
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList->GetCommandList().Get());

    glfwSwapBuffers(m_impl->m_window);
    m_swapchainRT->PrepareToPresent(*commandList, m_cpu_frame);
    commandContext.Close();

    m_impl->EndFrame(m_cpu_frame);

    ImGui::EndFrame();
    ImGui::UpdatePlatformWindows();
}

void KS::Device::InitializeSwapchain()
{
    // CREATE RENDER TARGETS
    std::shared_ptr<Texture> swapchainTex[FRAME_BUFFER_COUNT];
    for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        ComPtr<ID3D12Resource> res;
        HRESULT hr = m_impl->m_swapchain->GetBuffer(i, IID_PPV_ARGS(&res));
        if (FAILED(hr))
        {
            LOG(Log::Severity::FATAL, "Failed to get swapchain buffer");
        }

        swapchainTex[i] = std::make_shared<Texture>(res.Get(), m_swapchainWidth, m_swapchainHeight, Texture::RENDER_TARGET);
    }

    m_swapchainRT = std::make_shared<RenderTarget>();
    m_swapchainRT->AddTexture(*this, swapchainTex[0], swapchainTex[1], "Swapchain render target");

    auto swapchainDepthTex = std::make_shared<Texture>(*this, m_swapchainWidth, m_swapchainHeight, Texture::DEPTH_TEXTURE,
                                                    glm::vec4(1.f), Formats::D32_FLOAT, "swapchain depth", 1u);
    m_swapchainDS = std::make_shared<DepthStencil>(*this, swapchainDepthTex);
}

void KS::Device::FinishInitialization()
{
    int size = 128 * 1024;
    m_impl->m_uploadArena = std::make_shared<UploadArena>(size);

    auto frame_setup = m_impl->m_commandPool->Execute(*m_impl->m_command_queue.get());
    frame_setup.Wait();
    m_impl->m_commandPool->RetireCompleted();
}

void KS::Device::InitializeImGUI()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable |
                                  ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::GetIO().ConfigViewportsNoDecoration = false;
    ImGui::GetIO().DisplaySize.x = static_cast<float>(m_swapchainWidth);
    ImGui::GetIO().DisplaySize.y = static_cast<float>(m_swapchainHeight);

    auto resourceHeap = m_impl->m_descriptor_heaps[Impl::DXHeaps::IMGUI_HEAP];

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        resourceHeap->Get()->GetCPUDescriptorHandleForHeapStart(), resourceHeap->GetDescriptorSize());
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        resourceHeap->Get()->GetGPUDescriptorHandleForHeapStart(), resourceHeap->GetDescriptorSize());

    ImGui_ImplDX12_Init(m_impl->m_device.Get(), FRAME_BUFFER_COUNT, DXGI_FORMAT_R8G8B8A8_UNORM, resourceHeap->Get(),
                        D3D12_CPU_DESCRIPTOR_HANDLE(cpuHandle.ptr), D3D12_GPU_DESCRIPTOR_HANDLE(gpuHandle.ptr));
    ImGui_ImplGlfw_InitForOther(m_impl->m_window, true);
    ImGui_ImplGlfw_SetCallbacksChainForAllWindows(true);
}

void KS::Device::ResizeSwapchain(uint32_t newWidth, uint32_t newHeight)
{
    m_swapchainWidth = newWidth;
    m_swapchainHeight = newHeight;

    //EndFrame();
    //m_impl->m_fence_values[nextCpuFrame].Wait();

    // Release old render targets and depth buffer
    m_swapchainRT.reset();
    m_swapchainDS.reset();

    // Resize swapchain buffers
    DXGI_SWAP_CHAIN_DESC desc = {};
    m_impl->m_swapchain->GetDesc(&desc);

    HRESULT hr = m_impl->m_swapchain->ResizeBuffers(FRAME_BUFFER_COUNT, m_swapchainWidth, m_swapchainHeight, desc.BufferDesc.Format,
                                       desc.Flags);
    CheckDX(hr);

    InitializeSwapchain();
    // Note: buffer count, format, and flags should match original creation.

    m_frame_index = m_impl->m_swapchain->GetCurrentBackBufferIndex();
}

void KS::Device::Impl::CreateSwapchain(uint32_t newWidth, uint32_t newHeight, DXFactory* factory, HWND HWNDwindow)
{
    DXGI_SWAP_CHAIN_DESC1 swapchain_info = {};

    swapchain_info.Width = newWidth;
    swapchain_info.Height = newHeight;

    swapchain_info.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchain_info.Stereo = FALSE;

    swapchain_info.SampleDesc = {1, 0};

    swapchain_info.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_info.BufferCount = FRAME_BUFFER_COUNT;

    swapchain_info.Scaling = DXGI_SCALING_STRETCH;
    swapchain_info.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchain_info.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapchain_info.Flags = 0;

    ComPtr<IDXGISwapChain1> tempSwapChain;
    HRESULT hr = factory->Handle()->CreateSwapChainForHwnd(m_command_queue->Get(), HWNDwindow, &swapchain_info, nullptr, nullptr,
                                                   &tempSwapChain);

    CheckDX(hr);

    tempSwapChain.As(&m_swapchain);
}


void KS::Device::CopyToSwapchainRT(DXCommandList& commandList, std::shared_ptr<RenderTarget> rt)
{
    m_swapchainRT->CopyTo(commandList, m_cpu_frame, rt, 0, 0);
}

KS::UploadArena* KS::Device::GetUploadArena() const { return m_impl->m_uploadArena.get(); }

void KS::Device::Flush() {
    m_impl->m_command_queue->Flush();
}

void window_close_callback(GLFWwindow* window)
{
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void KS::Device::Impl::InitializeWindow(const DeviceInitParams& params)
{
    if (!glfwInit())
    {
        LOG(Log::Severity::FATAL, "GLFW could not be initialized");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    m_monitor = glfwGetPrimaryMonitor();

    std::string applicationName = params.name;
    if (applicationName.empty())
    {
        applicationName += "Unnamed application";
    }

    glfwWindowHint(GLFW_RESIZABLE, 1);
    m_window = glfwCreateWindow(params.window_width, params.window_height, applicationName.c_str(), nullptr, nullptr);

    if (m_window == nullptr)
    {
        LOG(Log::Severity::FATAL, "GLFW window could not be created.");
    }

    glfwMakeContextCurrent(m_window);
    glfwShowWindow(m_window);
    glfwSetWindowCloseCallback(m_window, window_close_callback);
}

UINT KS::Device::Impl::GetFramebufferIndex()
{
    return m_swapchain->GetCurrentBackBufferIndex();
}

void KS::Device::Impl::StartFrame(int cpuFrame)
{
    // Wait until the current swapchain is available;
    m_commandPool->RetireCompleted();
    m_uploadArena->Recycle(m_fence_values[cpuFrame].GetFutureValue());
}

void KS::Device::Impl::EndFrame(int cpuFrame)
{
    // PRESENT
    if (FAILED(m_swapchain->Present(1, 0)))
    {
        LOG(Log::Severity::FATAL, "Failed to present");
    }

    m_fence_values[cpuFrame] = m_commandPool->Execute(*m_command_queue.get());
    m_uploadArena->OnSubmit(m_fence_values[cpuFrame].GetFutureValue());
    m_fence_values[cpuFrame].Wait();
}

void CALLBACK DebugOutputCallback(D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY Severity, D3D12_MESSAGE_ID, LPCSTR pDescription, void*)
{
    switch (Severity)
    {
    case D3D12_MESSAGE_SEVERITY_CORRUPTION:
        LOG(Log::Severity::FATAL, pDescription);
        break;
    case D3D12_MESSAGE_SEVERITY_ERROR:
        LOG(Log::Severity::FATAL, pDescription);
        break;
    case D3D12_MESSAGE_SEVERITY_WARNING:
        LOG(Log::Severity::WARN, pDescription);
        break;
    case D3D12_MESSAGE_SEVERITY_INFO:
        LOG(Log::Severity::INFO, pDescription);
        break;
    case D3D12_MESSAGE_SEVERITY_MESSAGE:
        LOG(Log::Severity::INFO, pDescription);
        break;
    }
}

void SetupDebugOutputToConsole(ComPtr<ID3D12Device5> device)
{
    ComPtr<ID3D12InfoQueue1> infoQueue;
    HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&infoQueue));
    if (FAILED(hr))
    {
        LOG(Log::Severity::FATAL, "Failed to query interface");
    }
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

    D3D12_MESSAGE_ID hide[] = {
        D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
        D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
    };

    D3D12_INFO_QUEUE_FILTER filter = {};
    filter.DenyList.NumIDs = _countof(hide);
    filter.DenyList.pIDList = hide;
    infoQueue->AddStorageFilterEntries(&filter);

    DWORD callbackCookie = 0;
    hr = infoQueue->RegisterMessageCallback(DebugOutputCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &callbackCookie);
    if (FAILED(hr))
    {
        LOG(Log::Severity::FATAL, "Failed to set message callback");
    }
}

void KS::Device::Impl::InitializeDevice(const DeviceInitParams& params)
{
    // CREATE DXGI FACTORY
    auto factory = std::make_unique<KS::DXFactory>(params.debug_context);

    // CREATE DEVICE

    auto selection_criteria = [](CD3DX12FeatureSupport features)
    { return features.MaxSupportedFeatureLevel() >= D3D_FEATURE_LEVEL_12_0; };

    auto device = factory->CreateDevice(DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, selection_criteria).value();

    CheckDX(device.As(&m_device));

    HRESULT hr;

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    hr = m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,
        &options5, sizeof(options5));
    if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
    {
        LOG(Log::Severity::FATAL, "Raytracing not supported on device");
    }

    if (params.debug_context)
    {
        SetupDebugOutputToConsole(m_device);

        // CREATE COMMAND QUEUE
        m_command_queue = std::make_unique<DXCommandQueue>(m_device, L"Main command queue");

        // CREATE DESCRIPTOR HEAPS
        m_descriptor_heaps[RT_HEAP] = DXDescHeap::Construct(m_device, 64, D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            L"MAIN RENDER TARGETS HEAP");
        m_descriptor_heaps[DEPTH_HEAP] = DXDescHeap::Construct(
            m_device, 32, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, L"DEPTH DESCRIPTOR HEAP");
        m_descriptor_heaps[IMGUI_HEAP] =
            DXDescHeap::Construct(m_device, 8, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, L"IMGUI RESOURCE HEAP",
                                  0,
                                  D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
        // CREATE DEPTH STENCIL
        D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
        depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
        depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

        D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
        depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        depthOptimizedClearValue.DepthStencil.Stencil = 0;

        HWND HWNDwindow
            = reinterpret_cast<HWND>(glfwGetWin32Window(m_window));

        // Create Swapchain

        CreateSwapchain(params.window_width, params.window_height, factory.get(), HWNDwindow);
    }

    m_commandPool = std::make_shared<DXCommandContextPool>();
}
