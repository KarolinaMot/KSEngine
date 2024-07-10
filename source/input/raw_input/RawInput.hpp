#pragma once

#include "Codes.hpp"
#include <optional>
#include <variant>

namespace KS
{

struct RawInputCode
{
    template <typename E>
    RawInputCode(InputSource source, E input_enum)
        : source(source)
        , code(static_cast<uint32_t>(input_enum))
    {
    }

    InputSource source {};
    uint32_t code {};

    bool operator==(const RawInputCode& other) const
    {
        return source == other.source && code == other.code;
    }
};

class RawInputValue
{
public:
    std::optional<bool> GetState() const
    {
        if (std::holds_alternative<bool>(value))
        {
            return std::get<bool>(value);
        }
        return std::nullopt;
    }

    std::optional<float> GetRange() const
    {
        if (std::holds_alternative<float>(value))
        {
            return std::get<float>(value);
        }
        return std::nullopt;
    }

private:
    std::variant<bool, float> value;
};

}

namespace std
{

template <>
struct hash<KS::RawInputCode>
{
    size_t operator()(const KS::RawInputCode& k) const
    {
        return ((size_t)(k.source) << 32) | (size_t)(k.code);
    }
};
}