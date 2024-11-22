#pragma once

#include <Common.hpp>

template <typename T>
class ByteView
{
public:
    ByteView(const std::byte* source, std::size_t byte_size)
        : source(source)
        , byte_size(byte_size)
    {
        assert(byte_size % sizeof(T) == 0 && "Size of view must be divisible by the size of T for proper iteration");
        assert(byte_size > 0 && "Size of view must be bigger than 0");
    }

    const T* begin() const { return reinterpret_cast<const T*>(source); }
    const T* end() const { return begin() + byte_size; };
    size_t count() const { return byte_size / sizeof(T); };

private:
    const std::byte* source {};
    size_t byte_size {};
};