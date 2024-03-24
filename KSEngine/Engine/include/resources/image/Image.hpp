#pragma once
#include <vector>
#include <resources/image/ImageFormat.hpp>

namespace KSE {

//Image class for CPU usage
//Can be converted to GPU Image using TextureBuilder
class Image {
public:
	int width = 0, height = 0;
	std::vector<unsigned char> image_data;
	ImageFormat format;
};

}