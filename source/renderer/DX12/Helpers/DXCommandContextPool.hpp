#pragma once
#include "DXIncludes.hpp"
#include "DXGPUSync.hpp"
#include <queue>
#include <mutex>

class DXCommandList;
class DXCommandAllocator;
class DXCommandQueue;
class DXCommandContextPool;
struct DXCommandContext
{
    DXCommandContext() = default;
    ~DXCommandContext() { Close(); }

    DXCommandContext(DXCommandContext&& other) noexcept
    {
        Close();
        m_commandList = std::move(other.m_commandList);
        m_commandAllocator = std::move(other.m_commandAllocator);
        m_pool = std::move(other.m_pool);
        m_future = std::move(other.m_future);
    }

    DXCommandContext& operator=(DXCommandContext&& other) noexcept
    {
        if (this == &other) return *this;
        Close();
        m_commandList = std::move(other.m_commandList);
        m_commandAllocator = std::move(other.m_commandAllocator);
        m_pool = std::move(other.m_pool);
        m_future = std::move(other.m_future);
        return *this;
    }

    // Delete the copy constructor
    DXCommandContext(const DXCommandContext&) = delete;
    DXCommandContext& operator=(const DXCommandContext&) = delete;

    void Close();

    std::shared_ptr<DXCommandList> m_commandList{};
    std::shared_ptr<DXCommandAllocator> m_commandAllocator{};
    std::weak_ptr<DXCommandContextPool> m_pool{};
    DXGPUFuture m_future{};
};

class DXCommandContextPool : public std::enable_shared_from_this<DXCommandContextPool>
{
    friend DXCommandContext;
    friend std::shared_ptr<DXCommandContextPool>;

public:

    DXCommandContextPool() = default;
    ~DXCommandContextPool() = default;

    DXCommandContext GetCommandSet(ComPtr<ID3D12Device5> device);
    void MarkInFlight(DXCommandContext&& ctx, DXGPUFuture future);
    void RetireCompleted();
    DXGPUFuture Execute(DXCommandQueue& queue);
    
private:
    void CreateCommandSet(ComPtr<ID3D12Device5> device);
    void Close(DXCommandContext&& ctx);

    std::mutex m_mutex;
    std::queue<DXCommandContext> m_free;
    std::deque<DXCommandContext> m_inFlight;
    std::deque<DXCommandContext> m_closed;
};