#include "InputContext.hpp"
#include <tools/Log.hpp>

KS::InputContext& KS::InputContext::AddBinding(const std::string& action_name, RawInput::Code code, InputMapping::Operation conversion)
{
    mapping_context.emplace(code, std::make_pair(action_name, conversion));
    return *this;
}

std::queue<KS::InputData> KS::InputContext::ConvertInput(std::queue<RawInput::Data>&& input) const
{
    std::queue<InputData> out {};

    while (!input.empty())
    {
        auto raw_event = input.front();
        input.pop();

        auto& key = raw_event.first;
        auto& value = raw_event.second;

        if (auto it = mapping_context.find(key); it != mapping_context.end())
        {

            auto& [action_name, action_op] = it->second;
            if (auto processed_input = InputMapping::Map(action_op, value))
            {
                out.emplace(action_name, processed_input.value());
            }
        }
    }

    return out;
}