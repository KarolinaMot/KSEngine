#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <deque>

class DXCommandList;

namespace KS
{
class Device;

class UploadPage
{
public:
    friend class UploadArena;
    UploadPage();
    ~UploadPage();
    std::uint64_t FreeBytes() const { return m_size - m_head; }
    void ResetHead()
    {
        m_head = 0;
        m_touched = false;
    }

    void* GetResource();

    std::uint8_t* m_cpu = nullptr;    // mapped base
    std::uint64_t m_size = 0;         // total size
    std::uint64_t m_head = 0;         // next free offset
    std::uint64_t m_retireFence = 0;  // fence when page can be recycled
    bool m_touched = false;
    size_t m_index = 0;


private:
    class Impl;
    Impl* m_impl;
};

struct UploadSlice
{
    std::uint8_t* m_cpu = nullptr;  // mapped base
    std::uint64_t m_head = 0;       // next free offset
    size_t m_pageID = 0;
};

class UploadArena 
{
public:
    UploadArena(size_t pageSize);
    ~UploadArena();

    KS::UploadSlice Allocate(const Device& device, DXCommandList& commandList, uint64_t bytes,
                                           uint64_t alignment);
    void OnSubmit(std::uint64_t retireFenceValue);
    void Recycle(std::uint64_t completedFence);
    void* GetPageResource(size_t ID);

 private:
    UploadPage* CreatePage(const Device& device, DXCommandList& commandList, std::uint64_t bytes);
    UploadPage* AcquirePage(const Device& device, DXCommandList& commandList, std::uint64_t minBytes);
    void MarkActive(UploadPage* p);

    std::vector<std::unique_ptr<UploadPage>> m_pages;
    std::uint64_t m_pageSize = 0;

    // State buckets (non-owning pointers into 'pages')
    std::deque<UploadPage*> m_freePages;  // reusable pages (head == 0)
    std::deque<UploadPage*> m_inFlight;   // waiting for fence
    std::vector<UploadPage*> m_active;    // touched since last OnSubmit
    UploadPage* m_current = nullptr;      // page we allocate from
};
}  // namespace KS