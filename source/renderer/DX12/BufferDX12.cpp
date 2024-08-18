#include "../../device/Device.hpp"
#include "../Buffer.hpp"
#include "Helpers/DXConstBuffer.hpp"
#include "Helpers/DXDescHeap.hpp"
#include "Helpers/DXResource.hpp"
#include "Helpers/DXStructuredBuffer.hpp"
#include "Helpers/DXCommandList.hpp"

class KS::Buffer::Impl
{
public:
    std::unique_ptr<DXConstBuffer> m_const_buffer;
    std::shared_ptr<DXStructuredBuffer> m_structured_buffer;
};

KS::Buffer::Buffer()
{
    m_impl = new Impl();
}

KS::Buffer::~Buffer()
{
    delete m_impl;
}

void KS::Buffer::Resize(const Device& device, int newNumOfElements)
{
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    m_num_elements = newNumOfElements;
    if (m_buffer_type == BufferType::STRUCTURED_BUFFER)
        m_impl->m_structured_buffer->Resize(engineDevice, newNumOfElements);
    else if (m_buffer_type == BufferType::CONST_BUFFER)
        m_impl->m_const_buffer->Resize(engineDevice, newNumOfElements);
}

void KS::Buffer::BindToGraphics(const Device& device, int rootIndex, int elementIndex, bool readOnly)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    auto heap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());

    if (m_buffer_type == BufferType::STRUCTURED_BUFFER)
        m_impl->m_structured_buffer->Bind(commandList, rootIndex, readOnly, heap);
    else if (m_buffer_type == BufferType::CONST_BUFFER)
        m_impl->m_const_buffer->Bind(commandList, rootIndex, elementIndex, device.GetFrameIndex());
}

void KS::Buffer::BindToCompute(const Device& device, int rootIndex, int elementIndex, bool readOnly)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    auto heap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());

    if (m_buffer_type == BufferType::STRUCTURED_BUFFER)
        m_impl->m_structured_buffer->Bind(commandList, rootIndex, readOnly, heap);
    else if (m_buffer_type == BufferType::CONST_BUFFER)
        m_impl->m_const_buffer->Bind(commandList, rootIndex, elementIndex, device.GetFrameIndex());
}

void KS::Buffer::BindAsVertexData(const Device& device, uint32_t inputSlot, uint32_t elementOffset)
{
    if (m_buffer_type != BufferType::STRUCTURED_BUFFER)
    {
        LOG(Log::Severity::WARN, "Buffer {} is not a vertex data type, but is being bound as such. Command has been ignored.", m_name);
        return;
    }

    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    commandList->BindVertexData(m_impl->m_structured_buffer->GetResource(), m_buffer_stride, inputSlot, elementOffset);
}

void KS::Buffer::BindAsIndexData(const Device& device, uint32_t elementOffset)
{
    if (m_buffer_type != BufferType::STRUCTURED_BUFFER)
    {
        LOG(Log::Severity::WARN, "Buffer {} is not a index data type, but is being bound as such. Command has been ignored.", m_name);
        return;
    }

    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    commandList->BindIndexData(m_impl->m_structured_buffer->GetResource(), m_buffer_stride, elementOffset);
}

void KS::Buffer::CreateStructuredBuffer(const Device& device, const std::string& name, size_t dataSize, int numOfElements)
{
    m_impl = new Impl();
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());

    m_buffer_type = BufferType::STRUCTURED_BUFFER;
    if (m_read_write)
        m_impl->m_structured_buffer = std::make_unique<DXStructuredBuffer>(engineDevice, dataSize, numOfElements, name.c_str(),
            FRAME_BUFFER_COUNT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    else
        m_impl->m_structured_buffer = std::make_unique<DXStructuredBuffer>(engineDevice, dataSize, numOfElements, name.c_str(),
            FRAME_BUFFER_COUNT);
}

void KS::Buffer::CreateConstBuffer(const Device& device, const std::string& name, size_t dataSize, int numOfElements)
{
    m_impl = new Impl();
    m_buffer_type = BufferType::CONST_BUFFER;

    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());

    m_impl->m_const_buffer = std::make_unique<DXConstBuffer>(engineDevice,
        dataSize,
        numOfElements,
        name.c_str(),
        FRAME_BUFFER_COUNT);
}

void KS::Buffer::UploadDataStructuredBuffer(const Device& device, const void* data, int numOfElements)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());

    m_impl->m_structured_buffer->Update(commandList, data);
}

void KS::Buffer::UploadBigDataConstBuffer(const Device& device, const void* data, size_t dataSize)
{
    m_impl->m_const_buffer->Update(data, dataSize, 0, device.GetFrameIndex());
}

void KS::Buffer::UploadDataConstBuffer(const Device& device, const void* data, uint32_t offset)
{
    m_impl->m_const_buffer->Update(data, m_buffer_stride, offset, device.GetFrameIndex());
}
