#pragma once

#include <Common.hpp>
#include <memory>

// Wrapper for a shared pointer
template <typename T>
class ResourceHandle
{
public:
    explicit ResourceHandle(std::shared_ptr<T> res)
        : ptr(res)
    {
    }

    explicit ResourceHandle(T&& res)
        : ptr(std::make_shared<T>(std::move(res)))
    {
    }

    const T& Get() const { return *ptr; }
    T& Get() { return *ptr; }

private:
    std::shared_ptr<T> ptr;
};