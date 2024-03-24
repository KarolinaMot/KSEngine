#include <resources/image/ImageBuilder.hpp>
#include <resources/image/Image.hpp>

#include <unordered_map>

#include <utils/Log.hpp>
#include <utils/FileIO.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

const std::unordered_map<std::string, KSE::ImageFormat> IMAGE_EXTENSION_FORMATS {
    {".png", KSE::ImageFormat::RGBA_8 },
    {".jpg", KSE::ImageFormat::RGBA_8 }, //We tell stbi to convert 3 component jpegs to 4 component
    {".jpeg", KSE::ImageFormat::RGBA_8 },
    {".hdr", KSE::ImageFormat::RGBA_32F },
};


KSE::ResourceHandle<KSE::Image> KSE::ImageBuilder::FromPath(const std::filesystem::path& path)
{
    auto file_read = FileIO::read_file(path);

    auto format = IMAGE_EXTENSION_FORMATS.find(path.extension().string());

    if (format == IMAGE_EXTENSION_FORMATS.end()) {
        LOG(LogSeverity::WARN, "Image extension not recognized {}", path.string());
    }

    auto new_image = std::make_shared<Image>();
    auto props = ImageFormatProperties::FromEnum(format->second);

    int width, height, component;

    if (props.is_float) {
        auto stbi_result = stbi_loadf_from_memory(
            (stbi_uc*)(file_read.data()), static_cast<int>(file_read.size()), &width, &height, &component, props.components
        );

        if (stbi_result) {
            new_image->image_data = std::vector<uint8_t>((uint8_t*)(stbi_result), (uint8_t*)(stbi_result + width * height));
        }
        else {
            LOG(LogSeverity::WARN, "STBI load failed ({})", path.string());
        }
    }
    else {
        auto stbi_result = stbi_load_from_memory(
            (stbi_uc*)(file_read.data()), static_cast<int>(file_read.size()), &width, &height, &component, props.components
        );
        if (stbi_result) {
            new_image->image_data = std::vector<uint8_t>((uint8_t*)(stbi_result), (uint8_t*)(stbi_result + width * height));
        }
        else {
            LOG(LogSeverity::WARN, "STBI load failed ({})", path.string());
        }
    }

    new_image->format = format->second;
    new_image->height = height;
    new_image->width = width;

    return new_image;
}
