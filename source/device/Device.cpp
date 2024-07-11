#include "Device.hpp"
#include "renderer/Buffer.hpp"
#include "renderer/DX12/Helpers/DXDescHeap.hpp"
#include "renderer/DX12/Helpers/DXHeapHandle.hpp"
#include "renderer/DX12/Helpers/DXIncludes.hpp"
#include "renderer/DX12/Helpers/DXResource.hpp"
#include <concurrency/CommandQueue.hpp>
#include <tools/Log.hpp>

#include "DX12/DXFactory.hpp"

#include <thread>
#include <vector>

class KS::Device::Impl
{
public:
    void InitializeWindow(const DeviceInitParams& params);
    void InitializeDevice(const DeviceInitParams& params);
    UINT GetFramebufferIndex();

    void BindSwapchainRT();
    void StartFrame(int frameIndex, glm::vec4 clearColor);
    void EndFrame();

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
        RESOURCE_HEAP,
        NUM_DESC_HEAPS
    };

    GLFWwindow* m_window;
    GLFWmonitor* m_monitor;
    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissor_rect;

    ComPtr<ID3D12Device5> m_device;
    ComPtr<IDXGISwapChain3> m_swapchain;

    std::unique_ptr<CommandQueue> m_command_queue;
    ComPtr<ID3D12CommandAllocator> m_command_allocator[FRAME_BUFFER_COUNT];
    ComPtr<ID3D12GraphicsCommandList4> m_command_list;
    GPUFuture m_fence_values[FRAME_BUFFER_COUNT];
    int m_cpu_frame = 0;
    int m_gpu_frame = 0;

    std::unique_ptr<DXResource> m_resources[NUM_RESOURCES];
    std::shared_ptr<DXDescHeap> m_descriptor_heaps[NUM_DESC_HEAPS];
    DXHeapHandle m_rt_handle[FRAME_BUFFER_COUNT];
    DXHeapHandle m_depth_handle;
    const DXGI_FORMAT m_depth_format = DXGI_FORMAT_D32_FLOAT;
};

KS::Device::Device(const DeviceInitParams& params)
{
    m_impl = std::make_unique<Impl>();
    m_fullscreen = false;
    m_prev_width = params.window_width;
    m_prev_height = params.window_height;
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
    return m_impl->m_device.Get();
}

void* KS::Device::GetCommandList() const
{
    return m_impl->m_command_list.Get();
}

void* KS::Device::GetResourceHeap() const
{
    return m_impl->m_descriptor_heaps[Impl::DXHeaps::RESOURCE_HEAP].get();
}
void* KS::Device::GetWindowHandle() const
{
    return m_impl->m_window;
}

void KS::Device::NewFrame()
{
    m_window_open = !glfwWindowShouldClose(m_impl->m_window);
    m_frame_index = m_impl->GetFramebufferIndex();
    m_impl->StartFrame(m_frame_index, m_clear_color);
}

void KS::Device::EndFrame()
{
    glfwSwapBuffers(m_impl->m_window);
    m_impl->EndFrame();
    m_frame_resources[m_impl->m_gpu_frame].clear();
}

void KS::Device::TrackResource(std::shared_ptr<void> buffer)
{
    m_frame_resources[m_frame_index].push_back(buffer);
}

