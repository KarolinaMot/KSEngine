#include <code_utility.hpp>
#include <memory>
#include <string>

namespace KS {

struct DeviceInitParams {
  std::string name = "KS Engine";
  uint32_t window_width = 1600;
  uint32_t window_height = 900;
  bool debug_context = false;
};

class Device {
public:
  Device(DeviceInitParams params);
  ~Device();

  NON_COPYABLE(Device);
  NON_MOVABLE(Device);

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace KS
