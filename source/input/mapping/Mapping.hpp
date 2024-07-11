#pragma once
#include "InputData.hpp"
#include <fileio/Serialization.hpp>
#include <input/raw_input/RawInput.hpp>

namespace KS::InputMapping
{

struct ToAction
{
    std::optional<InputValue> operator()(const RawInput::Value& val) const;
    bool on_down = true;

    template <typename A>
    void serialize(A& ar)
    {
        ar(cereal::make_nvp("ActionOnDown", on_down));
    }
};

struct ToState
{
    std::optional<InputValue> operator()(const RawInput::Value& val) const;

    template <typename A>
    void serialize(A& ar)
    {
        // ar("ToState");
    }
};

struct ToAxis
{
    std::optional<InputValue> operator()(const RawInput::Value& val) const;
    float scale = 1.0f;

    template <typename A>
    void serialize(A& ar)
    {
        ar(cereal::make_nvp("AxisScale", scale));
    }
};

using Operation = std::variant<ToState, ToAction, ToAxis>;
std::optional<KS::InputValue> Map(const Operation& op, const RawInput::Value& val);
}
