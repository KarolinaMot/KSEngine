#pragma once

#include "Codes.hpp"
#include <fileio/Serialization.hpp>
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <string_view>
#include <variant>

namespace KS::RawInput
{

struct Code
{
    Code() = default;

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

    template <typename A>
    void save(A& ar) const;

    template <typename A>
    void load(A& ar);
};

enum class State
{
    ACTIVE,
    PRESS_DOWN,
    PRESS_UP
};

using Value = std::variant<State, float>;
using Data = std::pair<Code, Value>;

template <typename A>
inline void Code::save(A& ar) const
{
    std::string source_name { magic_enum::enum_name(source) };
    ar(cereal::make_nvp("Source", source_name));

    switch (source)
    {
    case Source::KEYBOARD:
    {
        if (auto key = magic_enum::enum_cast<KeyboardKey>(code))
        {
            std::string name { magic_enum::enum_name(key.value()) };
            ar(cereal::make_nvp("Code", name));
        }
    }
    break;
    case Source::MOUSE_BUTTONS:
    {
        if (auto key = magic_enum::enum_cast<MouseButton>(code))
        {
            std::string name { magic_enum::enum_name(key.value()) };
            ar(cereal::make_nvp("Code", name));
        }
    }
    break;
    case Source::MOUSE_POSITION:
    {
        if (auto key = magic_enum::enum_cast<Direction>(code))
        {
            std::string name { magic_enum::enum_name(key.value()) };
            ar(cereal::make_nvp("Code", name));
        }
    }
    break;
    case Source::MOUSE_SCROLL:
    {
        if (auto key = magic_enum::enum_cast<Direction>(code))
        {
            std::string name { magic_enum::enum_name(key.value()) };
            ar(cereal::make_nvp("Code", name));
        }
    }
    break;
    default:
        break;
    }
}

template <typename A>
inline void Code::load(A& ar)
{
    std::string source_name {};
    ar(cereal::make_nvp("Source", source_name));

    if (auto s = magic_enum::enum_cast<Source>(source_name))
    {
        source = s.value();

        std::string code_name {};
        ar(cereal::make_nvp("Code", code_name));

        switch (source)
        {
        case Source::KEYBOARD:
        {
            code = static_cast<uint32_t>(magic_enum::enum_cast<KeyboardKey>(code_name).value());
        }
        break;
        case Source::MOUSE_BUTTONS:
        {
            code = static_cast<uint32_t>(magic_enum::enum_cast<MouseButton>(code_name).value());
        }
        break;
        case Source::MOUSE_POSITION:
        {
            code = static_cast<uint32_t>(magic_enum::enum_cast<Direction>(code_name).value());
        }
        break;
        case Source::MOUSE_SCROLL:
        {
            code = static_cast<uint32_t>(magic_enum::enum_cast<Direction>(code_name).value());
        }
        break;
        default:
            break;
        }
    }
}
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