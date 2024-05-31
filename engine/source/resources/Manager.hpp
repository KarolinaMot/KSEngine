#pragma once

#include "Cache.hpp"
#include "Handle.hpp"
#include "Loader.hpp"
#include <typeindex>
#include <unordered_map>

namespace KS {

namespace detail {

template <typename T> std::type_index GetType() {
  return std::type_index(typeid(T *));
}

} // namespace detail

/// @brief Manages all file and runtime resources using Handle<T>
class ResourceManager {
public:
  ResourceManager() = default;
  ~ResourceManager() = default;

  NON_MOVABLE(ResourceManager);
  NON_COPYABLE(ResourceManager);

  // Reserves a handle to a file resource. This does not load the file.
  template <typename T> Handle<T> MakeHandle(const std::string &path);

  // Adds a new resource to the manager. Resources added this way must be
  // manually freed by the user and are immediately available after registering
  template <typename T> Handle<T> AddResource(std::shared_ptr<T> res);

  // Explicitly frees a resource. Resources created at runtime must be freed
  // this way, since they cannot be later retrieved if they are needed
  template <typename T> void Release(Handle<T> handle);

  // Releases all resources of a specific type. Only invalidates resources that
  // were created at runtime, file resources can still be recovered
  template <typename T> void ReleaseType();

  // Releases all resources in the manager.
  void ReleaseAll() { m_storage.clear(); }

  // Retrieves a shared_ptr to a resource based on the handle type. Will
  // return nullptr if
  // 1. The handle does not reference any file or path
  // 2. Loading has failed
  // 3. The resource is still in the process of loading.
  template <typename T> std::shared_ptr<T> Get(Handle<T> handle);

  // Binds a loader for loading resources from file assets. Necessary for types
  // that will be loaded from files.
  template <typename T>
  void BindLoader(std::unique_ptr<IResourceLoader> &&loader);

private:
  std::unordered_map<std::type_index, detail::ResourceCache> m_storage;
};

template <typename T>
inline Handle<T> ResourceManager::MakeHandle(const std::string &path) {
  m_storage[detail::GetType<T>()].Reserve(path);
  return Handle<T>(path);
}

template <typename T>
inline Handle<T> ResourceManager::AddResource(std::shared_ptr<T> res) {
  return m_storage[detail::GetType<T>()].Add(res);
}

template <typename T> inline void ResourceManager::Release(Handle<T> handle) {
  m_storage[detail::GetType<T>()].Release(handle.key);
}

template <typename T> inline void ResourceManager::ReleaseType() {
  m_storage[detail::GetType<T>()] = std::make_unique<detail::ResourceCache>();
}

template <typename T>
inline std::shared_ptr<T> ResourceManager::Get(Handle<T> handle) {
  return std::static_pointer_cast<T>(
      m_storage[detail::GetType<T>()].Get(*this, handle.key));
}

template <typename T>
inline void
ResourceManager::BindLoader(std::unique_ptr<IResourceLoader> &&loader) {
  m_storage[detail::GetType<T>()].SetLoader(std::move(loader));
}

} // namespace KS