#pragma once

// Delete Macros

#define NON_COPYABLE(name)                                                     \
  name(const name &other) = delete;                                            \
  name &operator=(const name &other) = delete

#define NON_MOVABLE(name)                                                      \
  name(name &&other) = delete;                                                 \
  name &operator=(name &&other) = delete

// Debug macros
#if not defined(NDEBUG)

#include <cassert>
#define ASSERT(expr) assert(expr)
#define DEBUG_ONLY(expr) expr

#else

#define ASSERT(expr)     // empty
#define DEBUG_ONLY(expr) // empty

#endif

// Very useful templates

namespace KS
{
template <class... Ts>
struct Overload : Ts...
{
    using Ts::operator()...;
};
}