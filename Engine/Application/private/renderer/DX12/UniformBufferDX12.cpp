#include <Device.hpp>
#include <renderer/DX12/Helpers/DXConstBuffer.hpp>
#include <renderer/UniformBuffer.hpp>

class UniformBuffer::Impl
{
public:
    std::unique_ptr<DXConstBuffer> m_buffer;
};

UniformBuffer::UniformBuffer()
{
    m_impl = new Impl();
}

UniformBuffer::~UniformBuffer()
{
    delete m_impl;
}

void UniformBuffer::CreateUniformBuffer(const Device& device, std::string name, size_t elementSize, int numberOfElements)
{
    m_impl = new Impl();

    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());

    m_impl->m_buffer = std::make_unique<DXConstBuffer>(engineDevice,
        elementSize,
        numberOfElements,
        name.c_str(),
        FRAME_BUFFER_COUNT);
}

void UniformBuffer::Resize(const Device& device, int newNumOfElements)
{
    m_num_elements = newNumOfElements;
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    m_impl->m_buffer->Resize(engineDevice, newNumOfElements);
}

void UniformBuffer::Bind(const Device& device, int rootIndex, int elementIndex)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());

    m_impl->m_buffer->Bind(commandList, rootIndex, elementIndex, device.GetFrameIndex());
}

void UniformBuffer::Upload(const Device& device, const void* data, uint32_t offset)
{
    m_impl->m_buffer->Update(data, m_buffer_stride, offset, device.GetFrameIndex());
}
