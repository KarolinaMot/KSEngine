#include <descriptors/DXDescriptorHandle.hpp>
#include <descriptors/DXDescriptorHeap.hpp>

DXDescriptorHandle::~DXDescriptorHandle()
{
    if (parent_heap)
    {
        parent_heap->FreeHandle(cpu_handle);
    }
}

DXDescriptorHandle::DXDescriptorHandle(DXDescriptorHandle&& other)
{
    cpu_handle = other.cpu_handle;
    parent_heap = other.parent_heap;
    other.parent_heap = nullptr;
}

DXDescriptorHandle& DXDescriptorHandle::operator=(DXDescriptorHandle&& other)
{
    if (this == &other)
        return *this;

    if (parent_heap)
    {
        parent_heap->FreeHandle(cpu_handle);
    }

    cpu_handle = other.cpu_handle;
    parent_heap = other.parent_heap;
    other.parent_heap = nullptr;

    return *this;
}