#include <resources/Texture.hpp>
#include <device/Device.hpp>
#include <resources/Image.hpp>
#include <renderer/RenderTarget.hpp>
#include <renderer/DepthStencil.hpp>
#include "Helpers/DXResource.hpp"
#include "Helpers/DXHeapHandle.hpp"
#include "Helpers/DXDescHeap.hpp"
#include "Helpers/DXCommandList.hpp"
#include "Helpers/DX12Conversion.hpp"

class KS::Texture::Impl
{
public:
    std::unique_ptr<DXResource> mTextureBuffer {};
    DXHeapHandle mSRVHeapSlot {};
    DXHeapHandle mUAVHeapSlot {};

    void AllocateAsUAV(DXDescHeap* descriptorHeap);
    void AllocateAsSRV(DXDescHeap* descriptorHeap);
};

KS::Texture::Texture(const Device& device, const Image& image, TextureFlags type)
{
    m_impl = new Impl();
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    m_width = image.GetWidth();
    m_height = image.GetHeight();
    m_format = R8G8B8A8_UNORM;
    m_flag = type;
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM,
        m_width,
        m_height,
        1,
        1,
        1,
        0,
        type == RW_TEXTURE ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                           : D3D12_RESOURCE_FLAG_NONE);

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

KS::Texture::Texture(const Device& device, uint32_t width, uint32_t height, TextureFlags type, glm::vec4 clearColor, Formats format)
{
    m_impl = new Impl();

    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    m_width = width;
    m_height = height;
    m_format = format;
    m_flag = type;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = KSFormatsToDXGI(format);
    clearValue.Color[0] = clearColor.x; // Red component
    clearValue.Color[1] = clearColor.y; // Green component
    clearValue.Color[2] = clearColor.z; // Blue component
    clearValue.Color[3] = clearColor.w; // Alpha component

    auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(KS::KSFormatsToDXGI(m_format),
        m_width,
        m_height,
        1,
        1,
        1,
        0,
        type == RW_TEXTURE          ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
            : type == RENDER_TARGET ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
            : type == DEPTH_TEXTURE ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
                                    : D3D12_RESOURCE_FLAG_NONE);

    CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    m_impl->mTextureBuffer = std::make_unique<DXResource>(engineDevice, heapProperties, resourceDesc, &clearValue, "Texture Buffer Resource Heap");
}

KS::Texture::Texture(const Device& device, void* resource, glm::vec2 size, TextureFlags type)
{
    m_impl = new Impl();
    m_impl->mTextureBuffer = std::make_unique<DXResource>();
    m_impl->mTextureBuffer->SetResource(reinterpret_cast<ID3D12Resource*>(resource));
    m_format = DXGIFormatsToKS(m_impl->mTextureBuffer->GetDesc().Format);
    m_width = size.x;
    m_height = size.y;
    m_flag = type;
}

KS::Texture::~Texture()
{
    delete m_impl;
}

void KS::Texture::Bind(const Device& device, uint32_t rootIndex, bool readOnly) const
{
    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());

    if (readOnly)
    {
        if (!m_impl->mSRVHeapSlot.IsValid())
        {
            m_impl->AllocateAsSRV(resourceHeap);
        }
        commandList->ResourceBarrier(m_impl->mTextureBuffer->Get(), m_impl->mTextureBuffer->GetState(), D3D12_RESOURCE_STATE_COMMON);
        m_impl->mTextureBuffer->ChangeState(D3D12_RESOURCE_STATE_COMMON);
        commandList->BindHeapResource(m_impl->mTextureBuffer, m_impl->mSRVHeapSlot, rootIndex);
    }
    else
    {
        if (m_flag == RENDER_TARGET || m_flag == DEPTH_TEXTURE)
        {
            LOG(Log::Severity::WARN, "Cannot bind textures created with the RENDER_TARGET or DEPTH_TEXTURE types as read-write textures. Command ignored.");
            return;
        }

        if (!m_impl->mUAVHeapSlot.IsValid())
        {
            m_impl->AllocateAsUAV(resourceHeap);
        }

        commandList->ResourceBarrier(m_impl->mTextureBuffer->Get(), m_impl->mTextureBuffer->GetState(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        m_impl->mTextureBuffer->ChangeState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->BindHeapResource(m_impl->mTextureBuffer, m_impl->mUAVHeapSlot, rootIndex);
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

class KS::RenderTarget::Impl
{
public:
    DXHeapHandle m_RT[8];
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

void KS::RenderTarget::AddTexture(Device& device, std::shared_ptr<Texture> texture, std::string name)
{
    if (m_textureCount >= 8)
    {
        LOG(Log::Severity::WARN, "Tried to attach more than 8 textures to a render target. This will be ignored.");
        return;
    }

    if (texture->GetType() != Texture::RENDER_TARGET)
    {
        LOG(Log::Severity::WARN, "Tried to attach a texture that was not created with the render target type. Command will be ignored.");
        return;
    }

    m_textures[m_textureCount] = texture;

    auto renderTargetHeap = reinterpret_cast<DXDescHeap*>(device.GetRenderTargetHeap());
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = KSFormatsToDXGI(texture->GetFormat());
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    m_impl->m_RT[m_textureCount] = renderTargetHeap->AllocateRenderTarget(texture->m_impl->mTextureBuffer.get(), &rtvDesc);
    m_textureCount++;

    m_impl->m_viewport.Width = texture->m_width;
    m_impl->m_viewport.Height = texture->m_height;
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
    texture->m_impl->mTextureBuffer->Get()->SetName(wString);
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
        m_resources[i] = m_textures[i]->m_impl->mTextureBuffer.get();
        commandList->ResourceBarrier(m_resources[i]->Get(), m_resources[i]->GetState(), D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_resources[i]->ChangeState(D3D12_RESOURCE_STATE_RENDER_TARGET);
    }

    commandList->ResourceBarrier(depth->m_texture->m_impl->mTextureBuffer->Get(), depth->m_texture->m_impl->mTextureBuffer->GetState(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
    depth->m_texture->m_impl->mTextureBuffer->ChangeState(D3D12_RESOURCE_STATE_DEPTH_WRITE);

    commandList->BindRenderTargets(&m_resources[0], &m_impl->m_RT[0], depth->m_texture->m_impl->mTextureBuffer, depth->m_impl->mDepthHandle, m_textureCount);

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
        commandList->ClearRenderTargets(m_textures[i]->m_impl->mTextureBuffer, m_impl->m_RT[i], &m_textures[i]->m_clearColor[0]);
    }
}

void KS::RenderTarget::PrepareToPresent(Device& device)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    for (int i = 0; i < m_textureCount; i++)
    {
        commandList->ResourceBarrier(m_textures[i]->m_impl->mTextureBuffer->Get(), m_textures[i]->m_impl->mTextureBuffer->GetState(), D3D12_RESOURCE_STATE_PRESENT);
        m_textures[i]->m_impl->mTextureBuffer->ChangeState(D3D12_RESOURCE_STATE_PRESENT);
    }
}

KS::DepthStencil::DepthStencil(Device& device, std::shared_ptr<Texture> texture)
{
    m_impl = std::make_unique<Impl>();

    if (texture->GetType() != Texture::DEPTH_TEXTURE)
    {
        LOG(Log::Severity::WARN, "Tried to attach a texture that was not created with the depth stencil type. Depth stencil will not be initialized.");
        return;
    }

    m_texture = texture;

    auto depthHeap = reinterpret_cast<DXDescHeap*>(device.GetDepthHeap());
    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
    depthStencilDesc.Format = KSFormatsToDXGI(texture->GetFormat());
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
