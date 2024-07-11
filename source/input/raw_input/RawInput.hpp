#pragma once

#include "Codes.hpp"
#include <optional>
#include <variant>

namespace KS::RawInput
{

struct Code
{
    template <typename E>
    Code(RawInput::Source source, E input_enum)
        : source(source)
        , code(static_cast<uint32_t>(input_enum))
    {
    }

    RawInput::Source source {};
    uint32_t code {};

    bool operator==(const Code& other) const
    {
        return source == other.source && code == other.code;
    }

    bool operator<(const Code& other) const
    {
        if (source == other.source)
        {
            return static_cast<uint32_t>(code) < static_cast<uint32_t>(other.code);
        }

        return static_cast<uint32_t>(source) < static_cast<uint32_t>(other.source);
    }
};

enum class State
{
    ACTIVE,
    PRESS_DOWN,
    PRESS_UP
};

using Value = std::variant<State, float>;
using Data = std::pair<Code, Value>;
}

namespace std
{

template <>
struct hash<KS::RawInput::Code>
{
    size_t operator()(const KS::RawInput::Code& k) const
    {
        return ((size_t)(k.source) << 32) | (size_t)(k.code);
    }
};
}