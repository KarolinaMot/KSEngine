#pragma once
#include <memory>
#include <string>
#include <tools/Log.hpp>
#include <vector>
#include <renderer/ShaderInput.hpp>

struct DXCommandContext;
namespace KS
{
class UploadArena;
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
    StorageBuffer(const Device& device, DXCommandList& commandList, const std::string& name,
                  const std::vector<T>& data,
                  bool readWriteEnabled, int flags = StorageBufferFlags::NONE)
        : StorageBuffer(device, commandList, name, (const void*)(data.data()), sizeof(T), data.size() == 0 ? 1 : static_cast<uint32_t>(data.size()),
                        readWriteEnabled, flags)
    {
    }

    StorageBuffer(const Device& device, DXCommandList& commandList, const std::string& name, const void* data,
                  uint32_t stride,
                  uint32_t element_count, bool readWriteEnabled, int flags = StorageBufferFlags::NONE)
    {
        m_read_write = readWriteEnabled;
        m_buffer_stride = stride;
        m_num_elements = element_count;
        m_total_buffer_size = stride * m_num_elements;
        m_name = name;
        m_flags = flags;

        CreateBuffer(device,commandList, name, m_num_elements);
        UploadDataBuffer(device, commandList, data, m_num_elements);
    }

    template <typename T>
    void Update(const Device& device, DXCommandList& commandList, const T* data, uint32_t numElements)
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

        if (numElements > m_num_elements) Resize(device, commandList, numElements);
        UploadDataBuffer(device, commandList, data, numElements);
    }

    template <typename T>
    void Update(const Device& device, DXCommandList& commandList, const std::vector<T>& data)
    {
        Update(device, commandList, data.data(), static_cast<uint32_t>(data.size()));
    }


    void Resize(const Device& device, DXCommandList& commandList, uint32_t newNumOfElements);
    virtual void Bind(const Device& device, DXCommandList& commandList, const ShaderInputDesc& desc,
                      uint32_t offsetIndex = 0) override;
    void BindAsVertexData(DXCommandList& commandList, uint32_t inputSlot, uint32_t elementOffset = 0);
    void BindAsIndexData(DXCommandList& commandList, uint32_t elementOffset = 0);
    void AllocateAsReadOnly(const Device& device, int slot = -1);
    void AllocateAsReadWrite(const Device& device, int slot = -1);

    uint32_t GetBufferStride() const { return m_buffer_stride; }
    size_t GetBufferSize() const { return m_total_buffer_size; }
    uint32_t GetElementCount() const { return m_num_elements; }
    size_t GetGPUAddress(int elementIndex, int frameIndex) const override;
    bool IsReadWrite() const { return m_read_write; }
    void* GetRawRealResource() const;
    void* GetRawResource() const;
    int GetAllocationIndex(bool readOnly);


private:
    void CreateBuffer(const Device& device, DXCommandList& commandList, const std::string& name,
                      uint32_t numOfElements);
    void UploadDataBuffer(const Device& device, DXCommandList& commandList, const void* data, uint32_t numOfElements);

    bool m_read_write = false;
    size_t m_total_buffer_size = 0;
    uint32_t m_buffer_stride = 0;
    uint32_t m_num_elements = 0;
    std::string m_name;
    int m_flags = 0;

    class Impl;
    Impl* m_impl;
};

}  // namespace KS
