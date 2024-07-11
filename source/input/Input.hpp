#pragma once
#include "mapping/InputData.hpp"
#include <optional>
#include <unordered_map>
#include <unordered_set>


namespace KS
{

class FrameInputResult
{
public:
    void StartFrame();
    void Process(const InputData& event);

    void Reset();

    // Returns 0.0f on invalid names
    float GetAxis(const std::string& action_name) const;

    // Returns 0.0f on invalid names
    float GetAxisDelta(const std::string& action_name) const;

    // Returns false when the action is not triggered
    bool GetAction(const std::string& action_name) const;

    // Returns false on invalid names that are not mapped to a state
    bool GetState(const std::string& action_name) const;

private:
    std::unordered_set<std::string> actions {};
    std::unordered_map<std::string, bool> state_cache;

    struct AxisCache
    {
        bool changed = true;
        float current {};
        float prev {};
    };

    std::unordered_map<std::string, AxisCache> axis_cache;
};

}