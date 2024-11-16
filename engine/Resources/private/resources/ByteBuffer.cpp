#include <resources/ByteBuffer.hpp>

ByteBuffer::ByteBuffer(const void* data, size_t byte_count)
{
    storage.resize(byte_count);
    std::memcpy(storage.data(), data, byte_count);
}
