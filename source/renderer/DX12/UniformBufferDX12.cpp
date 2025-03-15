#include <device/Device.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXResource.hpp>
#include <renderer/UniformBuffer.hpp>
#include <renderer/ShaderInputCollection.hpp>

class KS::UniformBuffer::Impl
{
public:
    std::unique_ptr<DXResource> mBuffers[FRAME_BUFFER_COUNT];
};

KS::UniformBuffer::UniformBuffer() { m_impl = new Impl(); }

KS::UniformBuffer::~UniformBuffer() { delete m_impl; }

void KS::UniformBuffer::CreateUniformBuffer(const Device& device)
{
    m_impl = new Impl();

    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());

    for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT64>(m_total_buffer_size));

        m_impl->mBuffers[i] = std::make_unique<DXResource>(engineDevice, heapProperties, resourceDesc, nullptr, m_name.c_str(),
                                                           D3D12_RESOURCE_STATE_GENERIC_READ);

        CD3DX12_RANGE readRange(0, 0);
        HRESULT hr = m_impl->mBuffers[i]->GetResource()->Map(0, &readRange, reinterpret_cast<void**>(&m_buffer_GPU_Address[i]));
        if (FAILED(hr)) ASSERT(false && "Buffer mapping failed");
    }
}

void KS::UniformBuffer::Resize(const Device& device, int newNumOfElements)
{
    if (m_num_elements == newNumOfElements) return;

    m_num_elements = newNumOfElements;
    m_total_buffer_size = m_buffer_stride * m_num_elements;

    CreateUniformBuffer(device);
}

size_t KS::UniformBuffer::GetGPUAddress(int elementIndex, int frameIndex) const
{
    return m_impl->mBuffers[frameIndex]->GetResource()->GetGPUVirtualAddress() + (m_buffer_stride * elementIndex);
}

void KS::UniformBuffer::Bind(Device& device, const ShaderInputDesc& desc, uint32_t offsetIndex)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    commandList->BindBuffer(m_impl->mBuffers[device.GetFrameIndex()], desc.rootIndex, m_buffer_stride, offsetIndex);
}

void KS::UniformBuffer::Upload(const Device& device, const void* data, uint32_t offset)
{
    memcpy(m_buffer_GPU_Address[device.GetFrameIndex()] + (m_buffer_stride * offset), data, m_element_size);
}
