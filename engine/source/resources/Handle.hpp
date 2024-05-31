#pragma once

namespace KS {

template <typename T> class Handle {
  friend class ResourceManager;

  Handle(const std::string &key) : key(key) {}
  std::string key;
};

} // namespace KS