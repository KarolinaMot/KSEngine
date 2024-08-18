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
#include <glm/glm.hpp>

namespace KS
{

struct DeviceInitParams
{
    std::string name = "KS Engine";
    uint32_t window_width = 1600;
    uint32_t window_height = 900;
    bool debug_context = true;
    glm::vec4 clear_color = glm::vec4(0.25f, 0.25f, 0.25f, 1.f);
};

class Buffer;

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

    unsigned int GetFrameIndex() const { return m_frame_index; }
    int GetWidth() const { return m_prev_width; }
    int GetHeight() const { return m_prev_height; }
    void TrackResource(std::shared_ptr<void> buffer);

    // Blocks until all rendering operations are finished
    void Flush();

    NON_COPYABLE(Device);
    NON_MOVABLE(Device);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_window_open {};
    unsigned int m_frame_index = 0;
    bool m_fullscreen = false;
    int m_prev_width, m_prev_height;
    std::vector<std::shared_ptr<void>> m_frame_resources[2];
    glm::vec4 m_clear_color;
};

} // namespace KS
