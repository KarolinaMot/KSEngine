#pragma once
#include <Common.hpp>
#include <DXCommon.hpp>

class DXDescriptorHeap;

// TODO: if necessary, add gpu descriptor handle as a member as well
class DXDescriptorHandle
{
public:
    DXDescriptorHandle() = default;

    DXDescriptorHandle(DXDescriptorHeap* heap, CD3DX12_CPU_DESCRIPTOR_HANDLE handle)
        : parent_heap(heap)
        , cpu_handle(handle)
    {
    }

    ~DXDescriptorHandle();

    DXDescriptorHandle(DXDescriptorHandle&&);
    DXDescriptorHandle& operator=(DXDescriptorHandle&&);

    NON_COPYABLE(DXDescriptorHandle);

    CD3DX12_CPU_DESCRIPTOR_HANDLE GetCPUHandle() const { return cpu_handle; }

private:
    DXDescriptorHeap* parent_heap {};
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle {};
};
