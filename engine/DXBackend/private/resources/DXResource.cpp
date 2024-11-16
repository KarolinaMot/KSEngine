#include <Log.hpp>
#include <gpu_resources/DXResource.hpp>

DXResource::MappedAddress DXResource::Map(uint32_t subresource, std::optional<D3D12_RANGE> read_range)
{
    void* ptr = nullptr;
    D3D12_RANGE* read = read_range ? &read_range.value() : nullptr;

    resource->Map(subresource, read, &ptr);
    return { ptr, subresource };
}

void DXResource::Unmap(MappedAddress&& ptr, std::optional<D3D12_RANGE> write_range)
{
    D3D12_RANGE* write = write_range ? &write_range.value() : nullptr;
    resource->Unmap(ptr.subresource, write);
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
    other.ptr = nullptr;

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

    subresource = other.subresource;
    other.subresource = 0;

    return *this;
}
