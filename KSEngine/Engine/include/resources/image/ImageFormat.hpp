#pragma once

namespace KSE {

enum class ImageFormat {
	RGBA_8,
	RGBA_32F
};

struct ImageFormatProperties {
	static ImageFormatProperties FromEnum(ImageFormat format);

	bool is_float = false;
	int components = 0;
};

}