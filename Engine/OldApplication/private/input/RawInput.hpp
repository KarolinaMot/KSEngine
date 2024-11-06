#pragma once
#include "Device.hpp"
#include "InputCodes.hpp"
#include <memory>

/// @brief TODO: only supports keyboard and is PC only
class RawInput
{
public:
    RawInput(std::shared_ptr<Device> device);
    ~RawInput();

    void ProcessInput();

    InputState GetKeyboard(KeyboardKey key) const;
    InputState GetMouseButton(MouseButton button) const;
    std::pair<float, float> GetMousePos() const;
    std::pair<float, float> GetMouseDelta() const;

    NON_COPYABLE(RawInput);
    NON_MOVABLE(RawInput);

    class Impl; // public because of user pointers
private:
    std::unique_ptr<Impl> m_Impl;

    std::weak_ptr<Device> m_Device;
};