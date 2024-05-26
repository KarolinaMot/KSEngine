#include "Device.hpp"
#include "renderer/DX12/DXIncludes.hpp"
#include "renderer/DX12/DXResource.hpp"
#include "renderer/DX12/DXDescHeap.hpp"
#include "renderer/DX12/DXHeapHandle.hpp"
#include "renderer/DX12/DXCommandQueue.hpp"
#include "../tools/Log.hpp"
#include <vector>
#include <thread>

class KS::Device::Impl
{
public:
    void InitializeWindow(const DeviceInitParams &params);
    void InitializeDevice(const DeviceInitParams &params);
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

    GLFWwindow *m_window;
    GLFWmonitor *m_monitor;
    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissor_rect;

    ComPtr<ID3D12Device5> m_device;
    ComPtr<IDXGISwapChain3> m_swapchain;

    std::unique_ptr<DXCommandQueue> m_command_queue;
    ComPtr<ID3D12CommandAllocator> m_command_allocator[FRAME_BUFFER_COUNT];
    ComPtr<ID3D12GraphicsCommandList4> m_command_list;
    int m_fence_values[FRAME_BUFFER_COUNT];
    int m_cpu_frame = 0;
    int m_gpu_frame = 0;

    std::unique_ptr<DXResource> m_resources[NUM_RESOURCES];
    std::shared_ptr<DXDescHeap> m_descriptor_heaps[NUM_DESC_HEAPS];
    DXHeapHandle m_rt_handle[FRAME_BUFFER_COUNT];
    DXHeapHandle m_depth_handle;
    const DXGI_FORMAT m_depth_format = DXGI_FORMAT_D32_FLOAT;
};

KS::Device::Device(const DeviceInitParams &params)
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

void *KS::Device::GetDevice()
{
    return m_impl->m_device.Get();
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
    glfwPollEvents();
}

