
#include <Log.hpp>
#include <resources/DXResourceBuilder.hpp>


std::optional<DXResource> DXResourceBuilder::MakeBuffer(ID3D12Device* device, size_t buffer_size, const wchar_t* name)
{
    ComPtr<ID3D12Resource> resource {};

    CD3DX12_CLEAR_VALUE* clear = clear_value ? &clear_value.value() : nullptr;
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);

    HRESULT result = device->CreateCommittedResource(
        &heap_properties,
        heap_flags,
        &desc,
        initial_state,
        clear,
        IID_PPV_ARGS(&resource));

    if (FAILED(result))
    {
        Log(L"DXResourceBuilder Error: Failed allocating '{}' (Buffer) with size of {}", name, buffer_size);
        return std::nullopt;
    }

    resource->SetName(name);
    return DXResource { resource };
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