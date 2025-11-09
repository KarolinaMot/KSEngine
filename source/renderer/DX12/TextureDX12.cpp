#include <resources/Texture.hpp>
#include <device/Device.hpp>
#include <resources/Image.hpp>
#include <renderer/RenderTarget.hpp>
#include <renderer/ShaderInputBlueprint.hpp>
#include <renderer/DepthStencil.hpp>
#include <renderer/UploadArena.h>
#include <renderer/UniformBuffer.hpp>
#include <renderer/Shader.hpp>
#include "Helpers/DXResource.hpp"
#include "Helpers/DXHeapHandle.hpp"
#include "Helpers/DXDescHeap.hpp"
#include "Helpers/DXCommandList.hpp"
#include "Helpers/DX12Conversion.hpp"

class KS::Texture::Impl
{
public:
    std::unique_ptr<DXResource> mTextureBuffer{};
    std::unique_ptr<UniformBuffer> mMipmapUB{};
    DXHeapHandle mSRVHeapSlot{};
    DXHeapHandle mUAVHeapSlot[4]{};
    UploadSlice m_slice;

    void AllocateAsUAV(DXDescHeap* descriptorHeap, int mipSlice);
    void AllocateAsUAV(DXDescHeap* descriptorHeap, int slot, int mipSlice);
    void AllocateAsSRV(DXDescHeap* descriptorHeap);
    void AllocateAsSRV(DXDescHeap* descriptorHeap, int slot);
};

KS::Texture::Texture(const Device& device, uint32_t width, uint32_t height, int type, glm::vec4 clearColor, Formats format,
                     uint32_t mipLevels)
{
    m_impl = new Impl();

    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    m_width = width;
    m_height = height;
    m_format = format;
    m_flag = type;
    m_clearColor = clearColor;
    m_mipLevels = m_width <= 5 ? 1 : mipLevels;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = Conversion::KSFormatsToDXGI(format);
    clearValue.Color[0] = clearColor.x;  // Red component
    clearValue.Color[1] = clearColor.y;  // Green component
    clearValue.Color[2] = clearColor.z;  // Blue component
    clearValue.Color[3] = clearColor.w;  // Alpha component

    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if (m_flag & TextureFlags::DEPTH_TEXTURE) flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    if (m_flag & TextureFlags::RENDER_TARGET) flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if (m_flag & TextureFlags::RW_TEXTURE) flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    auto resourceDesc =
        CD3DX12_RESOURCE_DESC::Tex2D(KS::Conversion::KSFormatsToDXGI(m_format), m_width, m_height, 1, static_cast<UINT16>(m_mipLevels), 1, 0, flags);

    CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    m_impl->mTextureBuffer =
        std::make_unique<DXResource>(engineDevice, heapProperties, resourceDesc,
        (flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) || (flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) ? &clearValue
                                                                                                               : nullptr,
                                                          "Texture Buffer Resource Heap");
}

