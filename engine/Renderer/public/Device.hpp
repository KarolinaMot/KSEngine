#pragma once

#include <Common.hpp>
#include <iostream>
#include <memory>
#include <renderer/DepthStencil.hpp>
#include <renderer/RenderTarget.hpp>
#include <resources/Texture.hpp>
#include <string>
#include <vector>


struct DeviceInitParams
{
    std::string name = "KS Engine";
    uint32_t window_width = 1600;
    uint32_t window_height = 900;
    bool debug_context = true;
    glm::vec4 clear_color = glm::vec4(0.25f, 0.25f, 0.25f, 1.f);
};

class Device
{
public:
    Device(const DeviceInitParams& params);
    ~Device();

    void NewFrame();
    void EndFrame();
    void InitializeSwapchain();
    void FinishInitialization();
    unsigned int GetFrameIndex() const { return m_frame_index; }
    unsigned int GetCPUFrameIndex() const { return m_cpu_frame; }

    void TrackResource(std::shared_ptr<void> buffer);

    std::shared_ptr<RenderTarget> GetRenderTarget() { return m_swapchainRT; };
    std::shared_ptr<Texture> GetRenderTargetTexture(int index) { return m_swapchainTex[index]; };
    std::shared_ptr<DepthStencil> GetDepthStencil() { return m_swapchainDS; };
    std::shared_ptr<Texture> GetDepthStencilTex() { return m_swapchainDepthTex; };

    NON_COPYABLE(Device);
    NON_MOVABLE(Device);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    unsigned int m_frame_index = 0;
    unsigned int m_cpu_frame = 0;

    glm::vec4 m_clear_color;
    std::shared_ptr<RenderTarget> m_swapchainRT;
    std::shared_ptr<Texture> m_swapchainTex[2];
    std::shared_ptr<DepthStencil> m_swapchainDS;
    std::shared_ptr<Texture> m_swapchainDepthTex;
};
