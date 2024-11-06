#include <Device.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXStructuredBuffer.hpp>
#include <renderer/StorageBuffer.hpp>

class StorageBuffer::Impl
{
public:
    std::unique_ptr<DXStructuredBuffer> m_buffer;
};

StorageBuffer::StorageBuffer()
{
    m_impl = new Impl();
}

StorageBuffer::~StorageBuffer()
{
    delete m_impl;
}

void StorageBuffer::CreateBuffer(const Device& device, const std::string& name, size_t dataSize, int numOfElements)
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

void StorageBuffer::UploadDataBuffer(const Device& device, const void* data, int numOfElements)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());

    m_impl->m_buffer->Update(commandList, data);
}

void StorageBuffer::Resize(const Device& device, int newNumOfElements)
{
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    m_num_elements = newNumOfElements;
    m_impl->m_buffer->Resize(engineDevice, newNumOfElements);
}

void StorageBuffer::Bind(const Device& device, int rootIndex, int elementIndex, bool readOnly)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    auto heap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());
    m_impl->m_buffer->Bind(commandList, rootIndex, readOnly, heap);
}

void StorageBuffer::BindAsVertexData(const Device& device, uint32_t inputSlot, uint32_t elementOffset)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    commandList->BindVertexData(m_impl->m_buffer->GetResource(), m_buffer_stride, inputSlot, elementOffset);
}

void StorageBuffer::BindAsIndexData(const Device& device, uint32_t elementOffset)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    commandList->BindIndexData(m_impl->m_buffer->GetResource(), m_buffer_stride, elementOffset);
}
