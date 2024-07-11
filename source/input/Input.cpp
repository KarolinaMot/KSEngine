#include "Input.hpp"

void KS::FrameInputResult::StartFrame()
{
    actions.clear();

    for (auto& [name, axis] : axis_cache)
    {
        if (axis.changed)
        {
            axis.changed = false;
            axis.prev = axis.current;
            axis.current = 0.0f;
        }
    }
}

void KS::FrameInputResult::Process(const InputData& event)
{
    auto& action = event.first;
    auto& value = event.second;

    switch (value.GetType())
    {
    case InputValue::Type::ACTION:
    {
        actions.emplace(action);
    }
    break;
    case InputValue::Type::STATE:
    {
        state_cache[action] = std::get<bool>(value.value);
    }
    break;
    case InputValue::Type::AXIS:
    {
        auto val = std::get<float>(value.value);

        if (auto it = axis_cache.find(action); it != axis_cache.end())
        {
            auto& axis = it->second;
            axis.changed = true;
            axis.current += val;
        }
        else
        {
            axis_cache.emplace(action, AxisCache { true, val, val });
        }
    }
    break;
    default:
        break;
    }
}

void KS::FrameInputResult::Reset()
{
    actions.clear();
    state_cache.clear();
    axis_cache.clear();
}

float KS::FrameInputResult::GetAxis(const std::string& action_name) const
{
    if (auto it = axis_cache.find(action_name); it != axis_cache.end())
    {
        return it->second.current;
    }
    return 0.0f;
}

float KS::FrameInputResult::GetAxisDelta(const std::string& action_name) const
{
    if (auto it = axis_cache.find(action_name); it != axis_cache.end())
    {
        if (it->second.changed)
            return it->second.current - it->second.prev;
    }
    return 0.0f;
}

bool KS::FrameInputResult::GetAction(const std::string& action_name) const
{
    if (actions.count(action_name))
        return true;
    return false;
}

bool KS::FrameInputResult::GetState(const std::string& action_name) const
{
    if (auto it = state_cache.find(action_name); it != state_cache.end())
    {
        return it->second;
    }
    return false;
}
