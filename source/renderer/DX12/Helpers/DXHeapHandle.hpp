#pragma once
#include "DXDescHeap.hpp"
#include <optional>

class DXHeapHandle
{
    friend DXDescHeap;

public:
    // Creates a heap handle that is not bound to anything
    DXHeapHandle() = default;

    DXHeapHandle(DXHeapHandle&& other) noexcept
    {
        FreeResource();
        mIndex = other.mIndex;
        other.mIndex = 0;
        mDescHeap = other.mDescHeap;
        other.mDescHeap.reset();
    }

    DXHeapHandle& operator=(DXHeapHandle&& other) noexcept
    {
        if (this == &other)
            return *this;
        FreeResource();

        mIndex = other.mIndex;
        other.mIndex = 0;
        mDescHeap = other.mDescHeap;
        other.mDescHeap.reset();
        return *this;
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE GetAddressCPU() const
    {
        if (auto lock = mDescHeap.lock())
            return CD3DX12_CPU_DESCRIPTOR_HANDLE(lock->Get()->GetCPUDescriptorHandleForHeapStart(), mIndex.value(),
                                                 lock->GetDescriptorSize());
        return {};
    }

    CD3DX12_GPU_DESCRIPTOR_HANDLE GetAddressGPU() const
    {
        if (auto lock = mDescHeap.lock())
            return CD3DX12_GPU_DESCRIPTOR_HANDLE(lock->Get()->GetGPUDescriptorHandleForHeapStart(), mIndex.value(),
                                                 lock->GetDescriptorSize());
        return {};
    }

    ~DXHeapHandle()
    {
        FreeResource();
    }

    // Delete the copy constructor
    DXHeapHandle(const DXHeapHandle&) = delete;
    DXHeapHandle& operator=(const DXHeapHandle&) = delete;

    bool IsValid() const { return mIndex.has_value(); }
    uint32_t GetIndex() const { return mIndex.value(); }

    private:
    DXHeapHandle(uint32_t index, std::weak_ptr<DXDescHeap> descHeap)
        : mIndex(index)
        , mDescHeap(descHeap) {};

    void FreeResource()
    {
        if (!mIndex.has_value() || mIndex.value() < OTHER_RESOURCES_START) return;

        if (auto lock = mDescHeap.lock())
        {
            lock->DeallocateResource(mIndex.value());
        }
    };

    std::optional<uint32_t> mIndex;
    std::weak_ptr<DXDescHeap> mDescHeap;
};
