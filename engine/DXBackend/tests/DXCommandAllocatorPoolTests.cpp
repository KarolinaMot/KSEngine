
#include <gtest/gtest.h>

#include <commands/DXCommandAllocatorPool.hpp>
#include <initialization/DXFactory.hpp>

TEST(DXCommandAllocatorPool, RequestingAndDiscarding)
{
    // Expect equality.
    EXPECT_EQ(7 * 6, 42);
}