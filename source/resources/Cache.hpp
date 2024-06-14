#pragma once

#include "Loader.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace KS {
class ResourceManager;
}

namespace KS::detail {

struct ResourceControl {
  std::shared_ptr<void> resource;
};

class ResourceCache {
public:
  ResourceCache() = default;
  ~ResourceCache() = default;

  void SetLoader(std::unique_ptr<IResourceLoader> &&loader);

  void Reserve(const std::string &key);
  void Add(std::shared_ptr<void> res);

  std::shared_ptr<void> Get(KS::ResourceManager &manager,
                            const std::string &key);

  void Release(const std::string &key);

private:
  uint32_t m_key_gen = 0;
  std::unique_ptr<IResourceLoader> m_loader = nullptr;
  std::unordered_map<std::string, std::unique_ptr<ResourceControl>> m_storage;
};

} // namespace KS::detail