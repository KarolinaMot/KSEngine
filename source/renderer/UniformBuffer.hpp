#pragma once
#include <memory>
#include <string>
#include <tools/Log.hpp>
#include <vector>
#include <renderer/ShaderInput.hpp>

namespace KS
{
class Device;
class UniformBuffer : public ShaderInput
{
public:
    UniformBuffer();
    ~UniformBuffer();

    // This type of buffer is better for data up to 64KB that is per-instance and does not need to be modified in the GPU
    template <typename T>
    UniformBuffer(const Device& device, const std::string& name, const T& data, int instances, bool doubleBuffer = true)
    {
        m_buffer_stride = (sizeof(T) + 255) & ~255;  // Aligning the buffer per object size
        m_total_buffer_size = m_buffer_stride * instances;
        m_name = name;
        m_num_elements = instances;
        m_element_size = sizeof(T);
        m_double_buffer = doubleBuffer;

        CreateUniformBuffer(device);
        for (int i = 0; i < m_num_elements; i++) Upload(device, &data, i);
    }

    template <typename T>
    void Update(const Device& device, const T& data, int member = 0)
    {
        int dataSize = sizeof(T);
        if (dataSize != m_element_size)
        {
            LOG(Log::Severity::WARN, "Buffer {} type on update does not fit the original format. Command has been ignored.",
                m_name);
            return;
        }

        if (member >= m_num_elements) Resize(device, member + 1);

        Upload(device, &data, member);
    }

    void Resize(const Device& device, int newNumOfElements);

    size_t GetBufferStride() const { return m_buffer_stride; }
    size_t GetBufferSize() const { return m_total_buffer_size; }
    size_t GetElementCount() const { return m_num_elements; }
    size_t GetGPUAddress(int elementIndex, int frameIndex) const override;

    virtual void Bind(Device& device, const ShaderInputDesc& desc, uint32_t offsetIndex = 0) override;

private:
    void CreateUniformBuffer(const Device& device);
    void Upload(const Device& device, const void* data, uint32_t offset);

    size_t m_total_buffer_size = 0;
    size_t m_buffer_stride = 0;
    size_t m_element_size = 0;
    uint8_t* m_buffer_GPU_Address[2] = {0, 0};

    int m_num_elements = 0;
    std::string m_name;
    bool m_double_buffer = false;

    class Impl;
    Impl* m_impl;
};

}  // namespace KS
