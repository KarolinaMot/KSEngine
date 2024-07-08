#pragma once
#include <vector>

namespace KS
{

// Not used currently, could be useful for something
template <typename T>
class SlotMap
{
public:
    using Key = uint64_t;
    using Version = uint32_t;
    using Index = uint32_t;
    using Self = SlotMap<T>;

    void Reserve(size_t capacity)
    {
        storage.reserve(capacity);
    }

    void Clear()
    {
        storage.clear();
        free_list.clear();
    }

    size_t Size() const
    {
        return storage.size();
    }

    bool Contains(Key k) const
    {

        auto [index, version] = SplitKey(k);

        if (index >= storage.size())
        {
            return false;
        }

        auto& [current_version, element] = storage.at(index);

        if (current_version != version)
        {
            return false;
        }

        return true;
    }

    const T* Get(Key k) const
    {
        if (Contains(k))
        {
            auto [index, version] = SplitKey(k);
            auto& [current_version, element] = storage.at(index);
            return &element;
        }

        return nullptr;
    }

    T* Get(Key k)
    {
        return const_cast<T*>(const_cast<const Self*>(this)->Get(k));
    }

    Key Insert(const T& val)
    {
        T copy = val;
        return Insert(std::move(copy));
    }

    Key Insert(T&& val)
    {
        auto new_index = NewIndex();
        auto& [version, new_element] = storage.at(new_index);
        new_element = std::move(val);
        return MakeKey(new_index, version);
    }

    void Erase(Key k)
    {

        if (Contains(k))
        {
            auto [index, version] = SplitKey(k);
            auto& [current_version, element] = storage.at(index);

            free_list.emplace_back(index);
            current_version += 1;
            element.~T();
        }
    }

private:
    std::pair<Index, Version> SplitKey(Key k) const
    {
        return { k >> 32, Version(k) };
    }

    Key MakeKey(Index i, Version v) const
    {
        return (uint64_t(i) << 32) | (uint64_t(v));
    }

    Index NewIndex()
    {
        if (free_list.empty())
        {
            storage.emplace_back();
            return storage.size() - 1;
        }

        return free_list.back();
    }

    // Holds pairs of version, item
    std::vector<std::pair<Version, T>> storage {};

    // Keeps track of freed elements
    std::vector<Index> free_list {};
};

namespace Tests
{
    void TestSlotMap();
}

}