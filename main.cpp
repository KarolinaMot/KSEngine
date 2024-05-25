#include <code_utility.hpp>
#include <device/Device.hpp>
#include <iostream>
#include <tools/Log.hpp>

int main()
{
  KS::Device device = KS::Device(KS::DeviceInitParams{});

  while (device.IsWindowOpen())
  {
    device.NewFrame();
    device.EndFrame();
  }

  return 0;
}