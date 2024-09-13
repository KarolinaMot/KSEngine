#pragma once
#include <memory>
#include <string>
#include <tools/Log.hpp>
#include <vector>

namespace KS
{
class Device;
class UniformBuffer
{
public:
    UniformBuffer();
    ~UniformBuffer();

    // This type of buffer is better for data up to 64KB that is per-instance and does not need to be modified in the GPU
    template <typename T>
    UniformBuffer(const Device& device, const std::string& name, const T& data, int instances)
    {
        m_total_buffer_size = sizeof(T) * instances;
        m_buffer_stride = sizeof(T);
        m_name = name;
        m_num_elements = instances;

        CreateUniformBuffer(device, name, sizeof(T), m_num_elements);
        for (int i = 0; i < m_num_elements; i++)
            Upload(device, &data, i);
    }

    template <typename T>
    void Update(const Device& device, const T& data, int member = 0)
    {
        if (sizeof(T) != m_buffer_stride)
        {
            LOG(Log::Severity::WARN, "Buffer {} type on update does not fit the original format. Command has been ignored.", m_name);
            return;
        }

        if (member >= m_num_elements)
            Resize(device, member + 1);

        Upload(device, &data, member);
    }

    void Resize(const Device& device, int newNumOfElements);

    size_t GetBufferStride() const { return m_buffer_stride; }
    size_t GetBufferSize() const { return m_total_buffer_size; }
    size_t GetElementCount() const { return m_num_elements; }
    void Bind(const Device& device, int rootIndex, int elementIndex);

private:
    void CreateUniformBuffer(const Device& device, std::string name, size_t elementSize, int numberOfElements);
    void Upload(const Device& device, const void* data, uint32_t offset);

    size_t m_total_buffer_size = 0;
    size_t m_buffer_stride = 0;
    int m_num_elements = 0;
    std::string m_name;

    class Impl;
    Impl* m_impl;
};

} // namespace KS
