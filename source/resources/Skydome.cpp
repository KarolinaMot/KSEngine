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

KS::Skydome::Skydome(Device& device, DXCommandList& commandList, const Texture& tex)
{
    m_impl = std::make_unique<Impl>();   
}

void KS::Skydome::Bind(const Device& device, DXCommandList& commandList, const ShaderInputDesc& desc, uint32_t offsetIndex)
{

}

void KS::Skydome::BindForCreation(const Device& device, DXCommandList& commandList, const ShaderInputDesc& desc, uint32_t offsetIndex)
{

}

size_t KS::Skydome::GetGPUAddress(int elementIndex, int frameIndex) const { return size_t(); }

uint32_t KS::Skydome::GetHandleIndex(bool readOnly) const { return 0; }
