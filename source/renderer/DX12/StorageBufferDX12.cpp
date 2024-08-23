#include <renderer/StorageBuffer.hpp>
#include <renderer/DX12/Helpers/DXStructuredBuffer.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <device/Device.hpp>

class KS::StorageBuffer::Impl
{
public:
    std::unique_ptr<DXStructuredBuffer> m_buffer;
};

KS::StorageBuffer::StorageBuffer()
{
    m_impl = new Impl();
}

KS::StorageBuffer::~StorageBuffer()
{
    delete m_impl;
}

void KS::StorageBuffer::CreateBuffer(const Device& device, const std::string& name, size_t dataSize, int numOfElements)
{
    m_impl = new Impl();
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());

    if (m_read_write)
        m_impl->m_buffer = std::make_unique<DXStructuredBuffer>(engineDevice, dataSize, numOfElements, name.c_str(),
            FRAME_BUFFER_COUNT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    else
        m_impl->m_buffer = std::make_unique<DXStructuredBuffer>(engineDevice, dataSize, numOfElements, name.c_str(),
            FRAME_BUFFER_COUNT);
}

void KS::StorageBuffer::UploadDataBuffer(const Device& device, const void* data, int numOfElements)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());

    m_impl->m_buffer->Update(commandList, data);
}

void KS::StorageBuffer::Resize(const Device& device, int newNumOfElements)
{
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    m_num_elements = newNumOfElements;
    m_impl->m_buffer->Resize(engineDevice, newNumOfElements);
}

void KS::StorageBuffer::Bind(const Device& device, int rootIndex, bool readOnly)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    auto heap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());
    m_impl->m_buffer->Bind(commandList, rootIndex, readOnly, heap);
}

void KS::StorageBuffer::BindAsVertexData(const Device& device, uint32_t inputSlot, uint32_t elementOffset)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    commandList->BindVertexData(m_impl->m_buffer->GetResource(), m_buffer_stride, inputSlot, elementOffset);
}

void KS::StorageBuffer::BindAsIndexData(const Device& device, uint32_t elementOffset)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    commandList->BindIndexData(m_impl->m_buffer->GetResource(), m_buffer_stride, elementOffset);
}
