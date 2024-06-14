#pragma once

#include <fileio/FileIO.hpp>
#include <resources/Loader.hpp>
#include <string>

namespace KS {

class TextLoader : public KS::IResourceLoader {
public:
  using Resource = std::string;

  TextLoader(std::weak_ptr<KS::FileIO> fileIO) : m_fileIO(fileIO) {}
  virtual ~TextLoader() = default;

  virtual std::shared_ptr<void>
  LoadProcedure(KS::ResourceManager &manager,
                const std::string &path) override {
    if (auto lock = m_fileIO.lock()) {
      if (auto contents = lock->ReadTextFile(path))
        return std::make_shared<std::string>(std::move(contents.value()));
    }
    return nullptr;
  }

protected:
  std::weak_ptr<KS::FileIO> m_fileIO;
};

} // namespace KS