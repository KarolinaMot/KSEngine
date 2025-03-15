#include <resources/Texture.hpp>
#include <device/Device.hpp>
#include <resources/Image.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputCollection.hpp>
#include <renderer/RenderTarget.hpp>
#include <renderer/DepthStencil.hpp>
#include <renderer/UniformBuffer.hpp>
#include "Helpers/DXResource.hpp"
#include "Helpers/DXHeapHandle.hpp"
#include "Helpers/DXDescHeap.hpp"
#include "Helpers/DXCommandList.hpp"
#include "Helpers/DX12Conversion.hpp"

class KS::Texture::Impl
{
public:
    std::unique_ptr<DXResource> mTextureBuffer {};
    std::unique_ptr<UniformBuffer> mMipmapUB {};
    DXHeapHandle mSRVHeapSlot {};
    DXHeapHandle mUAVHeapSlot {};
    DXHeapHandle mUAVMipslots[3]{};

    void AllocateAsUAV(DXDescHeap* descriptorHeap);
    void AllocateAsUAV(DXDescHeap* descriptorHeap, int slot);
    void AllocateAsSRV(DXDescHeap* descriptorHeap);
    void AllocateAsSRV(DXDescHeap* descriptorHeap, int slot);
};

KS::Texture::Texture(Device& device, const Image& image, int type)
{
    m_impl = new Impl();
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    m_width = image.GetWidth();
    m_height = image.GetHeight();
    m_format = R8G8B8A8_UNORM;
    m_flag = type;
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if (m_flag & TextureFlags::DEPTH_TEXTURE)
        flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    if (m_flag & TextureFlags::RENDER_TARGET)
        flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    auto resourceDesc
        = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM,
            m_width,
            m_height,
            1,
            m_width <= 5 ? 1 : 4,
            1,
            0,
            flags);

    CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    m_impl->mTextureBuffer = std::make_unique<DXResource>(engineDevice, heapProperties, resourceDesc, nullptr, "Texture Buffer Resource Heap");
    m_mipLevels = resourceDesc.MipLevels;

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
    auto descriptorHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());
    m_impl->AllocateAsSRV(descriptorHeap);

    for (int i = 1; i < resourceDesc.MipLevels; i++)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Format = resourceDesc.Format;
        uavDesc.Texture2D.MipSlice = i;
        uavDesc.Texture2D.PlaneSlice = 0;

        m_impl->mUAVMipslots[i - 1] = descriptorHeap->AllocateUAV(m_impl->mTextureBuffer.get(), &uavDesc);
    }

    GenerateMipmaps(device);
}

KS::Texture::Texture(const Device& device, uint32_t width, uint32_t height, int type, glm::vec4 clearColor, Formats format)
{
    m_impl = new Impl();

    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    m_width = width;
    m_height = height;
    m_format = format;
    m_flag = type;
    m_clearColor = clearColor;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = Conversion::KSFormatsToDXGI(format);
    clearValue.Color[0] = clearColor.x; // Red component
    clearValue.Color[1] = clearColor.y; // Green component
    clearValue.Color[2] = clearColor.z; // Blue component
    clearValue.Color[3] = clearColor.w; // Alpha component
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if (m_flag & TextureFlags::DEPTH_TEXTURE)
        flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    if (m_flag & TextureFlags::RENDER_TARGET)
        flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if (m_flag & TextureFlags::RW_TEXTURE)
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(KS::Conversion::KSFormatsToDXGI(m_format),
        m_width,
        m_height,
        1,
        1,
        1,
        0,
        flags);

    CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    m_impl->mTextureBuffer = std::make_unique<DXResource>(engineDevice, heapProperties, resourceDesc, &clearValue, "Texture Buffer Resource Heap");
}

KS::Texture::Texture(const Device& device, void* resource, glm::vec2 size, int type)
{
    m_impl = new Impl();
    m_impl->mTextureBuffer = std::make_unique<DXResource>();
    m_impl->mTextureBuffer->SetResource(reinterpret_cast<ID3D12Resource*>(resource));
    m_format = Conversion::DXGIFormatsToKS(m_impl->mTextureBuffer->GetDesc().Format);
    m_width = size.x;
    m_height = size.y;
    m_flag = type;
}

KS::Texture::Texture(const Device& device, uint32_t width, uint32_t height, int flags, glm::vec4 clearColor, Formats format,
                     int srvAllocationSlot, int uavAllocationSlot)
    : Texture(device, width, height, flags, clearColor, format)
{
    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());
    m_impl->AllocateAsSRV(resourceHeap, srvAllocationSlot);
    m_impl->AllocateAsUAV(resourceHeap, uavAllocationSlot);
}

KS::Texture::~Texture()
{
    delete m_impl;
}

