#include <resources/image/ImageFormat.hpp>

KSE::ImageFormatProperties KSE::ImageFormatProperties::FromEnum(ImageFormat format)
{
	switch (format)
	{
	case KSE::ImageFormat::RGBA_8:
		return { false, 4 };
		break;
	case KSE::ImageFormat::RGBA_32F:
		return { true, 4 };
		break;
	default:
		return {};
		break;
	}
}
