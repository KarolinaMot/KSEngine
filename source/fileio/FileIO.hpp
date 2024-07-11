#pragma once

#include <chrono>
#include <code_utility.hpp>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

namespace KS
{

/// @brief File interface system for opening and closing files
namespace FileIO
{
    constexpr int DEFAULT_READ_FLAGS = std::ios::in | std::ios::binary;
    constexpr int DEFAULT_WRITE_FLAGS = std::ios::out | std::ios::trunc | std::ios::binary;

    using FileTime = std::chrono::time_point<std::chrono::file_clock>;
    using Path = std::filesystem::path;

    /// <summary>
    /// Open a file stream for reading. Specify 0 or std::ios::flags
    /// </summary>
    std::optional<std::ifstream> OpenReadStream(const Path& path,
        int flags = DEFAULT_READ_FLAGS);

    /// <summary>
    /// Open a file stream for writing. Specify 0 or std::ios::flags
    /// </summary>
    std::optional<std::ofstream> OpenWriteStream(const Path& path,
        int flags = DEFAULT_WRITE_FLAGS);

    /// <summary>
    /// Dumps all bytes of a stream into a vector
    /// </summary>
    std::vector<char> DumpFullStream(std::istream& stream);

    /// <summary>
    /// Check if a file exists.
    /// </summary>
    bool Exists(const Path& path);

    /// <summary>
    /// Check if a file exists.
    /// </summary>
    bool MakeDirectory(const Path& path);

    /// <summary>
    /// Check the last time a file was modified. Nullopt if file doesn't exist
    /// </summary>
    std::optional<FileTime> GetLastModifiedTime(const Path& path);
};

} // namespace KS