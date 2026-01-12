#pragma once
#include <code_utility.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#pragma warning(push, 0)
#include <glm/glm.hpp>
#pragma warning(pop)
#include <renderer/RenderTarget.hpp>
#include <renderer/DepthStencil.hpp>
#include <resources/Texture.hpp>

class DXCommandList;
struct DXCommandContext;
namespace KS
{
class UploadArena;

struct DeviceInitParams
{
    std::string name = "KS Engine";
    uint32_t window_width = 1600;
    uint32_t window_height = 900;
    bool debug_context = true;
    glm::vec4 clear_color = glm::vec4(0.25f, 0.25f, 0.25f, 1.f);
};
class UploadArena;
class ShaderInputBlueprint;
class Shader;
class Device
{
public:
    Device(const DeviceInitParams& params);
    ~Device();

    void* GetDevice() const;
    DXCommandContext GetCommandContext() const;
    void* GetDepthHeap() const;
    void* GetRenderTargetHeap() const;
    void* GetImguiHeap() const;
    void* GetWindowHandle() const;
    
    inline bool IsWindowOpen() const { return m_window_open; }
    void NewFrame();
    void FlushAndWait();
    void EndFrame();
    void InitializeSwapchain();
    void FinishInitialization();
    void InitializeImGUI();
    void ResizeSwapchain(uint32_t newWidth, uint32_t newHeight);
    void SetVSync(bool value) { m_vSyncOn = value; }
    unsigned int GetFrameIndex() const { return m_gpu_frame; }
    unsigned int GetCPUFrameIndex() const { return m_cpu_frame; }

    int GetSwapchainWidth() const { return m_swapchainWidth; }
    int GetSwapchainHeight() const { return m_swapchainHeight; }
    int GetWindowWidth() const { return m_windowWidth; }
    int GetWindowHeight() const { return m_windowHeight; }
    bool GetVSync() const { return m_vSyncOn; }
    //RenderTarget* GetRenderTarget() { return m_swapchainRT.get(); };
    //DepthStencil* GetDepthStencil() { return m_swapchainDS.get(); };
    void CopyToSwapchainRT(DXCommandList& commandList, std::shared_ptr<RenderTarget> rt);
    UploadArena* GetUploadArena() const;

    // Blocks until all rendering operations are finished
    void Flush();

    NON_COPYABLE(Device);
    NON_MOVABLE(Device);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_window_open {};
    unsigned int m_gpu_frame = 0;
    unsigned int m_cpu_frame = 0;
    bool m_fullscreen = false;
    int m_swapchainWidth, m_swapchainHeight;
    int m_windowWidth, m_windowHeight;
    glm::vec4 m_clear_color;
    std::shared_ptr<RenderTarget> m_swapchainRT;
    std::shared_ptr<DepthStencil> m_swapchainDS;
    bool m_vSyncOn = true;
};

} // namespace KS
