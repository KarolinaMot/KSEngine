#pragma once
#include <memory>
#include <string>
#include <tools/Log.hpp>
#include <vector>
#include <renderer/ShaderInput.hpp>

namespace KS
{
class Device;
class CommandList;
class StorageBuffer : public ShaderInput
{
public:
    StorageBuffer();
    ~StorageBuffer();

    template <typename T>
    StorageBuffer(const Device& device, const std::string& name, const std::vector<T>& data, bool readWriteEnabled)
        : StorageBuffer(device, name, (const void*)(data.data()), sizeof(T), data.size() == 0 ? 1 : data.size(),
                        readWriteEnabled)
    {
    }

    StorageBuffer(const Device& device, const std::string& name, const void* data, size_t stride, size_t element_count,
                  bool readWriteEnabled)
    {
        m_read_write = readWriteEnabled;
        m_buffer_stride = stride;
        m_total_buffer_size = stride * element_count;
        m_num_elements = element_count;
        m_name = name;

        CreateBuffer(device, name, stride, m_num_elements);
        UploadDataBuffer(device, data, m_num_elements);
    }

    template <typename T>
    void Update(const Device& device, const std::vector<T>& data)
    {
        if (sizeof(T) != m_buffer_stride)
        {
            LOG(Log::Severity::WARN,
                "StorageBuffer {} type on update does not fit the original format. Command has been ignored.", m_name);
            return;
        }

        if (data.size() > m_num_elements) Resize(device, data.size());

        UploadDataBuffer(device, data.data(), data.size());
    }

    template <typename T>
    void Update(const Device& device, const T* data, size_t numElements)
    {
        if (sizeof(T) != m_buffer_stride)
        {
            LOG(Log::Severity::WARN,
                "StorageBuffer {} type on update does not fit the original format. Command has been ignored.", m_name);
            return;
        }

        if (data == nullptr)
        {
            LOG(Log::Severity::WARN, "StorageBuffer {} got an invalid data value for update. Command ignored.", m_name);
            return;
        }

        if (numElements > m_num_elements) Resize(device, numElements);

        UploadDataBuffer(device, data, numElements);
    }

    void Resize(const Device& device, int newNumOfElements);
    virtual void Bind(Device& device, const ShaderInputDesc& desc, uint32_t offsetIndex = 0) override;
    void BindAsVertexData(const Device& device, uint32_t inputSlot, uint32_t elementOffset = 0);
    void BindAsIndexData(const Device& device, uint32_t elementOffset = 0);
    void AllocateAsReadOnly(Device& device, int slot = -1);
    void AllocateAsReadWrite(Device& device, int slot = -1);

    size_t GetBufferStride() const { return m_buffer_stride; }
    size_t GetBufferSize() const { return m_total_buffer_size; }
    size_t GetElementCount() const { return m_num_elements; }
    bool IsReadWrite() const { return m_read_write; }
    void* GetRawRealResource() const;
    void* GetRawResource() const;
    int GetAllocationIndex(bool readOnly);

private:
    void CreateBuffer(const Device& device, const std::string& name, size_t dataSize, int numOfElements);
    void UploadDataBuffer(const Device& device, const void* data, int numOfElements);

    bool m_read_write = false;
    size_t m_total_buffer_size = 0;
    size_t m_buffer_stride = 0;
    int m_num_elements = 0;
    std::string m_name;

    class Impl;
    Impl* m_impl;
};

}  // namespace KS