void KS::Device::Flush()
{
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

    m_viewport.Width = params.window_width;
    m_viewport.Height = params.window_height;

    std::string applicationName = params.name;
    if (applicationName.empty())
    {
        applicationName += "Unnamed application";
    }

    glfwWindowHint(GLFW_RESIZABLE, 1);
    m_window = glfwCreateWindow(static_cast<int>(m_viewport.Width), static_cast<int>(m_viewport.Height), applicationName.c_str(), nullptr, nullptr);

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

void KS::Device::Impl::BindSwapchainRT()
{
    m_command_list->RSSetViewports(1, &m_viewport);
    m_command_list->RSSetScissorRects(1, &m_scissor_rect);
    m_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_resources[m_cpu_frame]->ChangeState(m_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_descriptor_heaps[RT_HEAP]->BindRenderTargets(m_command_list, &m_rt_handle[m_cpu_frame], m_depth_handle);
}

void KS::Device::Impl::StartFrame(int frameIndex, glm::vec4 clearColor)
{
    m_gpu_frame = frameIndex;
    m_cpu_frame = (frameIndex + 1) % FRAME_BUFFER_COUNT;

    // Wait until the current swapchain is available;
    m_fence_values->Wait();

    if (FAILED(m_command_allocator[m_cpu_frame]->Reset()))
    {
        LOG(Log::Severity::FATAL, "Failed to reset command allocator");
    }

    if (FAILED(m_command_list->Reset(m_command_allocator[m_cpu_frame].Get(), nullptr)))
    {
        LOG(Log::Severity::FATAL, "Failed to reset command list");
    }

    BindSwapchainRT();

    m_descriptor_heaps[RT_HEAP]->ClearRenderTarget(m_command_list, m_rt_handle[m_cpu_frame], &clearColor[0]);
    m_descriptor_heaps[DEPTH_HEAP]->ClearDepthStencil(m_command_list, m_depth_handle);
}

void KS::Device::Impl::EndFrame()
{
    m_resources[m_cpu_frame]->ChangeState(m_command_list, D3D12_RESOURCE_STATE_PRESENT);

    // CLOSE COMMAND LIST
    m_command_list->Close();
    ID3D12CommandList* ppCommandLists[] = { m_command_list.Get() };

    // PRESENT
    if (FAILED(m_swapchain->Present(0, 0)))
    {
        LOG(Log::Severity::FATAL, "Failed to present");
    }

    m_fence_values[m_cpu_frame] = m_command_queue->ExecuteCommandLists(ppCommandLists, 1);
    // m_command_queue->WaitForFenceValue(m_fence_values[m_gpu_frame]);
}

void CALLBACK DebugOutputCallback(D3D12_MESSAGE_CATEGORY Category, D3D12_MESSAGE_SEVERITY Severity, D3D12_MESSAGE_ID ID, LPCSTR pDescription, void* pContext)
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
    bool uncappedFPS = factory->SupportsTearing();

    // CREATE DEVICE

    auto selection_criteria = [](CD3DX12FeatureSupport features)
    { return features.MaxSupportedFeatureLevel() >= D3D_FEATURE_LEVEL_12_0; };

    auto device = factory->CreateDevice(DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, selection_criteria).value();

    CheckDX(device.As(&m_device));

    m_viewport.TopLeftX = 0;
    m_viewport.TopLeftY = 0;
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;

    m_scissor_rect.left = 0;
    m_scissor_rect.top = 0;
    m_scissor_rect.right = static_cast<LONG>(m_viewport.Width);
    m_scissor_rect.bottom = static_cast<LONG>(m_viewport.Height);

    HRESULT hr;

    if (params.debug_context)
    {
        SetupDebugOutputToConsole(m_device);
    }

    // CREATE COMMAND QUEUE
    m_command_queue = std::make_unique<CommandQueue>(m_device, L"Main command queue");

    // CREATE COMMAND ALLOCATOR
    for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        hr = m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_command_allocator[i]));
        if (FAILED(hr))
        {
            LOG(Log::Severity::FATAL, "Failed to create command allocator");
        }
    }

    // CREATE COMMAND LIST
    hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_command_allocator[0].Get(), NULL,
        IID_PPV_ARGS(&m_command_list));
    if (FAILED(hr))
    {
        LOG(Log::Severity::FATAL, "Failed to create command list");
    }
    m_command_list->SetName(L"Main command list");

    // CREATE DESCRIPTOR HEAPS
    m_descriptor_heaps[RT_HEAP] = DXDescHeap::Construct(m_device, 2000, D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        L"MAIN RENDER TARGETS HEAP");
    m_descriptor_heaps[DEPTH_HEAP] = DXDescHeap::Construct(
        m_device, 2000, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, L"DEPTH DESCRIPTOR HEAP");
    m_descriptor_heaps[RESOURCE_HEAP] = DXDescHeap::Construct(
        m_device, 50000, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, L"RESOURCE HEAP",
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

    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        m_depth_format, static_cast<UINT>(m_viewport.Width),
        static_cast<UINT>(m_viewport.Height), 1, 1, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    m_resources[DEPTH_STENCIL_RSC] = std::make_unique<DXResource>(
        m_device, heapProperties, resourceDesc, &depthOptimizedClearValue,
        "Depth/Stencil Resource");
    m_resources[DEPTH_STENCIL_RSC]->ChangeState(m_command_list,
        D3D12_RESOURCE_STATE_DEPTH_WRITE);
    m_depth_handle = m_descriptor_heaps[DEPTH_HEAP]->AllocateDepthStencil(
        m_resources[DEPTH_STENCIL_RSC].get(), m_device.Get(), &depthStencilDesc);

    m_command_list->Close();
    ID3D12CommandList* ppCommandLists[] = { m_command_list.Get() };

    auto frame_setup = m_command_queue->ExecuteCommandLists(ppCommandLists, 1);
    frame_setup.Wait();

    HWND HWNDwindow
        = reinterpret_cast<HWND>(glfwGetWin32Window(m_window));

    // Create Swapchain

    DXGI_SWAP_CHAIN_DESC1 swapchain_info = {};

    swapchain_info.Width = params.window_width;
    swapchain_info.Height = params.window_height;

    swapchain_info.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchain_info.Stereo = FALSE;

    swapchain_info.SampleDesc = { 1, 0 };

    swapchain_info.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_info.BufferCount = FRAME_BUFFER_COUNT;

    swapchain_info.Scaling = DXGI_SCALING_STRETCH;
    swapchain_info.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchain_info.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

    swapchain_info.Flags = 0;
    if (uncappedFPS)
        swapchain_info.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    ComPtr<IDXGISwapChain1> tempSwapChain;
    hr = factory->Handle()->CreateSwapChainForHwnd(
        m_command_queue->Get(), HWNDwindow, &swapchain_info, nullptr, nullptr,
        &tempSwapChain);
    if (FAILED(hr))
    {
        LOG(Log::Severity::FATAL, "Failed to create swap chain");
    }
    tempSwapChain.As(&m_swapchain);

    // CREATE RENDER TARGETS
    for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        m_resources[RT + i] = std::make_unique<DXResource>();
        ComPtr<ID3D12Resource> res;
        hr = m_swapchain->GetBuffer(i, IID_PPV_ARGS(&res));
        if (FAILED(hr))
        {
            LOG(Log::Severity::FATAL, "Failed to get swapchain buffer");
        }
        m_resources[RT + i]->SetResource(res);
        m_rt_handle[i] = m_descriptor_heaps[RT_HEAP]->AllocateRenderTarget(
            m_resources[RT + i].get(), m_device.Get(), nullptr);
        m_resources[RT + i]->GetResource()->SetName(L"RENDER TARGET");
    }
}
