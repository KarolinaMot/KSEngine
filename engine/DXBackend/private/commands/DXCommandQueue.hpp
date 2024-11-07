
// #pragma once

// #include "DXGPUSync.hpp"
// #include <memory>
// #include <renderer/DX12/Helpers/DXIncludes.hpp>
// #include <string>

// class DXCommandList;
// class DXCommandQueue
// {
// public:
//     DXCommandQueue(const ComPtr<ID3D12Device5>& device, const std::wstring& name);
//     ~DXCommandQueue();

//     // Waits the calling thread until all pending operations are executed
//     void Flush();

//     DXGPUFuture ExecuteCommandLists(const DXCommandList** ppCommandLists, uint32_t commandListCount = 1);

//     ID3D12CommandQueue* Get() const
//     {
//         return m_command_queue.Get();
//     }

// private:
//     ComPtr<ID3D12CommandQueue> m_command_queue;
//     std::shared_ptr<DXGPUFence> m_fence {};
//     uint64_t m_next_fence_value = 0;
//};