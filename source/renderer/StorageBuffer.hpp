#pragma once
#include <memory>
#include <string>
#include <tools/Log.hpp>
#include <vector>
#include <renderer/ShaderInput.hpp>

class DXCommandList;


namespace KS
{
class Device;
class StorageBuffer : public ShaderInput
{
public:

    enum StorageBufferFlags
    {
        NONE = 1 << 0,
        INDEX_DATA_BUFFER = 1 << 1,
        VERTEX_DATA_BUFFER = 1 << 2
    };

    StorageBuffer();
    ~StorageBuffer();

    template <typename T>
    StorageBuffer(const Device& device, DXCommandList& commandList, const std::string& name, const std::vector<T>& data,
                  bool readWriteEnabled, int flags = StorageBufferFlags::NONE)
        : StorageBuffer(device, commandList, name, (const void*)(data.data()), sizeof(T), data.size() == 0 ? 1 : data.size(),
                        readWriteEnabled, flags)
    {
    }

    StorageBuffer(const Device& device, DXCommandList& commandList, const std::string& name, const void* data, size_t stride,
                  size_t element_count, bool readWriteEnabled, int flags = StorageBufferFlags::NONE)
    {
        m_read_write = readWriteEnabled;
        m_buffer_stride = stride;
        m_total_buffer_size = stride * element_count;
        m_num_elements = element_count;
        m_name = name;
        m_flags = flags;

        CreateBuffer(device, name, stride, m_num_elements);
        UploadDataBuffer(commandList, data, m_num_elements);
    }

    template <typename T>
    void Update(const Device& device, DXCommandList& commandList, const std::vector<T>& data)
    {
        if (sizeof(T) != m_buffer_stride)
        {
            LOG(Log::Severity::WARN,
                "StorageBuffer {} type on update does not fit the original format. Command has been ignored.", m_name);
            return;
        }

        if (data.size() > m_num_elements) Resize(device, data.size());

        UploadDataBuffer(commandList, data.data(), data.size());
    }

    template <typename T>
    void Update(const Device& device, DXCommandList& commandList, const T* data, size_t numElements)
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

        UploadDataBuffer(commandList, data, numElements);
    }

    void Resize(const Device& device, int newNumOfElements);
    virtual void Bind(const Device& device, DXCommandList& commandList, const ShaderInputDesc& desc,
                      uint32_t offsetIndex = 0) override;
    void BindAsVertexData(DXCommandList& commandList, uint32_t inputSlot, uint32_t elementOffset = 0);
    void BindAsIndexData(DXCommandList& commandList, uint32_t elementOffset = 0);
    void AllocateAsReadOnly(const Device& device, int slot = -1);
    void AllocateAsReadWrite(const Device& device, int slot = -1);

    size_t GetBufferStride() const { return m_buffer_stride; }
    size_t GetBufferSize() const { return m_total_buffer_size; }
    size_t GetElementCount() const { return m_num_elements; }
    size_t GetGPUAddress(int elementIndex, int frameIndex) const override;
    bool IsReadWrite() const { return m_read_write; }
    void* GetRawRealResource() const;
    void* GetRawResource() const;
    int GetAllocationIndex(bool readOnly);


private:
    void CreateBuffer(const Device& device, const std::string& name, size_t dataSize,
                      int numOfElements);
    void UploadDataBuffer(DXCommandList& commandList, const void* data, int numOfElements);

    bool m_read_write = false;
    size_t m_total_buffer_size = 0;
    size_t m_buffer_stride = 0;
    int m_num_elements = 0;
    std::string m_name;
    int m_flags = 0;

    class Impl;
    Impl* m_impl;
};

}  // namespace KS
