#pragma once
#include "DXFence.hpp"
#include <sync/DXFence.hpp>

class DXFuture
{
public:
    DXFuture() = default;

    DXFuture(DXFence* fence, uint64_t target_val)
        : bound_fence(fence)
        , future_value(target_val)
    {
    }

    // Returns whether the Future is tracking an operation
    bool Valid() const;

    // Blocks the calling thread until the operation is finished
    void Wait();

    // Queries if the operation is finished (does not block)
    bool IsComplete() const;

private:
    DXFence* bound_fence {};
    uint64_t future_value {};
};