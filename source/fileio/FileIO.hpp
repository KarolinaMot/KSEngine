#pragma once

#include <chrono>
#include <code_utility.hpp>
#include <fstream>
#include <optional>
#include <vector>

namespace KS {

// TODO: currently just a class with functions, can be used later for platform
// dependent file stuff
/// @brief File interface system for opening and closing files
class FileIO {
public:
  using FileTime = std::chrono::time_point<std::chrono::file_clock>;

  FileIO() = default;
  ~FileIO() = default;

  NON_COPYABLE(FileIO);
  NON_MOVABLE(FileIO);

  /// <summary>
  /// Open a file stream for reading. Specify 0 or std::ios::flags
  /// </summary>
  std::optional<std::ifstream> OpenReadStream(const std::string &path,
                                              int flags);

  /// <summary>
  /// Open a file stream for writing. Specify 0 or std::ios::flags
  /// </summary>
  std::optional<std::ofstream> OpenWriteStream(const std::string &path,
                                               int flags);

  /// <summary>
  /// Read a text file into a string. Nullopt if file does not exist
  /// </summary>
  std::optional<std::string> ReadTextFile(const std::string &path);

  /// <summary>
  /// Read a binary file into a string. Nullopt if file does not exist
  /// </summary>
  std::optional<std::vector<char>> ReadBinaryFile(const std::string &path);

  /// <summary>
  /// Write a string to a text file. The file is created if it does not exist.
  /// Returns true if the file was written successfully.
  /// </summary>
  bool WriteTextFile(const std::string &path, const std::string &content);

  /// <summary>
  /// Write a string to a binary file. The file is created if it does not exist.
  /// Returns true if the file was written successfully.
  /// </summary>
  bool WriteBinaryFile(const std::string &path,
                       const std::vector<char> &content);

  /// <summary>
  /// Check if a file exists.
  /// </summary>
  bool Exists(const std::string &path);

  /// <summary>
  /// Check the last time a file was modified. Nullopt if file doesn't exist
  /// </summary>
  std::optional<FileTime> GetLastModifiedTime(const std::string &path);
};

} // namespace KS