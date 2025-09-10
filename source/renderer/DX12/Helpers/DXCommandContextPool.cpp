#include "DXCommandContextPool.hpp"
#include "DXCommandList.hpp"
#include "DXGPUSync.hpp"
#include "DXCommandQueue.hpp"

DXCommandContext DXCommandContextPool::GetCommandSet(ComPtr<ID3D12Device5> device)
{ 
   std::lock_guard<std::mutex> lock(m_mutex);
   if (m_free.size() == 0)
       CreateCommandSet(device);

   DXCommandContext res = std::move(m_free.front());
   res.m_commandList->Open(res.m_commandAllocator);
   m_free.pop();
   return res;
}

void DXCommandContextPool::MarkInFlight(DXCommandContext&& ctx, DXGPUFuture future)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    ctx.m_future = future;
    m_inFlight.push_back(std::move(ctx));
}

void DXCommandContextPool::Close(DXCommandContext&& ctx)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    ctx.m_commandList->Close();
    m_closed.push_back(std::move(ctx));
}

void DXCommandContextPool::RetireCompleted()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto ctx : m_inFlight)
    {
        if (!ctx.m_future.IsComplete()) continue;

        ctx.m_commandAllocator->Reset();
        m_inFlight.pop_front();
        m_free.push(std::move(ctx));
    }
}

DXGPUFuture DXCommandContextPool::Execute(DXCommandQueue& queue)
{
    std::vector<DXCommandContext> batch;
    std::vector<ID3D12CommandList*> raw;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const size_t count = m_closed.size();
        if (count == 0) return {};  // return default/invalid future

        batch.reserve(count);
        raw.reserve(count);

        // Move out of m_closed into a local batch; collect raw pointers
        while (!m_closed.empty())
        {
            batch.push_back(std::move(m_closed.front()));
            m_closed.pop_front();  // or pop() if queue, erase(begin()) if vector/deque
            raw.push_back(batch.back().m_commandList->GetCommandList().Get());
        }
    }  // lock released before Execute

    // 2) Submit once for the whole batch (no lock held).
    DXGPUFuture fut = queue.ExecuteCommandLists(raw.data(), static_cast<uint32_t>(raw.size()));

    // 3) Tag futures and put into m_inFlight (under lock).
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& ctx : batch)
        {
            ctx.m_future = fut;  // store a copy per context
            m_inFlight.push_back(std::move(ctx));
        }
    }

    return fut;  // caller can Wait()/IsComplete() on this copy}
}

void DXCommandContextPool::CreateCommandSet(ComPtr<ID3D12Device5> device)
{ 
	DXCommandContext context;
    context.m_commandAllocator = std::make_shared<DXCommandAllocator>(device, ("CommandAllocator" + std::to_string(m_free.size())).c_str());
    context.m_commandList = std::make_shared<DXCommandList>(device, context.m_commandAllocator,
                                                                ("CommandList" + std::to_string(m_free.size())).c_str());
	m_free.push(std::move(context));
}
