#pragma once

// Class Decorations

#define NON_COPYABLE(ClassName)           \
    ClassName(const ClassName&) = delete; \
    ClassName& operator=(const ClassName&) = delete;

#define NON_MOVABLE(ClassName)       \
    ClassName(ClassName&&) = delete; \
    ClassName& operator=(ClassName&&) = delete;

#define DEFAULT_COPYABLE(ClassName)        \
    ClassName(const ClassName&) = default; \
    ClassName& operator=(const ClassName&) = default;

#define DEFAULT_MOVEABLE(ClassName)   \
    ClassName(ClassName&&) = default; \
    ClassName& operator=(ClassName&&) = default;

// Attribute macros

#define MAYBE_UNUSED [[maybe_unused]]
#define NO_DISCARD [[nodiscard]]

#include <cstddef>

void* operator new(std::size_t count);
void operator delete(void* ptr) noexcept;