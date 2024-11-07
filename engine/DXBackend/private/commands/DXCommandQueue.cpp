// #include "DXCommandQueue.hpp"
// #include "DXCommandList.hpp"

// DXCommandQueue::DXCommandQueue(const ComPtr<ID3D12Device5>& device, const std::wstring& name)
// {
//     D3D12_COMMAND_QUEUE_DESC desc = {};

//     CheckDX(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_command_queue)));
//     m_command_queue->SetName(name.c_str());

//     m_fence = std::make_shared<DXGPUFence>(device);
// }

// DXCommandQueue::~DXCommandQueue()
// {
//     Flush();
// }

// void DXCommandQueue::Flush()
// {
//     m_fence->Signal(m_command_queue, ++m_next_fence_value);
//     m_fence->WaitFor(m_next_fence_value);
// }

// DXGPUFuture DXCommandQueue::ExecuteCommandLists(const DXCommandList** ppCommandLists, uint32_t commandListCount)
// {
//     ID3D12CommandList* commandLists[20];
//     for (int i = 0; i < commandListCount; i++)
//     {
//         commandLists[i] = ppCommandLists[i]->GetCommandList().Get();
//     }

//     m_command_queue->ExecuteCommandLists(commandListCount, commandLists);
//     m_fence->Signal(m_command_queue, ++m_next_fence_value);

//     return DXGPUFuture(m_fence, m_next_fence_value);
// }
