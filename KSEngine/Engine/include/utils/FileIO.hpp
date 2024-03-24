#pragma once
#include <vector>
#include <filesystem>

namespace KSE::FileIO {

std::vector<char> read_file(const std::filesystem::path& path);



}