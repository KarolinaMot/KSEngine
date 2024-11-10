#pragma once
#include <Common.hpp>
#include <initialization/DXFactory.hpp>
#include <rendering/DXSwapchain.hpp>

class Renderer
{
public:
    Renderer();
    ~Renderer() = default;

    NON_COPYABLE(Renderer);
    NON_MOVABLE(Renderer);
};