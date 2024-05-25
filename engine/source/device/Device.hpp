#pragma once
#include <code_utility.hpp>
#include <memory>
#include <string>
#include <iostream>
#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"
#include <glm/glm.hpp>

namespace KS {

struct DeviceInitParams {
  std::string name = "KS Engine";
  uint32_t window_width = 1600;
  uint32_t window_height = 900;
  bool debug_context = true;
  glm::vec4 clear_color = glm::vec4(0.25f, 0.25f, 0.25f, 1.f);
};

class Device {
public:
  Device(const DeviceInitParams &params);
  ~Device();

  void *GetDevice();

  inline bool IsWindowOpen() const { return m_window_open; }
  void NewFrame();
  void EndFrame();

  NON_COPYABLE(Device);
  NON_MOVABLE(Device);

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
  bool m_window_open{};
  unsigned int m_frame_index = 0;
  bool m_fullscreen = false;
  int m_prev_width, m_prev_height;

  glm::vec4 m_clear_color;
};

} // namespace KS
