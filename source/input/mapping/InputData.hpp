#pragma once
#include <string>
#include <variant>

namespace KS
{

class InputValue
{
public:
    enum class Type
    {
        ACTION,
        STATE,
        AXIS
    };

    // Monostate / action input
    InputValue() = default;

    // Bool / State Input
    InputValue(bool val)
        : value(val)
    {
    }

    // Float / Range Input
    InputValue(float val)
        : value(val)
    {
    }

    Type GetType() const
    {
        if (std::holds_alternative<std::monostate>(value))
            return Type::ACTION;
        else if (std::holds_alternative<bool>(value))
            return Type::STATE;
        else
            return Type::AXIS;
    }

    std::variant<std::monostate, bool, float> value;
};

using InputData = std::pair<std::string, InputValue>;

}