
#include <Log.hpp>
#include <gpu_resources/DXResourceBuilder.hpp>

ComPtr<ID3D12Resource> DXResourceBuilder::AllocateResource(ID3D12Device* device, CD3DX12_RESOURCE_DESC& resource_desc) const
{
    ComPtr<ID3D12Resource> resource {};
    const CD3DX12_CLEAR_VALUE* clear = clear_value ? &clear_value.value() : nullptr;

    resource_desc.Flags |= flags;

    HRESULT result = device->CreateCommittedResource(
        &heap_properties,
        heap_flags,
        &resource_desc,
        initial_state,
        clear,
        IID_PPV_ARGS(&resource));

    if (FAILED(result))
    {
        return nullptr;
    }

    return resource;
}

std::optional<DXResource> DXResourceBuilder::MakeBuffer(ID3D12Device* device, size_t buffer_size, const wchar_t* name) const
{
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);
    auto resource = AllocateResource(device, desc);

    if (resource == nullptr)
    {
        Log(L"DXResourceBuilder Error: Failed allocating '{}' (Buffer) with size of {}", name, buffer_size);
        return std::nullopt;
    }

    resource->SetName(name);
    return DXResource { resource, glm::uvec3 { buffer_size, 0, 0 } };
}

std::optional<DXResource> DXResourceBuilder::MakeTexture2D(ID3D12Device* device, glm::uvec2 size, DXGI_FORMAT format, const wchar_t* name) const
{
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(format, size.x, size.y);
    auto resource = AllocateResource(device, desc);

    if (resource == nullptr)
    {
        Log(L"DXResourceBuilder Error: Failed allocating '{}' (Render Target) with dimensions of {}", name, size.x, size.y);
        return std::nullopt;
    }

    resource->SetName(name);
    return DXResource { resource, glm::uvec3 { size.x, size.y, 0 } };
}

DXResourceBuilder& DXResourceBuilder::WithHeapType(D3D12_HEAP_TYPE type)
{
    heap_properties.Type = type;
    return *this;
}

DXResourceBuilder& DXResourceBuilder::WithHeapFlags(D3D12_HEAP_FLAGS flags)
{
    heap_flags = flags;
    return *this;
}

DXResourceBuilder& DXResourceBuilder::WithOptimizedClearColour(std::optional<CD3DX12_CLEAR_VALUE> value)
{
    clear_value = value;
    return *this;
}

DXResourceBuilder& DXResourceBuilder::WithInitialState(D3D12_RESOURCE_STATES state)
{
    initial_state = state;
    return *this;
}

DXResourceBuilder& DXResourceBuilder::WithResourceFlags(D3D12_RESOURCE_FLAGS resource_flags)
{
    flags = resource_flags;
    return *this;
}