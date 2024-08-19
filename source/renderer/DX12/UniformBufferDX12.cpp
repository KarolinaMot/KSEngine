#include <renderer/UniformBuffer.hpp>
#include <renderer/DX12/Helpers/DXConstBuffer.hpp>
#include <device/Device.hpp>

class KS::UniformBuffer::Impl
{
public:
    std::unique_ptr<DXConstBuffer> m_buffer;
};

KS::UniformBuffer::UniformBuffer()
{
    m_impl = new Impl();
}

KS::UniformBuffer::~UniformBuffer()
{
    delete m_impl;
}

void KS::UniformBuffer::CreateUniformBuffer(const Device& device, std::string name, size_t elementSize, int numberOfElements)
{
    m_impl = new Impl();

    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());

    m_impl->m_buffer = std::make_unique<DXConstBuffer>(engineDevice,
        elementSize,
        numberOfElements,
        name.c_str(),
        FRAME_BUFFER_COUNT);
}

void KS::UniformBuffer::Resize(const Device& device, int newNumOfElements)
{
    m_num_elements = newNumOfElements;
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    m_impl->m_buffer->Resize(engineDevice, newNumOfElements);
}

void KS::UniformBuffer::Upload(const Device& device, const void* data, uint32_t offset)
{
    m_impl->m_buffer->Update(data, m_buffer_stride, offset, device.GetFrameIndex());
}
