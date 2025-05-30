#include "FileIO.hpp"
#include <filesystem>

std::optional<std::ifstream> KS::FileIO::OpenReadStream(const Path& path,
    int flags)
{
    if (Exists(path))
    {
        return std::ifstream(path, flags);
    }
    else
    {
        return std::nullopt;
    }
}

std::optional<std::ofstream>
KS::FileIO::OpenWriteStream(const Path& path, int flags)
{
    std::ofstream stream(path, flags);
    if (stream.is_open())
    {
        return stream;
    }
    else
    {
        return std::nullopt;
    }
}

bool KS::FileIO::Exists(const Path& path)
{
    return std::filesystem::exists(path);
}

bool KS::FileIO::MakeDirectory(const Path& path)
{
    std::error_code e {};
    std::filesystem::create_directory(path, e);
    if (e == std::error_code {})
        return true;
    else
        return false;
}

std::optional<KS::FileIO::FileTime>
KS::FileIO::GetLastModifiedTime(const Path& path)
{
    if (Exists(path))
    {
        return std::filesystem::last_write_time(path);
    }
    else
    {
        return std::nullopt;
    }
}

std::vector<char> KS::FileIO::DumpFullStream(std::istream& stream)
{
    stream.seekg(0, std::ios::end);
    size_t size = stream.tellg();
    std::vector<char> out(size, '\0');
    stream.seekg(0);
    stream.read(out.data(), size);
    return out;
}
