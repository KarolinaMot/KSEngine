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
    std::unordered_map<KeyboardKey, InputState> keys;
    std::unordered_map<MouseButton, InputState> mouse_buttons;

    float mouseX {}, mouseY {};
    float deltaX {}, deltaY {};
    float mouseScrollX {}, mouseScrollY {};
};