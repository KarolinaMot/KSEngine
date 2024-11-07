#include <cassert>
#include <sync/DXFuture.hpp>

bool DXFuture::Valid() const
{
    return bound_fence != nullptr;
}

void DXFuture::Wait()
{
    if (Valid())
        bound_fence->WaitFor(future_value);
}

bool DXFuture::IsComplete() const
{
    if (Valid())
        return bound_fence->Reached(future_value);
    return true;
}