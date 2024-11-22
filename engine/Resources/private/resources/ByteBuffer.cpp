#include <cereal/types/vector.hpp>
#include <resources/ByteBuffer.hpp>

ByteBuffer::ByteBuffer(const void* data, size_t byte_count)
{
    storage.resize(byte_count);
    std::memcpy(storage.data(), data, byte_count);
}

void ByteBuffer::save(BinarySaver& ar, const uint32_t v) const
{
    switch (v)
    {
    case 0:
        ar(cereal::make_nvp("Contents", storage));
        break;

    default:
        break;
    }
}

void ByteBuffer::load(BinaryLoader& ar, const uint32_t v)
{
    switch (v)
    {
    case 0:
    {
        ar(cereal::make_nvp("Contents", storage));
        break;
    }
    default:
        break;
    }
}