#include <resources/Texture.hpp>
#include <device/Device.hpp>
#include <resources/Image.hpp>
#include "Helpers/DXResource.hpp"
#include "Helpers/DXHeapHandle.hpp"
#include "Helpers/DXDescHeap.hpp"

class KS::Texture::Impl
{
public:
    std::unique_ptr<DXResource> mTextureBuffer {};
    DXHeapHandle mSRVHeapSlot {};
    DXHeapHandle mUAVHeapSlot {};

    void AllocateAsUAV(DXDescHeap* descriptorHeap);
    void AllocateAsSRV(DXDescHeap* descriptorHeap);
};

KS::Texture::Texture(const Device& device, const Image& image, bool readOnly)
{
    m_impl = new Impl();
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    auto commandList = reinterpret_cast<ID3D12GraphicsCommandList4*>(device.GetCommandList());
    m_width = image.GetWidth();
    m_height = image.GetHeight();

    auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM,
        m_width,
        m_height,
        1,
        1,
        1,
        0,
        readOnly ? D3D12_RESOURCE_FLAG_NONE : D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    m_impl->mTextureBuffer = std::make_unique<DXResource>(engineDevice, heapProperties, resourceDesc, nullptr, "Texture Buffer Resource Heap");

    UINT64 textureUploadBufferSize;
    engineDevice->GetCopyableFootprints(&resourceDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

    const int bitsPerPixel = 32;
    int bytesPerRow = (m_width * bitsPerPixel) / 8;

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = image.GetData().GetView<uint8_t>().begin();
    textureData.RowPitch = bytesPerRow;
    textureData.SlicePitch = bytesPerRow * m_height;
    m_impl->mTextureBuffer->CreateUploadBuffer(engineDevice, static_cast<int>(textureUploadBufferSize), 0);
    m_impl->mTextureBuffer->Update(commandList, textureData, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 1);
}

KS::Texture::Texture(const Device& device, uint32_t width, uint32_t height, bool readOnly)
{
    m_impl = new Impl();

    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    m_width = width;
    m_height = height;

    auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM,
        m_width,
        m_height,
        1,
        1,
        1,
        0,
        readOnly ? D3D12_RESOURCE_FLAG_NONE : D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    m_impl->mTextureBuffer = std::make_unique<DXResource>(engineDevice, heapProperties, resourceDesc, nullptr, "Texture Buffer Resource Heap");
}

KS::Texture::~Texture()
{
    delete m_impl;
}

void KS::Texture::BindToGraphics(const Device& device, uint32_t rootIndex, bool readOnly) const
{
    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());
    auto commandList = reinterpret_cast<ID3D12GraphicsCommandList4*>(device.GetCommandList());

    if (readOnly)
    {
        if (m_impl->mSRVHeapSlot.IsValid())
        {
            resourceHeap->BindToGraphics(commandList, rootIndex, m_impl->mSRVHeapSlot);
        }
        else
            m_impl->AllocateAsSRV(resourceHeap);
    }
    else
    {
        if (m_impl->mUAVHeapSlot.IsValid())
        {
            resourceHeap->BindToGraphics(commandList, rootIndex, m_impl->mUAVHeapSlot);
        }
        else
            m_impl->AllocateAsUAV(resourceHeap);
    }
}
void KS::Texture::BindToCompute(const Device& device, uint32_t rootIndex, bool readOnly) const
{
    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());
    auto commandList = reinterpret_cast<ID3D12GraphicsCommandList4*>(device.GetCommandList());

    if (readOnly)
    {
        if (m_impl->mSRVHeapSlot.IsValid())
        {
            resourceHeap->BindToCompute(commandList, rootIndex, m_impl->mSRVHeapSlot);
        }
        else
            m_impl->AllocateAsSRV(resourceHeap);
    }
    else
    {
        if (m_impl->mUAVHeapSlot.IsValid())
        {
            resourceHeap->BindToCompute(commandList, rootIndex, m_impl->mUAVHeapSlot);
        }
        else
            m_impl->AllocateAsUAV(resourceHeap);
    }
}

void KS::Texture::Impl::AllocateAsUAV(DXDescHeap* descriptorHeap)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uavDesc.Texture2D.MipSlice = 0;
    uavDesc.Texture2D.PlaneSlice = 0;
    mUAVHeapSlot = descriptorHeap->AllocateUAV(mTextureBuffer.get(), &uavDesc);
}

void KS::Texture::Impl::AllocateAsSRV(DXDescHeap* descriptorHeap)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    mSRVHeapSlot = descriptorHeap->AllocateResource(mTextureBuffer.get(), &srvDesc);
}
