#include <DXDevice.hpp>

// DXDevice::DXDevice(ComPtr<ID3D12Device> device)
// {

//     if (params.debug_context)
//     {
//         SetupDebugOutputToConsole(m_device);

//         // CREATE COMMAND QUEUE
//         m_command_queue = std::make_unique<DXCommandQueue>(m_device, L"Main command queue");

//         // CREATE COMMAND ALLOCATOR
//         for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
//         {
//             m_command_allocator[i] = std::make_shared<DXCommandAllocator>(m_device, ("MAIN COMMAND ALLOCATOR " + std::to_string(i + 1)).c_str());
//         }
//         m_command_list = std::make_unique<DXCommandList>(m_device, m_command_allocator[0], "MAIN COMMAND LIST");

//         // CREATE DESCRIPTOR HEAPS
//         m_descriptor_heaps[RT_HEAP] = DXDescHeap::Construct(m_device, 2000, D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
//             L"MAIN RENDER TARGETS HEAP");
//         m_descriptor_heaps[DEPTH_HEAP] = DXDescHeap::Construct(
//             m_device, 2000, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, L"DEPTH DESCRIPTOR HEAP");
//         m_descriptor_heaps[RESOURCE_HEAP] = DXDescHeap::Construct(
//             m_device, 50000, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, L"RESOURCE HEAP",
//             D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

//         // CREATE DEPTH STENCIL
//         D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
//         depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
//         depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
//         depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

//         D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
//         depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
//         depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
//         depthOptimizedClearValue.DepthStencil.Stencil = 0;

//         HWND HWNDwindow
//             = reinterpret_cast<HWND>(glfwGetWin32Window(m_window));

//         // Create Swapchain

//         DXGI_SWAP_CHAIN_DESC1 swapchain_info = {};

//         swapchain_info.Width = params.window_width;
//         swapchain_info.Height = params.window_height;

//         swapchain_info.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
//         swapchain_info.Stereo = FALSE;

//         swapchain_info.SampleDesc = { 1, 0 };

//         swapchain_info.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
//         swapchain_info.BufferCount = FRAME_BUFFER_COUNT;

//         swapchain_info.Scaling = DXGI_SCALING_STRETCH;
//         swapchain_info.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
//         swapchain_info.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

//         swapchain_info.Flags = 0;
//         if (uncappedFPS)
//             swapchain_info.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

//         ComPtr<IDXGISwapChain1> tempSwapChain;
//         hr = factory->Handle()->CreateSwapChainForHwnd(
//             m_command_queue->Get(), HWNDwindow, &swapchain_info, nullptr, nullptr,
//             &tempSwapChain);
//         if (FAILED(hr))
//         {
//             Log("Failed to create swap chain");
//         }
//         tempSwapChain.As(&m_swapchain);
//     }
// }