void KS::Texture::Bind(Device& device, const ShaderInputDesc& desc, uint32_t offsetIndex)
{
    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());

    if (desc.modifications == ShaderInputMod::READ_ONLY)
    {
        TransitionToRO(device);
        m_impl->mTextureBuffer->ChangeState(D3D12_RESOURCE_STATE_COMMON);
        commandList->BindHeapResource(m_impl->mTextureBuffer, m_impl->mSRVHeapSlot, desc.rootIndex);
    }
    else
    {
        TransitionToRW(device);
        m_impl->mTextureBuffer->ChangeState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->BindHeapResource(m_impl->mTextureBuffer, m_impl->mUAVHeapSlot, desc.rootIndex);
    }
}

void KS::Texture::TransitionToRO(const Device& device) const
{
    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());

    if (!m_impl->mSRVHeapSlot.IsValid())
    {
        m_impl->AllocateAsSRV(resourceHeap);
    }

    commandList->ResourceBarrier(*m_impl->mTextureBuffer->Get(), m_impl->mTextureBuffer->GetState(),
                                 D3D12_RESOURCE_STATE_COMMON);
}

void KS::Texture::TransitionToRW(const Device& device) const
{
    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());

    if (!m_impl->mUAVHeapSlot.IsValid())
    {
        m_impl->AllocateAsUAV(resourceHeap);
    }

    commandList->ResourceBarrier(*m_impl->mTextureBuffer->Get(), m_impl->mTextureBuffer->GetState(),
                                 D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void KS::Texture::GenerateMipmaps(Device& device)
{
    // FROM: https://www.3dgep.com/learning-directx-12-4/#Generate_Mipmaps_Compute_Shader
    // TODO: Do the required steps if resource does not support UAVs

    ID3D12Device5* engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    DXCommandList* commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());

    GenerateMipsInfo generateMipsCB;
    generateMipsCB.IsSRGB = false;  // TODO: check if format is SRGB

    auto resource = m_impl->mTextureBuffer->GetResource();
    auto resourceDesc = resource->GetDesc();

    if (resourceDesc.MipLevels < 4) return;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = resourceDesc.Format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension =
        D3D12_SRV_DIMENSION_TEXTURE2D;  // Only 2D textures are supported (this was checked in the calling function).
    srvDesc.Texture2D.MipLevels = resourceDesc.MipLevels;

    uint64_t srcWidth = resourceDesc.Width;
    uint32_t srcHeight = resourceDesc.Height;
    uint32_t dstWidth = static_cast<uint32_t>(srcWidth >> 1);
    uint32_t dstHeight = srcHeight >> 1;

    // 0b00(0): Both width and height are even.
    // 0b01(1): Width is odd, height is even.
    // 0b10(2): Width is even, height is odd.
    // 0b11(3): Both width and height are odd.
    generateMipsCB.SrcDimension = (srcHeight & 1) << 1 | (srcWidth & 1);
    DWORD mipCount = 4;

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

    CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    m_impl->mMipmapUB = std::make_unique<UniformBuffer>(device, "Texture mipmap buffer", generateMipsCB, 1);

    ID3D12PipelineState* pipeline = reinterpret_cast<ID3D12PipelineState*>(device.GetMipGenShader()->GetPipeline());
    commandList->BindPipeline(pipeline);

    auto rootSignature = device.GetMipGenShaderInputs();
    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(rootSignature->GetSignature()), true);

    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());
    commandList->BindDescriptorHeaps(resourceHeap, nullptr, nullptr);

    m_impl->mMipmapUB->Bind(device, rootSignature->GetInput("mipmap_info"));
    commandList->BindHeapResource(m_impl->mTextureBuffer, m_impl->mUAVMipslots[0], rootSignature->GetInput("mip_1").rootIndex);
    commandList->BindHeapResource(m_impl->mTextureBuffer, m_impl->mUAVMipslots[1], rootSignature->GetInput("mip_2").rootIndex);
    commandList->BindHeapResource(m_impl->mTextureBuffer, m_impl->mUAVMipslots[2], rootSignature->GetInput("mip_3").rootIndex);
    commandList->BindHeapResource(m_impl->mTextureBuffer, m_impl->mSRVHeapSlot, rootSignature->GetInput("mip_0").rootIndex);

    commandList->DispatchShader(dstWidth / 8, dstHeight / 8, 1);
}

void KS::Texture::Impl::AllocateAsUAV(DXDescHeap* descriptorHeap)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Format = mTextureBuffer->GetDesc().Format;
    uavDesc.Texture2D.MipSlice = 0;
    uavDesc.Texture2D.PlaneSlice = 0;
    mUAVHeapSlot = descriptorHeap->AllocateUAV(mTextureBuffer.get(), &uavDesc);
}

