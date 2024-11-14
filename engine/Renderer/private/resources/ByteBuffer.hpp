#pragma once

#include <Common.hpp>

#include <resources/Serialization.hpp>
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
    class View
    {
    public:
        View(const ByteBuffer* source)
            : source(source)
        {
        }

        const T* begin() const;
        const T* end() const;
        size_t count() const;

    private:
        const ByteBuffer* source {};
    };

    template <typename T>
    View<T> GetView() const
    {
        assert(storage.size() % sizeof(T) == 0
            && "Size of buffer must be divisible by the size of T for proper iteration");
        return View<T>(this);
    }

    size_t GetSize() const { return storage.size(); }
    const void* GetData() const { return storage.data(); }

    template <typename A>
    void save(A& ar, const uint32_t v) const;

    template <typename A>
    void load(A& ar, const uint32_t v);

private:
    friend class cereal::access;

    std::vector<uint8_t> storage {};
};

template <typename T>
inline const T* ByteBuffer::View<T>::begin() const
{
    return reinterpret_cast<const T*>(source->storage.data());
}

template <typename T>
inline const T* ByteBuffer::View<T>::end() const
{
    return reinterpret_cast<const T*>(source->storage.data() + source->storage.size());
}

template <typename T>
inline size_t ByteBuffer::View<T>::count() const
{
    return end() - begin();
}

template <typename A>
inline void ByteBuffer::save(A& ar, const uint32_t v) const
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

template <typename A>
inline void ByteBuffer::load(A& ar, const uint32_t v)
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

CEREAL_CLASS_VERSION(ByteBuffer, 0);
