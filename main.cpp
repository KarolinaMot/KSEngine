#include <code_utility.hpp>
#include <device/Device.hpp>
#include <iostream>

int main() {

  KS::Device device = KS::Device(KS::DeviceInitParams{});

  std::string in;
  while (in != std::string("exit")) { std::cin >> in; }

  return 0;
}