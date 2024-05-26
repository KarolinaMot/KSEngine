#include <code_utility.hpp>
#include <device/Device.hpp>
#include <fileio/FileIO.hpp>
#include <tools/Log.hpp>

int main()
{
  KS::Device device = KS::Device(KS::DeviceInitParams{});
  KS::FileIO filesystem = KS::FileIO();

  filesystem.WriteTextFile("text.txt", "Hello World");

  while (device.IsWindowOpen())
  {
    device.NewFrame();
    device.EndFrame();
  }

  return 0;
}