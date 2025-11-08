#include "Skydome.hpp"
#include <device/Device.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <resources/Image.hpp>
#include <renderer/ShaderInputBlueprint.hpp>

#include <renderer/DX12/Helpers/DXResource.hpp>
#include <renderer/DX12/Helpers/DXHeapHandle.hpp>
#include <renderer/UploadArena.h>

class KS::Skydome::Impl
{
public:
    std::unique_ptr<DXResource> m_cubemap{};
    DXHeapHandle mSRVHeapSlot{};
    DXHeapHandle mUAVHeapSlot[4]{};
    UploadSlice m_slice;
};

KS::Skydome::Skydome(Device& device, const Texture& tex) : ShaderInput()
{
    m_impl = std::make_unique<Impl>();   
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    auto descriptorHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());

    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    m_width = tex.GetWidth() / 4;
    m_height = tex.GetHeight() / 2;

    auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, m_width, m_height, 6, m_width <= 5 ? 1 : 4, 1, 0, flags);
    CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    m_impl->m_cubemap = std::make_unique<DXResource>(engineDevice, heapProperties, resourceDesc, nullptr, "Cubemap Resource");
    m_mipLevels = resourceDesc.MipLevels;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = m_impl->m_cubemap->GetDesc().Format;
    srvDesc.TextureCube.MipLevels = m_impl->m_cubemap->GetDesc().MipLevels;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    m_impl->mSRVHeapSlot = descriptorHeap->AllocateResource(m_impl->m_cubemap.get(), &srvDesc);

    for (int i = 0; i < 4; i++)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = m_impl->m_cubemap->GetDesc().Format;
        uavDesc.Texture2DArray.ArraySize = 6;
        uavDesc.Texture2DArray.MipSlice = i;
        uavDesc.Texture2DArray.FirstArraySlice = 0;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
        m_impl->mUAVHeapSlot[i] = descriptorHeap->AllocateUAV(m_impl->m_cubemap.get(), &uavDesc);

    }
}

KS::Skydome::~Skydome() {}

void KS::Skydome::Bind(const Device&, DXCommandList& commandList, const ShaderInputDesc& desc, uint32_t mip)
{
    if (desc.modifications == ShaderInputMod::READ_ONLY)
    {
        commandList.TransitionResource(*m_impl->m_cubemap, D3D12_RESOURCE_STATE_COMMON);
        commandList.BindHeapResource(*m_impl->m_cubemap, m_impl->mSRVHeapSlot, desc.rootIndex);
    }
    else
    {

        commandList.TransitionResource(*m_impl->m_cubemap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList.BindHeapResource(*m_impl->m_cubemap, m_impl->mUAVHeapSlot[mip], desc.rootIndex);
        m_ready = true;
    }
}

size_t KS::Skydome::GetGPUAddress(int, int) const
{
    return m_impl->m_cubemap->GetResource()->GetGPUVirtualAddress();
}

uint32_t KS::Skydome::GetHandleIndex(bool readOnly) const 
{
    if (readOnly)
    {
        return m_impl->mSRVHeapSlot.GetIndex();
    }
    else
    {
        return m_impl->mUAVHeapSlot[0].GetIndex();
    }
}
