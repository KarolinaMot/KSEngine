#pragma once
#include "RawInput.hpp"
#include <queue>
#include <unordered_map>

namespace KS
{

class Device;

/// @brief TODO: only supports keyboard and is PC only
class RawInputCollector
{
public:
    RawInputCollector(const Device& device);

    void AddRawInput(RawInput::Data input);
    std::queue<RawInput::Data> ProcessInput();

private:
    std::queue<RawInput::Data> collected {};
    std::unordered_map<RawInput::Code, bool> press_cache {};
};
}