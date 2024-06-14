#include "Cache.hpp"

void KS::detail::ResourceCache::SetLoader(
    std::unique_ptr<IResourceLoader> &&loader) {
  m_loader = std::move(loader);
}

void KS::detail::ResourceCache::Reserve(const std::string &key) {
  auto &entry = m_storage[key];
  entry = std::make_unique<ResourceControl>();
}

void KS::detail::ResourceCache::Add(std::shared_ptr<void> res) {

  std::string gen_key = std::to_string(m_key_gen++);
  auto &entry = m_storage[gen_key];

  entry = std::make_unique<ResourceControl>();
  entry->resource = res;
}

std::shared_ptr<void> KS::detail::ResourceCache::Get(ResourceManager &manager,
                                                     const std::string &key) {
  auto &entry = m_storage[key];

  if (entry != nullptr && entry->resource != nullptr) {
    return entry->resource;
  }

  if (m_loader == nullptr)
    return nullptr;

  entry = std::make_unique<ResourceControl>();
  auto resource = m_loader->LoadProcedure(manager, key);
  entry->resource = resource;

  return resource;
}

void KS::detail::ResourceCache::Release(const std::string &key) {
  m_storage.erase(key);
}