KS::Texture::Texture(Device& device, void* resourceHeap, DXCommandList& commandList, const Image& image) :
    Texture(device, image.GetWidth(), image.GetHeight(), RW_TEXTURE, glm::vec4(0.f), R8G8B8A8_UNORM, 4)
{
    UINT64 textureUploadBufferSize;
    auto resourceDesc = m_impl->mTextureBuffer->GetDesc();
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());

    engineDevice->GetCopyableFootprints(&resourceDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

    auto upl = device.GetUploadArena();

    UINT64 requiredSize = 0;
    engineDevice->GetCopyableFootprints(&resourceDesc, 0, 1, 0, nullptr, nullptr, nullptr, &requiredSize);
    constexpr UINT64 kPlacementAlign = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;  // 512
    m_impl->m_slice = upl->Allocate(device, commandList, requiredSize, kPlacementAlign);

    commandList.TransitionResource(*m_impl->mTextureBuffer, D3D12_RESOURCE_STATE_COPY_DEST);

    auto uploadSource = reinterpret_cast<DXResource*>(upl->GetPageResource(m_impl->m_slice.m_pageID));

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = image.GetData().GetView<uint8_t>().begin();
    textureData.RowPitch = static_cast<LONG_PTR>(m_width * 4);
    textureData.SlicePitch = static_cast<LONG_PTR>(m_width * 4 * m_height);

    UpdateSubresources(commandList.GetCommandList().Get(), m_impl->mTextureBuffer->GetResource().Get(),
                       uploadSource->GetResource().Get(), static_cast<UINT64>(m_impl->m_slice.m_head), 0, 1, &textureData);

    commandList.TransitionResource(*m_impl->mTextureBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    auto descriptorHeap = reinterpret_cast<DXDescHeap*>(resourceHeap);

    m_impl->AllocateAsSRV(descriptorHeap);

    if (m_flag & RW_TEXTURE)
    {
        m_impl->AllocateAsUAV(descriptorHeap, 0);
    }

    for (int i = 1; i < resourceDesc.MipLevels; i++)
    {
        m_impl->AllocateAsUAV(descriptorHeap, i);
    }
}

KS::Texture::Texture(const Device& device, void* resourceHeap, uint32_t width, uint32_t height, int type, glm::vec4 clearColor,
                     Formats format, uint32_t mipLevels)
    : Texture(device, width, height, type, clearColor, format, mipLevels)
{
    auto descriptorHeap = reinterpret_cast<DXDescHeap*>(resourceHeap);
    auto resourceDesc = m_impl->mTextureBuffer->GetDesc();

    if (m_flag & RW_TEXTURE)
    {
        m_impl->AllocateAsUAV(descriptorHeap, 0);
    }

    if (m_mipLevels > 1)
    {
        m_impl->AllocateAsSRV(descriptorHeap);

        for (int i = 1; i < resourceDesc.MipLevels; i++)
        {
            m_impl->AllocateAsUAV(descriptorHeap, i);
        }
    }
}

uint32_t KS::Texture::GetHandleIndex(bool readOnly) const
{
    if (readOnly)
    {
        if (!m_impl->mSRVHeapSlot.IsValid())
        {
            LOG(Log::Severity::WARN, "Tried to get SRV index of texture with no allocated SRV.");
            assert(false);
            return 0;
        }

        return m_impl->mSRVHeapSlot.GetIndex();
    }
    else
    {
        if (!m_impl->mUAVHeapSlot[0].IsValid())
        {
            LOG(Log::Severity::FATAL, "Tried to get UAV index of texture with no allocated UAV.");
            assert(false);
            return 0;
        }

        return m_impl->mUAVHeapSlot[0].GetIndex();
    }
}

KS::Texture::Texture(void* resource, uint32_t width, uint32_t height, int type)
{
    m_impl = new Impl();
    m_impl->mTextureBuffer = std::make_unique<DXResource>();
    m_impl->mTextureBuffer->SetResource(reinterpret_cast<ID3D12Resource*>(resource));
    m_format = Conversion::DXGIFormatsToKS(m_impl->mTextureBuffer->GetDesc().Format);
    m_width = width;
    m_height = height;
    m_flag = type;
}

KS::Texture::Texture(const Device& device, void* resourceHeap, uint32_t width, uint32_t height, int type, glm::vec4 clearColor,
                     Formats format, int srvAllocationSlot, int uavAllocationSlot)
{
    m_impl = new Impl();

    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    m_width = width;
    m_height = height;
    m_format = format;
    m_flag = type;
    m_clearColor = clearColor;
    m_mipLevels = 1;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = Conversion::KSFormatsToDXGI(format);
    clearValue.Color[0] = clearColor.x;  // Red component
    clearValue.Color[1] = clearColor.y;  // Green component
    clearValue.Color[2] = clearColor.z;  // Blue component
    clearValue.Color[3] = clearColor.w;  // Alpha component
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if (m_flag & TextureFlags::DEPTH_TEXTURE) flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    if (m_flag & TextureFlags::RENDER_TARGET) flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if (m_flag & TextureFlags::RW_TEXTURE) flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    auto resourceDesc =
        CD3DX12_RESOURCE_DESC::Tex2D(KS::Conversion::KSFormatsToDXGI(m_format), m_width, m_height, 1, static_cast<UINT16>(m_mipLevels), 1, 0, flags);

    CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    m_impl->mTextureBuffer =
        std::make_unique<DXResource>(engineDevice, heapProperties, resourceDesc, &clearValue, "Texture Buffer Resource Heap");

    auto heap = reinterpret_cast<DXDescHeap*>(resourceHeap);
    m_impl->AllocateAsSRV(heap, srvAllocationSlot);
    m_impl->AllocateAsUAV(heap, uavAllocationSlot, 0);
}

KS::Texture::~Texture() { delete m_impl; }

void KS::Texture::Bind(const Device&, void* resourceHeap, DXCommandList& commandList, const ShaderInputDesc& desc,
                       uint32_t mip)
{
    auto heap = reinterpret_cast<DXDescHeap*>(resourceHeap);

    if (desc.modifications == ShaderInputMod::READ_ONLY)
    {
        if (!m_impl->mSRVHeapSlot.IsValid()) 
            m_impl->AllocateAsSRV(heap);

        TransitionToRO(resourceHeap, commandList);
        commandList.BindHeapResource(*m_impl->mTextureBuffer, m_impl->mSRVHeapSlot, desc.rootIndex);
    }
    else
    {
        if (!(m_flag & RW_TEXTURE) && mip == 0)
        {
            LOG(Log::Severity::WARN,
                "Tried to bind a texture as read write, when it was not created witht he read-write flag. Command ignored.");
            return;
        }
            
        TransitionToRW(resourceHeap, commandList);
        uint32_t mipLevel = mip;
        if (mip > 0 && m_mipLevels == 0)
        {
            LOG(Log::Severity::WARN, "Tried to bind a mip level ({}) above the defined max mip level ({}) of a texture. Mip level 0 will be bound.", mip, m_mipLevels);
            mipLevel = 0;
        }

         if (!m_impl->mUAVHeapSlot[mipLevel].IsValid())
               m_impl->AllocateAsUAV(heap, mipLevel);

        commandList.BindHeapResource(*m_impl->mTextureBuffer, m_impl->mUAVHeapSlot[mipLevel], desc.rootIndex);
    }
}

void KS::Texture::TransitionToRO(void* resourceHeap, DXCommandList& commandList) const
{
    auto heap = reinterpret_cast<DXDescHeap*>(resourceHeap);
    if (!m_impl->mSRVHeapSlot.IsValid())
    {
        m_impl->AllocateAsSRV(heap);
    }

    commandList.TransitionResource(*m_impl->mTextureBuffer, D3D12_RESOURCE_STATE_COMMON);
}

void KS::Texture::TransitionToRW(void* resourceHeap, DXCommandList& commandList) const
{
    auto heap = reinterpret_cast<DXDescHeap*>(resourceHeap);

    if (!m_impl->mUAVHeapSlot[0].IsValid())
    {
        m_impl->AllocateAsUAV(heap, 0);
    }

    commandList.TransitionResource(*m_impl->mTextureBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

KS::GenerateMipsInfo KS::Texture::GetMipmapInfo() const
{
    auto resource = m_impl->mTextureBuffer->GetResource();
    auto resourceDesc = resource->GetDesc();
    DWORD mipCount = 4;

    GenerateMipsInfo generateMipsCB;
    generateMipsCB.IsSRGB = false;  // TODO: check if format is SRGB
    uint64_t srcWidth = resourceDesc.Width;
    uint32_t srcHeight = resourceDesc.Height;
    uint32_t dstWidth = static_cast<uint32_t>(srcWidth >> 1);
    uint32_t dstHeight = srcHeight >> 1;

    // 0b00(0): Both width and height are even.
    // 0b01(1): Width is odd, height is even.
    // 0b10(2): Width is even, height is odd.
    // 0b11(3): Both width and height are odd.
    generateMipsCB.SrcDimension = (srcHeight & 1) << 1 | (srcWidth & 1);

    // The number of times we can half the size of the texture and get
    // exactly a 50% reduction in size.
    // A 1 bit in the width or height indicates an odd dimension.
    // The case where either the width or the height is exactly 1 is handled
    // as a special case (as the dimension does not require reduction).
    _BitScanForward(&mipCount, (dstWidth == 1 ? dstHeight : dstWidth) | (dstHeight == 1 ? dstWidth : dstHeight));
    
    // Dimensions should not reduce to 0.
    // This can happen if the width and height are not the same.
    dstWidth = std::max<DWORD>(1, dstWidth);
    dstHeight = std::max<DWORD>(1, dstHeight);
    
    generateMipsCB.SrcMipLevel = 0;
    generateMipsCB.NumMipLevels = mipCount;
    generateMipsCB.TexelSize.x = 1.0f / (float)dstWidth;
    generateMipsCB.TexelSize.y = 1.0f / (float)dstHeight;

    return generateMipsCB;
}

size_t KS::Texture::GetGPUAddress(int, int) const { return m_impl->mTextureBuffer->GetResource()->GetGPUVirtualAddress(); }

void KS::Texture::Impl::AllocateAsUAV(DXDescHeap* descriptorHeap, int mipSlice)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Format = mTextureBuffer->GetDesc().Format;
    uavDesc.Texture2D.MipSlice = mipSlice;
    uavDesc.Texture2D.PlaneSlice = 0;
    mUAVHeapSlot[mipSlice] = descriptorHeap->AllocateUAV(mTextureBuffer.get(), &uavDesc);
}

void KS::Texture::Impl::AllocateAsUAV(DXDescHeap* descriptorHeap, int slot, int mipSlice)
{
    if (slot < 0)
    {
        AllocateAsUAV(descriptorHeap, mipSlice);
        return;
    }

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Format = mTextureBuffer->GetDesc().Format;
    uavDesc.Texture2D.MipSlice = mipSlice;
    uavDesc.Texture2D.PlaneSlice = 0;
    mUAVHeapSlot[mipSlice] = descriptorHeap->AllocateUAV(mTextureBuffer.get(), &uavDesc, slot);
}

void KS::Texture::Impl::AllocateAsSRV(DXDescHeap* descriptorHeap)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = mTextureBuffer->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = mTextureBuffer->GetDesc().MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    mSRVHeapSlot = descriptorHeap->AllocateResource(mTextureBuffer.get(), &srvDesc);
}

void KS::Texture::Impl::AllocateAsSRV(DXDescHeap* descriptorHeap, int slot)
{
    if (slot < 0)
    {
        AllocateAsSRV(descriptorHeap);
        return;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = mTextureBuffer->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    mSRVHeapSlot = descriptorHeap->AllocateResource(mTextureBuffer.get(), &srvDesc, slot);
}


class KS::RenderTarget::Impl
{
public:
    DXHeapHandle m_RT[2][8];
    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissor_rect;
};

class KS::DepthStencil::Impl
{
public:
    DXHeapHandle mDepthHandle;
};

KS::RenderTarget::RenderTarget()
{
    m_impl = std::make_unique<Impl>();
}

KS::RenderTarget::~RenderTarget()
{
}

void KS::RenderTarget::AddTexture(Device& device, std::shared_ptr<Texture> texture1, std::shared_ptr<Texture> texture2, std::string name)
{
    if (m_textureCount >= 8)
    {
        LOG(Log::Severity::WARN, "Tried to attach more than 8 textures to a render target. This will be ignored.");
        return;
    }

    if (!(texture1->GetType() & Texture::RENDER_TARGET) || !(texture2->GetType() & Texture::RENDER_TARGET))
    {
        LOG(Log::Severity::WARN, "Tried to attach a texture that was not created with the render target type. Command will be ignored.");
        return;
    }

    m_textures[0][m_textureCount] = texture1;
    m_textures[1][m_textureCount] = texture2;

    auto renderTargetHeap = reinterpret_cast<DXDescHeap*>(device.GetRenderTargetHeap());

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = Conversion::KSFormatsToDXGI(texture1->GetFormat());
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    m_impl->m_RT[0][m_textureCount] = renderTargetHeap->AllocateRenderTarget(texture1->m_impl->mTextureBuffer.get(), &rtvDesc);

    rtvDesc.Format = Conversion::KSFormatsToDXGI(texture2->GetFormat());
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    m_impl->m_RT[1][m_textureCount] = renderTargetHeap->AllocateRenderTarget(texture2->m_impl->mTextureBuffer.get(), &rtvDesc);

    m_textureCount++;

    m_impl->m_viewport.Width = static_cast<FLOAT>(texture1->m_width);
    m_impl->m_viewport.Height = static_cast<FLOAT>(texture1->m_height);
    m_impl->m_viewport.TopLeftX = 0.f;
    m_impl->m_viewport.TopLeftY = 0.f;
    m_impl->m_viewport.MinDepth = 0.0f;
    m_impl->m_viewport.MaxDepth = 1.0f;

    m_impl->m_scissor_rect.left = 0;
    m_impl->m_scissor_rect.top = 0;
    m_impl->m_scissor_rect.right = static_cast<LONG>(m_impl->m_viewport.Width);
    m_impl->m_scissor_rect.bottom = static_cast<LONG>(m_impl->m_viewport.Height);

    wchar_t wString[4096];
    MultiByteToWideChar(CP_ACP, 0, name.c_str(), -1, wString, 4096);
    texture1->m_impl->mTextureBuffer->Get()->SetName(wString);
    texture2->m_impl->mTextureBuffer->Get()->SetName(wString);
}

void KS::RenderTarget::AddTexture(Device& device, std::shared_ptr<Texture> texture1, std::shared_ptr<Texture> texture2, std::string name, unsigned int slot1, unsigned int slot2)
{
    if (m_textureCount >= 8)
    {
        LOG(Log::Severity::WARN, "Tried to attach more than 8 textures to a render target. This will be ignored.");
        return;
    }

    if (!(texture1->GetType() & Texture::RENDER_TARGET) || !(texture2->GetType() & Texture::RENDER_TARGET))
    {
        LOG(Log::Severity::WARN, "Tried to attach a texture that was not created with the render target type. Command will be ignored.");
        return;
    }

    m_textures[0][m_textureCount] = texture1;
    m_textures[1][m_textureCount] = texture2;

    auto renderTargetHeap = reinterpret_cast<DXDescHeap*>(device.GetRenderTargetHeap());

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = Conversion::KSFormatsToDXGI(texture1->GetFormat());
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    m_impl->m_RT[0][m_textureCount] = renderTargetHeap->AllocateRenderTarget(texture1->m_impl->mTextureBuffer.get(), &rtvDesc, slot1);

    rtvDesc.Format = Conversion::KSFormatsToDXGI(texture2->GetFormat());
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    m_impl->m_RT[1][m_textureCount] = renderTargetHeap->AllocateRenderTarget(texture2->m_impl->mTextureBuffer.get(), &rtvDesc, slot2);

    m_textureCount++;

    m_impl->m_viewport.Width = static_cast<FLOAT>(texture1->m_width);
    m_impl->m_viewport.Height = static_cast<FLOAT>(texture1->m_height);
    m_impl->m_viewport.TopLeftX = 0;
    m_impl->m_viewport.TopLeftY = 0;
    m_impl->m_viewport.MinDepth = 0.0f;
    m_impl->m_viewport.MaxDepth = 1.0f;

    m_impl->m_scissor_rect.left = 0;
    m_impl->m_scissor_rect.top = 0;
    m_impl->m_scissor_rect.right = static_cast<LONG>(m_impl->m_viewport.Width);
    m_impl->m_scissor_rect.bottom = static_cast<LONG>(m_impl->m_viewport.Height);

    wchar_t wString[4096];
    MultiByteToWideChar(CP_ACP, 0, name.c_str(), -1, wString, 4096);
    texture1->m_impl->mTextureBuffer->Get()->SetName(wString);
    texture2->m_impl->mTextureBuffer->Get()->SetName(wString);
}
void KS::RenderTarget::Bind(DXCommandList& commandList, uint32_t frameIndex, const DepthStencil* depth) const
{
    if (m_textureCount <= 0)
    {
        LOG(Log::Severity::WARN, "Trying to bind a render target with no textures attached. Command ignored.");
        return;
    }

    if (depth == nullptr && depth->IsValid())
    {
        LOG(Log::Severity::WARN, "Trying to bind a render target with no depth stencil. Command ignored.");
        return;
    }

    DXResource* m_resources[8];
    for (int i = 0; i < m_textureCount; i++)
    {
        m_resources[i] = m_textures[frameIndex][i]->m_impl->mTextureBuffer.get();
        commandList.TransitionResource(*m_resources[i], D3D12_RESOURCE_STATE_RENDER_TARGET);
    }

    commandList.TransitionResource(*depth->m_texture->m_impl->mTextureBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);

    commandList.BindRenderTargets(&m_resources[0], &m_impl->m_RT[frameIndex][0], depth->m_texture->m_impl->mTextureBuffer, depth->m_impl->mDepthHandle, m_textureCount);

    commandList.GetCommandList()->RSSetViewports(1, &m_impl->m_viewport);
    commandList.GetCommandList()->RSSetScissorRects(1, &m_impl->m_scissor_rect);
}

void KS::RenderTarget::Clear(DXCommandList& commandList, uint32_t frameIndex)
{
    if (m_textureCount <= 0)
    {
        LOG(Log::Severity::WARN, "Trying to clear a render target with no textures attached. Command ignored.");
        return;
    }

    for (int i = 0; i < m_textureCount; i++)
    {
        glm::vec4 clearColor = m_textures[frameIndex][i]->m_clearColor;
        commandList.ClearRenderTargets(*m_textures[frameIndex][i]->m_impl->mTextureBuffer, m_impl->m_RT[frameIndex][i], &clearColor[0]);
    }
}

void KS::RenderTarget::CopyTo(DXCommandList& commandList, uint32_t frameIndex, std::shared_ptr<RenderTarget> sourceRT,
                              int sourceRtIndex, int dstRTIndex)
{
    auto& dstTexBuffer = m_textures[frameIndex][dstRTIndex]->m_impl->mTextureBuffer;
    auto& srcTexBuffer = sourceRT->GetTexture(frameIndex, sourceRtIndex)->m_impl->mTextureBuffer;

    commandList.TransitionResource(*dstTexBuffer,  D3D12_RESOURCE_STATE_COPY_DEST);
    sourceRT->SetCopyFrom(commandList, frameIndex, sourceRtIndex);
    commandList.CopyResource(srcTexBuffer, dstTexBuffer);
}

void KS::RenderTarget::SetCopyFrom(DXCommandList& commandList, uint32_t frameIndex, int rtIndex)
{
    auto& texBuffer = m_textures[frameIndex][rtIndex]->m_impl->mTextureBuffer;
    commandList.TransitionResource(*texBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
}

void KS::RenderTarget::PrepareToPresent(DXCommandList& commandList, uint32_t frameIndex)
{
    for (int i = 0; i < m_textureCount; i++)
    {
        auto& texBuffer = m_textures[frameIndex][i]->m_impl->mTextureBuffer;

        commandList.TransitionResource(*texBuffer, D3D12_RESOURCE_STATE_PRESENT);
    }
}

std::shared_ptr<KS::Texture> KS::RenderTarget::GetTexture(uint32_t frameIndex, int index)
{
    return m_textures[frameIndex][index];
}

KS::DepthStencil::DepthStencil(Device& device, std::shared_ptr<Texture> texture)
{
    m_impl = std::make_unique<Impl>();

    if (!(texture->GetType() & Texture::DEPTH_TEXTURE))
    {
        LOG(Log::Severity::WARN, "Tried to attach a texture that was not created with the depth stencil type. Depth stencil will not be initialized.");
        return;
    }

    m_texture = texture;

    auto depthHeap = reinterpret_cast<DXDescHeap*>(device.GetDepthHeap());
    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
    depthStencilDesc.Format = Conversion::KSFormatsToDXGI(texture->GetFormat());
    depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;
    m_impl->mDepthHandle = depthHeap->AllocateDepthStencil(m_texture->m_impl->mTextureBuffer.get(), &depthStencilDesc);
}

KS::DepthStencil::~DepthStencil()
{
}

void KS::DepthStencil::Clear(DXCommandList& commandList)
{
    commandList.ClearDepthStencils(*m_texture->m_impl->mTextureBuffer, m_impl->mDepthHandle);
}
