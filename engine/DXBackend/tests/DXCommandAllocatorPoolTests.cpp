
#include <gtest/gtest.h>

#include <commands/DXCommandAllocatorPool.hpp>
#include <initialization/DXFactory.hpp>

TEST(DXCommandAllocatorPool, RequestingAndDiscarding)
{
    DXFactory factory { true };
    auto feature_eval = [](CD3DX12FeatureSupport)
    { return true; };
    auto device = factory.CreateDevice(DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, feature_eval);

    auto command_allocator = DXCommandAllocatorPool();
    auto allocator = command_allocator.GetAllocator(device.Get());
    EXPECT_EQ(command_allocator.GetAllocatorCount(), 1ull);

    auto allocator2 = command_allocator.GetAllocator(device.Get());
    EXPECT_EQ(command_allocator.GetAllocatorCount(), 2ull);

    command_allocator.DiscardAllocator(allocator2, DXFuture {}, {});
    auto allocator3 = command_allocator.GetAllocator(device.Get());
    EXPECT_EQ(command_allocator.GetAllocatorCount(), 2ull);

    command_allocator.DiscardAllocator(allocator, DXFuture {}, {});
    command_allocator.DiscardAllocator(allocator3, DXFuture {}, {});
    command_allocator.FreeUnused();

    EXPECT_EQ(command_allocator.GetAllocatorCount(), 0ull);
}