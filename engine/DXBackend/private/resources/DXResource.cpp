#include <Log.hpp>
#include <resources/DXResource.hpp>

DXResource::MappedAddress DXResource::Map(size_t read_start, size_t read_end, uint32_t subresource)
{
    void* ptr = nullptr;
    D3D12_RANGE range = { read_start, read_end };
    resource->Map(subresource, &range, &ptr);
    return { ptr, subresource, range };
}

DXResource::MappedAddress DXResource::Map(uint32_t subresource)
{
    void* ptr = nullptr;
    resource->Map(subresource, nullptr, &ptr);
    return { ptr, subresource, std::nullopt };
}

void DXResource::Unmap(MappedAddress&& ptr)
{
    D3D12_RANGE* range = ptr.read_range ? &ptr.read_range.value() : nullptr;
    resource->Unmap(ptr.subresource, range);
    ptr.Release();
}

DXResource::MappedAddress::~MappedAddress()
{
    if (ptr != 0)
    {
        Log("Memory Mapping Leak: Forgot to unmap mapped pointer");
    }
}

DXResource::MappedAddress::MappedAddress(MappedAddress&& other)
{
    ptr = other.ptr;
    other.ptr = 0;

    read_range = other.read_range;
    other.read_range = {};

    subresource = other.subresource;
    other.subresource = 0;
}
DXResource::MappedAddress& DXResource::MappedAddress::operator=(MappedAddress&& other)
{
    if (this != &other)
        return *this;

    if (ptr == 0)
    {
        Log("Memory Mapping Leak: Forgot to unmap mapped pointer");
    }

    ptr = other.ptr;
    other.ptr = 0;

    read_range = other.read_range;
    other.read_range = {};

    subresource = other.subresource;
    other.subresource = 0;

    return *this;
}
