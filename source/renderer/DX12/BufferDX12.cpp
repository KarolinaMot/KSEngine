#include "../../device/Device.hpp"
#include "../Buffer.hpp"
#include "Helpers/DXConstBuffer.hpp"
#include "Helpers/DXDescHeap.hpp"
#include "Helpers/DXResource.hpp"
#include "Helpers/DXStructuredBuffer.hpp"

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
    auto commandList = reinterpret_cast<ID3D12GraphicsCommandList4*>(device.GetCommandList());
    auto heap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());

    if (m_buffer_type == BufferType::STRUCTURED_BUFFER)
        m_impl->m_structured_buffer->BindToGraphics(commandList, rootIndex, readOnly, heap);
    else if (m_buffer_type == BufferType::CONST_BUFFER)
        m_impl->m_const_buffer->BindToGraphics(commandList, rootIndex, elementIndex, device.GetFrameIndex());
}

void KS::Buffer::BindToCompute(const Device& device, int rootIndex, int elementIndex, bool readOnly)
{
    auto commandList = reinterpret_cast<ID3D12GraphicsCommandList4*>(device.GetCommandList());
    auto heap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());

    if (m_buffer_type == BufferType::STRUCTURED_BUFFER)
        m_impl->m_structured_buffer->BindToCompute(commandList, rootIndex, readOnly, heap);
    else if (m_buffer_type == BufferType::CONST_BUFFER)
        m_impl->m_const_buffer->BindToCompute(commandList, rootIndex, elementIndex, device.GetFrameIndex());
}

void KS::Buffer::BindAsVertexData(const Device& device, uint32_t inputSlot, uint32_t elementOffset)
{
    if (m_buffer_type != BufferType::STRUCTURED_BUFFER)
    {
        LOG(Log::Severity::WARN, "Buffer {} is not a vertex data type, but is being bound as such. Command has been ignored.", m_name);
        return;
    }

    auto commandList = reinterpret_cast<ID3D12GraphicsCommandList4*>(device.GetCommandList());

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};
    vertexBufferView.BufferLocation = m_impl->m_structured_buffer->GetGPUVirtualAddress() + elementOffset * m_buffer_stride;
    vertexBufferView.StrideInBytes = m_buffer_stride;
    vertexBufferView.SizeInBytes = m_total_buffer_size;

    commandList->IASetVertexBuffers(inputSlot, 1, &vertexBufferView);
}

void KS::Buffer::BindAsIndexData(const Device& device, uint32_t elementOffset)
{
    if (m_buffer_type != BufferType::STRUCTURED_BUFFER)
    {
        LOG(Log::Severity::WARN, "Buffer {} is not a index data type, but is being bound as such. Command has been ignored.", m_name);
        return;
    }

    auto commandList = reinterpret_cast<ID3D12GraphicsCommandList4*>(device.GetCommandList());

    D3D12_INDEX_BUFFER_VIEW indexBufferView {};
    indexBufferView.BufferLocation = m_impl->m_structured_buffer->GetGPUVirtualAddress() + elementOffset * m_buffer_stride;
    indexBufferView.SizeInBytes = m_total_buffer_size;

    switch (m_buffer_stride)
    {
    case sizeof(unsigned char):
        indexBufferView.Format = DXGI_FORMAT_R8_UINT;
        break;
    case sizeof(unsigned short):
        indexBufferView.Format = DXGI_FORMAT_R16_UINT;
        break;
    case sizeof(unsigned int):
        indexBufferView.Format = DXGI_FORMAT_R32_UINT;
        break;
    default:
        LOG(Log::Severity::WARN, "Buffer {} format do not fit the expected format of an index buffer format, although it is being bound as one. Command has been ignored.", m_name);
        return;
    }

    commandList->IASetIndexBuffer(&indexBufferView);
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
    auto commandList = reinterpret_cast<ID3D12GraphicsCommandList4*>(device.GetCommandList());
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
