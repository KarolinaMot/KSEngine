#pragma once
#include "Mapping.hpp"
#include "input/raw_input/RawInput.hpp"
#include <map>
#include <queue>

namespace KS
{

class InputContext
{
public:
    InputContext& AddBinding(const std::string& action_name, RawInput::Code code, InputMapping::Operation conversion);

    std::queue<InputData> ConvertInput(std::queue<RawInput::Data>&& input) const;

private:
    using ActionMappingPair = std::pair<std::string, InputMapping::Operation>;
    std::map<RawInput::Code, ActionMappingPair> mapping_context;
};

}