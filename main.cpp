#include <code_utility.hpp>
#include <device/Device.hpp>
#include <iostream>
#include <tools/Log.hpp>

int main() {

  KS::Device device = KS::Device(KS::DeviceInitParams{});

  std::string in;
  while (in != std::string("exit")) {
    LOG(Log::Severity::ERROR, "Error here");
    std::cin >> in;
  }

  return 0;
}