#pragma once
#include "DXIncludes.hpp"
#include "DXGPUSync.hpp"
#include <queue>
#include <mutex>

class DXCommandList;
class DXCommandAllocator;
class DXCommandQueue;
struct DXCommandContext
{
    std::shared_ptr<DXCommandList> m_commandList;
    std::shared_ptr<DXCommandAllocator> m_commandAllocator;
    DXGPUFuture m_future;
};
class DXCommandContextPool
{
public:

    DXCommandContextPool() = default;
    ~DXCommandContextPool() = default;

    DXCommandContext GetCommandSet(ComPtr<ID3D12Device5> device);
    void MarkInFlight(DXCommandContext&& ctx, DXGPUFuture future);
    void Close(DXCommandContext&& ctx);
    void RetireCompleted();
    DXGPUFuture Execute(DXCommandQueue& queue);
    
private:
    void CreateCommandSet(ComPtr<ID3D12Device5> device);

    std::mutex m_mutex;
    std::queue<DXCommandContext> m_free;
    std::deque<DXCommandContext> m_inFlight;
    std::deque<DXCommandContext> m_closed;
};