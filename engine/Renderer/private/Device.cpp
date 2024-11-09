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
