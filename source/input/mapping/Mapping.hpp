#pragma once
#include "InputData.hpp"
#include <input/raw_input/RawInput.hpp>

namespace KS::InputMapping
{

struct ToAction
{
    std::optional<InputValue> operator()(const RawInput::Value& val) const;
    bool on_down = true;
};

struct ToState
{
    std::optional<InputValue> operator()(const RawInput::Value& val) const;
};

struct ToAxis
{
    std::optional<InputValue> operator()(const RawInput::Value& val) const;
    float scale = 1.0f;
};

using Operation = std::variant<ToState, ToAction, ToAxis>;
std::optional<KS::InputValue> Map(const Operation& op, const RawInput::Value& val);
}