void window_close_callback(GLFWwindow *window)
{
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void KS::Device::Impl::InitializeWindow(const DeviceInitParams &params)
{
    std::cout << "Initializing window..." << std::endl;

    if (!glfwInit())
    {
        LOG(Log::Severity::FATAL, "GLFW could not be initialized");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    m_monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(m_monitor);

    auto maxScreenWidth = mode->width;
    auto maxScreenHeight = mode->height;

    m_viewport.Width = 1920;
    m_viewport.Height = 1080;

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
    ID3D12CommandList *ppCommandLists[] = {m_command_list.Get()};

    // PRESENT
    if (FAILED(m_swapchain->Present(0, 0)))
    {
        LOG(Log::Severity::FATAL, "Failed to present");
    }

    m_fence_values[m_cpu_frame] = m_command_queue->ExecuteCommandLists(ppCommandLists, 1);
    m_command_queue->WaitForFenceValue(m_fence_values[m_gpu_frame]);
}

void CALLBACK DebugOutputCallback(D3D12_MESSAGE_CATEGORY Category, D3D12_MESSAGE_SEVERITY Severity, D3D12_MESSAGE_ID ID, LPCSTR pDescription, void *pContext)
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

    D3D12_MESSAGE_ID hide[] =
        {
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

void KS::Device::Impl::InitializeDevice(const DeviceInitParams &params)
{
    // DEBUG LAYERS
    if (params.debug_context)
    {
        Microsoft::WRL::ComPtr<ID3D12Debug> debugInterface;
        if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface))))
        {
            LOG(Log::Severity::FATAL, "Failed to get debug interface");
        }
        debugInterface->EnableDebugLayer();
    }

    m_viewport.TopLeftX = 0;
    m_viewport.TopLeftY = 0;
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;

    m_scissor_rect.left = 0;
    m_scissor_rect.top = 0;
    m_scissor_rect.right = static_cast<LONG>(m_viewport.Width);
    m_scissor_rect.bottom = static_cast<LONG>(m_viewport.Height);

    ComPtr<IDXGIFactory4> dxgiFactory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
    if (FAILED(hr))
    {
        LOG(Log::Severity::FATAL, "Failed to create dxgi factory");
    }

    ComPtr<IDXGIAdapter1> adapter;
    int adapterIndex = 0;
    bool adapterFound = false;
    while (dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            adapterIndex++;
            continue;
        }

        hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr);
        if (SUCCEEDED(hr))
        {

            hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
            if (FAILED(hr))
            {
                LOG(Log::Severity::FATAL, "Failed to create render device");
            }

            D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
            hr = m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,
                                               &options5, sizeof(options5));

            if (FAILED(hr))
            {
                LOG(Log::Severity::FATAL, "Failed to pick adaptor");
            }

            if (options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
            {
                adapterFound = false;
                break;
            }
        }

        adapterIndex++;
    }

    if (params.debug_context)
    {
        SetupDebugOutputToConsole(m_device);
    }

    // CREATE COMMAND QUEUE
    m_command_queue = std::make_unique<DXCommandQueue>(m_device, L"Main command queue");

    // CREATE COMMAND ALLOCATOR
    for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_command_allocator[i]));
        if (FAILED(hr))
        {
            LOG(Log::Severity::FATAL, "Failed to create command allocator");
        }
    }

    // CREATE COMMAND LIST
    hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_command_allocator[0].Get(), NULL, IID_PPV_ARGS(&m_command_list));
    if (FAILED(hr))
    {
        LOG(Log::Severity::FATAL, "Failed to create command list");
    }
    m_command_list->SetName(L"Main command list");

    // CREATE SWAPCHAIN
    DXGI_MODE_DESC backBufferDesc = {};
    backBufferDesc.Width = static_cast<UINT>(m_viewport.Width);
    backBufferDesc.Height = static_cast<UINT>(m_viewport.Height);
    backBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    DXGI_SAMPLE_DESC sampleDesc = {};
    sampleDesc.Count = 1;
    sampleDesc.Quality = 0;

    HWND HWNDwindow = reinterpret_cast<HWND>(glfwGetWin32Window(m_window));
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc = backBufferDesc;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.OutputWindow = HWNDwindow;
    swapChainDesc.SampleDesc = sampleDesc;
    swapChainDesc.Windowed = true;

    IDXGISwapChain *tempSwapChain;
    hr = dxgiFactory->CreateSwapChain(m_command_queue->Get(), &swapChainDesc, &tempSwapChain);
    if (FAILED(hr))
    {
        LOG(Log::Severity::FATAL, "Failed to create swap chain");
    }
    m_swapchain = static_cast<IDXGISwapChain3 *>(tempSwapChain);

    // CREATE DESCRIPTOR HEAPS
    m_descriptor_heaps[RT_HEAP] = DXDescHeap::Construct(m_device, 2000, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, L"MAIN RENDER TARGETS HEAP");
    m_descriptor_heaps[DEPTH_HEAP] = DXDescHeap::Construct(m_device, 2000, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, L"DEPTH DESCRIPTOR HEAP");
    m_descriptor_heaps[RESOURCE_HEAP] = DXDescHeap::Construct(m_device, 50000, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, L"RESOURCE HEAP", D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

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
        m_rt_handle[i] = m_descriptor_heaps[RT_HEAP]->AllocateRenderTarget(m_resources[RT + i].get(), m_device.Get(), nullptr);
        m_resources[RT + i]->GetResource()->SetName(L"RENDER TARGET");
    }

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
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(m_depth_format, static_cast<UINT>(m_viewport.Width), static_cast<UINT>(m_viewport.Height), 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    m_resources[DEPTH_STENCIL_RSC] = std::make_unique<DXResource>(m_device, heapProperties, resourceDesc, &depthOptimizedClearValue, "Depth/Stencil Resource");
    m_resources[DEPTH_STENCIL_RSC]->ChangeState(m_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    m_depth_handle = m_descriptor_heaps[DEPTH_HEAP]->AllocateDepthStencil(m_resources[DEPTH_STENCIL_RSC].get(), m_device.Get(), &depthStencilDesc);

    m_command_list->Close();
    ID3D12CommandList *ppCommandLists[] = {m_command_list.Get()};
    m_fence_values[0] = m_command_queue->ExecuteCommandLists(ppCommandLists, 1);
}
