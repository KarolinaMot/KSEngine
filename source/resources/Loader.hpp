#pragma once

#include <code_utility.hpp>
#include <memory>
#include <string>


namespace KS {

class IResourceLoader {
public:
  virtual ~IResourceLoader() = default;

  virtual std::shared_ptr<void> LoadProcedure(class ResourceManager &manager,
                                              const std::string &path) = 0;

  NON_COPYABLE(IResourceLoader);
  NON_MOVABLE(IResourceLoader);

protected:
  IResourceLoader() = default;
};

} // namespace KS