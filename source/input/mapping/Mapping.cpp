#include "Mapping.hpp"
#include <code_utility.hpp>

std::optional<KS::InputValue> KS::InputMapping::ToAction::operator()(const RawInput::Value& val) const
{
    auto visitor = Overload {
        [this](RawInput::State b) -> std::optional<KS::InputValue>
        {
            if (b == RawInput::State::PRESS_DOWN && on_down)
            {
                return InputValue();
            }
            else if (b == RawInput::State::PRESS_UP && !on_down)
            {
                return InputValue();
            }
            return std::nullopt;
        },
        [](float f) -> std::optional<KS::InputValue>
        {
            return InputValue();
        }
    };

    return std::visit(visitor, val);
}

std::optional<KS::InputValue> KS::InputMapping::ToState::operator()(const RawInput::Value& val) const
{
    auto visitor = Overload {
        [](RawInput::State b) -> std::optional<KS::InputValue>
        {
            switch (b)
            {
            case RawInput::State::PRESS_DOWN:
                return InputValue(true);
            case RawInput::State::PRESS_UP:
                return InputValue(false);
            default:
                return std::nullopt;
            }
        },
        [](float f) -> std::optional<KS::InputValue>
        {
            return std::nullopt;
        }
    };

    return std::visit(visitor, val);
}

std::optional<KS::InputValue> KS::InputMapping::ToAxis::operator()(const RawInput::Value& val) const
{
    auto visitor = Overload {
        [this](RawInput::State b) -> std::optional<KS::InputValue>
        {
            if (b == RawInput::State::ACTIVE)
            {
                return InputValue(scale);
            }
            return InputValue(0.0f);
        },
        [this](float f) -> std::optional<KS::InputValue>
        {
            return InputValue(f * scale);
        }
    };

    return std::visit(visitor, val);
}

std::optional<KS::InputValue> KS::InputMapping::Map(const Operation& op, const RawInput::Value& val)
{
    auto visitor = [](const auto& op, const RawInput::Value& val)
    { return op(val); };

    return std::visit(visitor, op, val);
}
