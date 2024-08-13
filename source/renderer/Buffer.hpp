#pragma once
#include <memory>
#include <string>
#include <tools/Log.hpp>
#include <vector>

namespace KS
{
class Device;
enum class BufferType
{
    CONST_BUFFER,
    STRUCTURED_BUFFER,
    VERTEX_DATA_BUFFER
};

class CommandList;
class Buffer
{
public:
    Buffer();
    ~Buffer();

    template <typename T>
    Buffer(const Device& device, const std::string& name, const std::vector<T>& data, bool readWriteEnabled)
        : Buffer(device, name, (const void*)(data.data()), sizeof(T), data.size() == 0 ? 1 : data.size(), readWriteEnabled)
    {
    }

    Buffer(const Device& device, const std::string& name, const void* data, size_t stride, size_t element_count, bool readWriteEnabled)
    {
        m_read_write = readWriteEnabled;
        m_buffer_stride = stride;
        m_total_buffer_size = stride * element_count;
        m_num_elements = element_count;
        m_name = name;

        CreateStructuredBuffer(device, name, stride, m_num_elements);
        UploadDataStructuredBuffer(device, data, m_num_elements);
    }

    // This type of buffer is better for data up to 64KB that is per-instance and does not need to be modified in the GPU
    template <typename T>
    Buffer(const Device& device, const std::string& name, const T& data, int instances, bool readWriteEnabled)
    {
        m_read_write = readWriteEnabled;
        m_total_buffer_size = sizeof(T) * instances;
        m_buffer_stride = sizeof(T);
        m_name = name;
        m_num_elements = instances;

        if (m_read_write)
        {
            CreateStructuredBuffer(device, name, sizeof(T), m_num_elements);
            UploadDataStructuredBuffer(device, &data, m_num_elements);
        }
        else
        {
            CreateConstBuffer(device, name, sizeof(T), m_num_elements);
            for (int i = 0; i < m_num_elements; i++)
                UploadDataConstBuffer(device, &data, i);
        }
    }

    template <typename T>
    void Update(const Device& device, const std::vector<T>& data)
    {
        if (sizeof(T) != m_buffer_stride)
        {
            LOG(Log::Severity::WARN, "Buffer {} type on update does not fit the original format. Command has been ignored.", m_name);
            return;
        }

        if (data.size() > m_num_elements)
            Resize(device, data.size());

        if (m_buffer_type == BufferType::STRUCTURED_BUFFER)
            UploadDataStructuredBuffer(device, data.data(), data.size());
        else if (sizeof(T) * data.size() <= m_total_buffer_size)
            UploadBigDataConstBuffer(device, data.data(), sizeof(T) * data.size());
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

        if (m_buffer_type == BufferType::STRUCTURED_BUFFER)
            UploadDataStructuredBuffer(device, &data, 1);
        else
            UploadDataConstBuffer(device, &data, member);
    }

    void Resize(const Device& device, int newNumOfElements);
    void BindToGraphics(const Device& device, int rootIndex, int elementIndex = 0, bool readOnly = true);
    void BindToCompute(const Device& device, int rootIndex, int elementIndex = 0, bool readOnly = true);
    void BindAsVertexData(const Device& device, uint32_t inputSlot, uint32_t elementOffset = 0);
    void BindAsIndexData(const Device& device, uint32_t elementOffset = 0);

    size_t GetBufferStride() const { return m_buffer_stride; }
    size_t GetBufferSize() const { return m_total_buffer_size; }
    size_t GetElementCount() const { return m_num_elements; }

private:
    void CreateStructuredBuffer(const Device& device, const std::string& name, size_t dataSize, int numOfElements);
    void CreateConstBuffer(const Device& device, const std::string& name, size_t dataSize, int numOfElements);
    void CreateTexture(const Device& device, const std::string& name, uint32_t width, uint32_t height, size_t dataSize, bool readWriteEnabled);

    void UploadDataStructuredBuffer(const Device& device, const void* data, int numOfElements);
    void UploadDataConstBuffer(const Device& device, const void* data, uint32_t offset);
    void UploadBigDataConstBuffer(const Device& device, const void* data, size_t dataSize);

    BufferType m_buffer_type;
    bool m_read_write = false;
    size_t m_total_buffer_size = 0;
    size_t m_buffer_stride = 0;
    int m_num_elements = 0;
    std::string m_name;

    class Impl;
    Impl* m_impl;
};

} // namespace KS
