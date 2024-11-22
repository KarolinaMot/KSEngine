#pragma once

#include <Common.hpp>

#include <SerializationCommon.hpp>
#include <resources/ByteView.hpp>
#include <vector>

class ByteBuffer
{
public:
    ByteBuffer() = default;
    ByteBuffer(const void* data, size_t byte_count);

    template <typename T>
    ByteBuffer(const T* data, size_t count)
        : ByteBuffer(static_cast<const void*>(data), count * sizeof(T))
    {
    }

    template <typename T>
    ByteView<T> GetView() const { return ByteView<T>(storage.data(), storage.size()); }

    size_t GetSize() const { return storage.size(); }
    const std::byte* GetData() const { return storage.data(); }

private:
    std::vector<std::byte> storage {};

    friend class cereal::access;
    void save(BinarySaver& ar, const uint32_t v) const;
    void load(BinaryLoader& ar, const uint32_t v);
};

CEREAL_CLASS_VERSION(ByteBuffer, 0);
