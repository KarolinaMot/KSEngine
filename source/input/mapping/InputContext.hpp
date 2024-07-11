#pragma once

#include "Mapping.hpp"
#include "input/raw_input/RawInput.hpp"
#include <fileio/Serialization.hpp>
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
    friend class cereal::access;

    template <typename A>
    void save(A& ar, const uint32_t v) const;

    template <typename A>
    void load(A& ar, const uint32_t v);

    using ActionMappingPair = std::pair<std::string, InputMapping::Operation>;
    std::map<RawInput::Code, ActionMappingPair> mapping_context;
};

template <typename A>
inline void InputContext::save(A& ar, const uint32_t v) const
{
    switch (v)
    {
    case 0:
        ar(cereal::make_nvp("MappingContext", mapping_context));
        break;

    default:
        break;
    }
}

template <typename A>
inline void InputContext::load(A& ar, const uint32_t v)
{
    switch (v)
    {
    case 0:
        ar(cereal::make_nvp("MappingContext", mapping_context));
        break;

    default:
        break;
    }
}
}

CEREAL_CLASS_VERSION(KS::InputContext, 0)