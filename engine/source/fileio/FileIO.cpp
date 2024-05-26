#include "FileIO.hpp"
#include <filesystem>

std::optional<std::ifstream> KS::FileIO::OpenReadStream(const std::string &path,
                                                        int flags) {
  if (Exists(path)) {
    return std::ifstream(path, flags);
  } else {
    return std::nullopt;
  }
}

std::optional<std::ofstream>
KS::FileIO::OpenWriteStream(const std::string &path, int flags) {
  std::ofstream stream(path, flags);
  if (stream.is_open()) {
    return stream;
  } else {
    return std::nullopt;
  }
}

std::optional<std::string> KS::FileIO::ReadTextFile(const std::string &path) {
  if (auto stream = OpenReadStream(path, std::ios::ate)) {
    stream.value().seekg(0, std::ios::end);
    size_t size = stream.value().tellg();
    std::string out(size, '\0');
    stream.value().seekg(0);
    stream.value().read(out.data(), size);
    return out;
  } else {
    return std::nullopt;
  }
}

std::optional<std::vector<char>>
KS::FileIO::ReadBinaryFile(const std::string &path) {
  if (auto stream = OpenReadStream(path, std::ios::ate | std::ios::binary)) {
    stream.value().seekg(0, std::ios::end);
    size_t size = stream.value().tellg();
    std::vector<char> out(size, '\0');
    stream.value().seekg(0);
    stream.value().read(out.data(), size);
    return out;
  } else {
    return std::nullopt;
  }
}

bool KS::FileIO::WriteTextFile(const std::string &path,
                               const std::string &content) {

  if (auto stream = OpenWriteStream(path, std::ios::trunc)) {
    stream.value() << content;
    return true;
  } else {
    return false;
  }
}

bool KS::FileIO::WriteBinaryFile(const std::string &path,
                                 const std::vector<char> &content) {
  if (auto stream = OpenWriteStream(path, std::ios::trunc | std::ios::binary)) {
    stream.value().write(content.data(), content.size());
    return true;
  } else {
    return false;
  }
}

bool KS::FileIO::Exists(const std::string &path) {
  return std::filesystem::exists(path);
}

std::optional<KS::FileIO::FileTime>
KS::FileIO::GetLastModifiedTime(const std::string &path) {
  if (Exists(path)) {
    return std::filesystem::last_write_time(path);
  } else {
    return std::nullopt;
  }
}
