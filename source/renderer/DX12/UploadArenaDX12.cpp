#include <renderer/UploadArena.h>
#include <device/Device.hpp>
#include <renderer/DX12/Helpers/DXResource.hpp>
#include <renderer/DX12/Helpers/DXIncludes.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <tools/Log.hpp>

class KS::UploadPage::Impl
{
public:
    std::unique_ptr<DXResource> m_resource;
};

KS::UploadPage::UploadPage()
{
    m_impl = new Impl();
}

KS::UploadPage::~UploadPage() { delete m_impl; }

void* KS::UploadPage::GetResource() { return m_impl->m_resource.get(); }

static inline std::uint64_t AlignUp(std::uint64_t p, std::uint64_t a) { return (p + (a - 1)) & ~(a - 1); }

KS::UploadArena::UploadArena(size_t pageSize) : m_pageSize(pageSize)
{ 
}

KS::UploadArena::~UploadArena() { }

KS::UploadPage* KS::UploadArena::CreatePage(const Device& device, DXCommandList& commandList, std::uint64_t bytes)
{
    auto p = std::make_unique<UploadPage>();
    p->m_size = bytes;

    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    p->m_impl->m_resource = std::make_unique<DXResource>(engineDevice, CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                                                      CD3DX12_RESOURCE_DESC::Buffer(bytes), nullptr, "Upload heap page");
    commandList.ResourceBarrier(*p->m_impl->m_resource, D3D12_RESOURCE_STATE_COPY_SOURCE);
    p->m_index = m_pages.size();

    HRESULT hr = p->m_impl->m_resource->GetResource()->Map(0, nullptr, reinterpret_cast<void**>(&p->m_cpu));
    if (FAILED(hr))
    {
        LOG(Log::Severity::WARN, "Failure trying to map upload page buffer");
    }

    UploadPage* raw = p.get();
    m_pages.emplace_back(std::move(p));
    return raw;
}

KS::UploadSlice KS::UploadArena::Allocate(const Device& device, DXCommandList& commandList, uint64_t bytes,
                                                    uint64_t alignment)
{
    // Ensure we have a current page with enough space
    if (!m_current || AlignUp(m_current->m_head, alignment) + bytes > m_current->m_size)
    {
        // If current exists and was touched, keep it active; otherwise park it as free.
        if (m_current)
        {
            if (m_current->m_touched)
            {
                // Will be sent to inFlight on OnSubmit()
            }
            else
            {
                // No allocations happened: safe to reuse later
                m_current->ResetHead();
                m_freePages.push_back(m_current);
            }
        }
        // Acquire a page that can fit 'bytes'
        m_current = AcquirePage(device, commandList, bytes);
    }

    // Allocate from current
    std::uint64_t off = AlignUp(m_current->m_head, alignment);
    if (off + bytes > m_current->m_size)
    {
        // Shouldn't happen due to acquirePage(bytes), but guard anyway
        m_current = AcquirePage(device, commandList, bytes);
        off = AlignUp(m_current->m_head, alignment);
    }

    MarkActive(m_current);
    m_current->m_head = off + bytes;

    UploadSlice res;
    res.m_cpu = m_current->m_cpu;
    res.m_head = off;
    res.m_pageID = m_current->m_index;
    return res;
}

void* KS::UploadArena::GetPageResource(size_t ID) { return m_pages[ID]->m_impl->m_resource.get(); }

void KS::UploadArena::OnSubmit(std::uint64_t retireFenceValue)
{
    // Any pages touched since the last submit become in-flight
    for (UploadPage* p : m_active)
    {
        p->m_retireFence = retireFenceValue;
        // Move to inFlight if not already there
        m_inFlight.push_back(p);
    }
    m_active.clear();

    // Drop current so the next Allocate can pick a fresh/free page.
    m_current = nullptr;
}

void KS::UploadArena::Recycle(std::uint64_t completedFence)
{
    // Return pages whose retire fence is done
    auto& q = m_inFlight;
    for (auto it = q.begin(); it != q.end();)
    {
        UploadPage* p = *it;
        if (p->m_retireFence != 0 && p->m_retireFence <= completedFence)
        {
            p->ResetHead();
            m_freePages.push_back(p);
            it = q.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

KS::UploadPage* KS::UploadArena::AcquirePage(const Device& device, DXCommandList& commandList, std::uint64_t minBytes)
{ 
    // Try a reusable page with enough space
    for (auto it = m_freePages.begin(); it != m_freePages.end(); ++it)
    {
        if ((*it)->m_size >= minBytes)
        {
            UploadPage* p = *it;
            m_freePages.erase(it);
            return p;
        }
    }
    // Otherwise create a new one (round up to max(defaultPage, minBytes))
    std::uint64_t sz = std::max(m_pageSize, minBytes);
    return CreatePage(device, commandList, sz);
}

void KS::UploadArena::MarkActive(UploadPage* p)
{
    if (!p->m_touched)
    {
        p->m_touched = true;
        m_active.push_back(p);
    }
}