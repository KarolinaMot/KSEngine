#include "DX12/DXFactory.hpp"
#include <Device.hpp>
#include <Log.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXCommandQueue.hpp>
#include <renderer/DX12/Helpers/DXDescHeap.hpp>
#include <renderer/DX12/Helpers/DXHeapHandle.hpp>
#include <renderer/DX12/Helpers/DXIncludes.hpp>
#include <renderer/DX12/Helpers/DXResource.hpp>

#include "Device.hpp"
#include <thread>
#include <vector>

class Device::Impl
{
public:
    void InitializeWindow(const DeviceInitParams& params);
    void InitializeDevice(const DeviceInitParams& params);
    UINT GetFramebufferIndex();

    // void BindSwapchainRT();
};

Device::Device(const DeviceInitParams& params)
{
    m_impl = std::make_unique<Impl>();
    m_fullscreen = false;
    m_width = params.window_width;
    m_height = params.window_height;
    m_impl->InitializeWindow(params);
    m_window_open = true;
    m_clear_color = params.clear_color;
    m_impl->InitializeDevice(params);
    m_frame_index = 0;
}

Device::~Device()
{
    Flush();
}

void* Device::GetDevice() const
{
    return m_impl->m_device.Get();
}

void* Device::GetCommandList() const
{
    return m_impl->m_command_list.get();
}

void Device::NewFrame()
{
    m_window_open = !glfwWindowShouldClose(m_impl->m_window);
    m_frame_index = m_impl->GetFramebufferIndex();
    m_cpu_frame = (m_frame_index + 1) % FRAME_BUFFER_COUNT;
    m_impl->StartFrame(m_frame_index, m_cpu_frame, m_clear_color);
    m_swapchainRT->Bind(*this, m_swapchainDS.get());
    m_swapchainRT->Clear(*this);
    m_swapchainDS->Clear(*this);
}

void Device::EndFrame()
{
    glfwSwapBuffers(m_impl->m_window);
    m_swapchainRT->PrepareToPresent(*this);
    m_impl->EndFrame(m_cpu_frame);
}

void Device::InitializeSwapchain()
{
    // CREATE RENDER TARGETS
    for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        ComPtr<ID3D12Resource> res;
        HRESULT hr = m_impl->m_swapchain->GetBuffer(i, IID_PPV_ARGS(&res));
        if (FAILED(hr))
        {
            Log("Failed to get swapchain buffer");
        }

        m_swapchainTex[i] = std::make_shared<Texture>(*this, res.Get(), glm::vec2(m_width, m_height), Texture::RENDER_TARGET);
    }

    m_swapchainRT = std::make_shared<RenderTarget>();
    m_swapchainRT->AddTexture(*this, m_swapchainTex[0], m_swapchainTex[1], "Swapchain render target");

    m_swapchainDepthTex = std::make_shared<Texture>(*this, m_width, m_height, Texture::DEPTH_TEXTURE, glm::vec4(1.f), Formats::D32_FLOAT);
    m_swapchainDS = std::make_shared<DepthStencil>(*this, m_swapchainDepthTex);
}

void Device::FinishInitialization()
{
    m_impl->m_command_list->Close();

    const DXCommandList* commandLists[] = { m_impl->m_command_list.get() };
    auto frame_setup = m_impl->m_command_queue->ExecuteCommandLists(&commandLists[0], 1);
    frame_setup.Wait();
}

void Device::TrackResource(std::shared_ptr<void> buffer)
{
}

void Device::Flush()
{
    m_impl->m_command_queue->Flush();
}

UINT Device::Impl::GetFramebufferIndex()
{
    return m_swapchain->GetCurrentBackBufferIndex();
}

void Device::Impl::StartFrame(int frameIndex, int cpuFrame, glm::vec4 clearColor)
{
    // Wait until the current swapchain is available;
    m_fence_values->Wait();

    m_command_allocator[cpuFrame]->Reset();
    m_command_list->Open(m_command_allocator[cpuFrame]);
    m_command_list->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Device::Impl::EndFrame(int cpuFrame)
{
    // CLOSE COMMAND LIST
    m_command_list->Close();

    // PRESENT
    if (FAILED(m_swapchain->Present(0, 0)))
    {
        Log("Failed to present");
    }

    const DXCommandList* commandLists[] = { m_command_list.get() };

    m_fence_values[cpuFrame] = m_command_queue->ExecuteCommandLists(&commandLists[0], 1);
}

void Device::Impl::InitializeDevice(const DeviceInitParams& params)
{
    // CREATE DXGI FACTORY
    auto factory = std::make_unique<DXFactory>(params.debug_context);
    bool uncappedFPS = factory->SupportsTearing();

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
        Log("Raytracing not supported on device");
    }

    if (params.debug_context)
    {
        SetupDebugOutputToConsole(m_device);

        // CREATE COMMAND QUEUE
        m_command_queue = std::make_unique<DXCommandQueue>(m_device, L"Main command queue");

        // CREATE COMMAND ALLOCATOR
        for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
        {
            m_command_allocator[i] = std::make_shared<DXCommandAllocator>(m_device, ("MAIN COMMAND ALLOCATOR " + std::to_string(i + 1)).c_str());
        }
        m_command_list = std::make_unique<DXCommandList>(m_device, m_command_allocator[0], "MAIN COMMAND LIST");

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
            Log("Failed to create swap chain");
        }
        tempSwapChain.As(&m_swapchain);
    }
}
