#pragma once
#include <filesystem>
#include <resources/ResourceHandle.hpp>

namespace KSE {

enum class ImageFormat;

class ImageBuilder {
public:
	
	ResourceHandle<Image> FromPath(const std::filesystem::path& path);

private:

};

}