void KS::Texture::Impl::AllocateAsUAV(DXDescHeap* descriptorHeap, int slot)
{
    if (slot < 0)
    {
        return;
    }

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Format = mTextureBuffer->GetDesc().Format;
    uavDesc.Texture2D.MipSlice = 0;
    uavDesc.Texture2D.PlaneSlice = 0;
    mUAVHeapSlot = descriptorHeap->AllocateUAV(mTextureBuffer.get(), &uavDesc, slot);
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

    m_impl->m_viewport.Width = texture1->m_width;
    m_impl->m_viewport.Height = texture1->m_height;
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

    m_impl->m_viewport.Width = texture1->m_width;
    m_impl->m_viewport.Height = texture1->m_height;
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
void KS::RenderTarget::Bind(Device& device, const DepthStencil* depth) const
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

    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    DXResource* m_resources[8];
    for (int i = 0; i < m_textureCount; i++)
    {
        m_resources[i] = m_textures[device.GetCPUFrameIndex()][i]->m_impl->mTextureBuffer.get();
        commandList->ResourceBarrier(*m_resources[i]->Get(), m_resources[i]->GetState(), D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_resources[i]->ChangeState(D3D12_RESOURCE_STATE_RENDER_TARGET);
    }

    commandList->ResourceBarrier(*depth->m_texture->m_impl->mTextureBuffer->Get(), depth->m_texture->m_impl->mTextureBuffer->GetState(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
    depth->m_texture->m_impl->mTextureBuffer->ChangeState(D3D12_RESOURCE_STATE_DEPTH_WRITE);

    commandList->BindRenderTargets(&m_resources[0], &m_impl->m_RT[device.GetCPUFrameIndex()][0], depth->m_texture->m_impl->mTextureBuffer, depth->m_impl->mDepthHandle, m_textureCount);

    commandList->GetCommandList()->RSSetViewports(1, &m_impl->m_viewport);
    commandList->GetCommandList()->RSSetScissorRects(1, &m_impl->m_scissor_rect);
}

void KS::RenderTarget::Clear(const Device& device)
{
    if (m_textureCount <= 0)
    {
        LOG(Log::Severity::WARN, "Trying to clear a render target with no textures attached. Command ignored.");
        return;
    }

    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    for (int i = 0; i < m_textureCount; i++)
    {
        glm::vec4 clearColor = m_textures[device.GetCPUFrameIndex()][i]->m_clearColor;
        commandList->ClearRenderTargets(m_textures[device.GetCPUFrameIndex()][i]->m_impl->mTextureBuffer, m_impl->m_RT[device.GetCPUFrameIndex()][i], &clearColor[0]);
    }
}

void KS::RenderTarget::CopyTo(Device& device, std::shared_ptr<RenderTarget> sourceRT, int sourceRtIndex, int dstRTIndex)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());

    commandList->ResourceBarrier(*m_textures[device.GetCPUFrameIndex()][dstRTIndex]->m_impl->mTextureBuffer->Get(),
        m_textures[device.GetCPUFrameIndex()][dstRTIndex]->m_impl->mTextureBuffer->GetState(),
        D3D12_RESOURCE_STATE_COPY_DEST);
    m_textures[device.GetCPUFrameIndex()][dstRTIndex]->m_impl->mTextureBuffer->ChangeState(D3D12_RESOURCE_STATE_COPY_DEST);

    sourceRT->SetCopyFrom(device, sourceRtIndex);

    commandList->CopyResource(sourceRT->GetTexture(device, sourceRtIndex)->m_impl->mTextureBuffer,
        m_textures[device.GetCPUFrameIndex()][dstRTIndex]->m_impl->mTextureBuffer);
}

void KS::RenderTarget::SetCopyFrom(const Device& device, int rtIndex)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());

    commandList->ResourceBarrier(*m_textures[device.GetCPUFrameIndex()][rtIndex]->m_impl->mTextureBuffer->Get(),
        m_textures[device.GetCPUFrameIndex()][rtIndex]->m_impl->mTextureBuffer->GetState(),
        D3D12_RESOURCE_STATE_COPY_SOURCE);
    m_textures[device.GetCPUFrameIndex()][rtIndex]->m_impl->mTextureBuffer->ChangeState(D3D12_RESOURCE_STATE_COPY_SOURCE);
}

void KS::RenderTarget::PrepareToPresent(Device& device)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    for (int i = 0; i < m_textureCount; i++)
    {
        commandList->ResourceBarrier(*m_textures[device.GetCPUFrameIndex()][i]->m_impl->mTextureBuffer->Get(), m_textures[device.GetCPUFrameIndex()][i]->m_impl->mTextureBuffer->GetState(), D3D12_RESOURCE_STATE_PRESENT);
        m_textures[device.GetCPUFrameIndex()][i]->m_impl->mTextureBuffer->ChangeState(D3D12_RESOURCE_STATE_PRESENT);
    }
}

std::shared_ptr<KS::Texture> KS::RenderTarget::GetTexture(Device& device, int index)
{
    return m_textures[device.GetCPUFrameIndex()][index];
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

void KS::DepthStencil::Clear(Device& device)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    commandList->ClearDepthStencils(m_texture->m_impl->mTextureBuffer, m_impl->mDepthHandle);
}
