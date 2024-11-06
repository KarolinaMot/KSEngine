#pragma once

#include <Common.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#include <glfw3.h>
#include <glfw3native.h>

#include <renderer/DepthStencil.hpp>
#include <renderer/RenderTarget.hpp>
#include <resources/Texture.hpp>

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

    void* GetDevice() const;
    void* GetCommandList() const;
    void* GetResourceHeap() const;
    void* GetDepthHeap() const;
    void* GetRenderTargetHeap() const;
    void* GetWindowHandle() const;

    inline bool IsWindowOpen() const { return m_window_open; }
    void NewFrame();
    void EndFrame();
    void InitializeSwapchain();
    void FinishInitialization();
    unsigned int GetFrameIndex() const { return m_frame_index; }
    unsigned int GetCPUFrameIndex() const { return m_cpu_frame; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    void TrackResource(std::shared_ptr<void> buffer);
    std::shared_ptr<RenderTarget> GetRenderTarget() { return m_swapchainRT; };
    std::shared_ptr<Texture> GetRenderTargetTexture(int index) { return m_swapchainTex[index]; };
    std::shared_ptr<DepthStencil> GetDepthStencil() { return m_swapchainDS; };
    std::shared_ptr<Texture> GetDepthStencilTex() { return m_swapchainDepthTex; };
    // Blocks until all rendering operations are finished
    void Flush();

    NON_COPYABLE(Device);
    NON_MOVABLE(Device);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_window_open {};
    unsigned int m_frame_index = 0;
    unsigned int m_cpu_frame = 0;
    bool m_fullscreen = false;
    int m_width, m_height;
    glm::vec4 m_clear_color;
    std::shared_ptr<RenderTarget> m_swapchainRT;
    std::shared_ptr<Texture> m_swapchainTex[2];
    std::shared_ptr<DepthStencil> m_swapchainDS;
    std::shared_ptr<Texture> m_swapchainDepthTex;
};
