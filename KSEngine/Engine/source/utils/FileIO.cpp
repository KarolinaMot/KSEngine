#include <utils/FileIO.hpp>
#include <fstream>
#include <utils/Log.hpp>

std::vector<char> KSE::FileIO::read_file(const std::filesystem::path& path)
{
	if (!std::filesystem::exists(path)) {
		LOG(LogSeverity::WARN, "File path does not exist at {}", path.string());
		return {};
	}

	auto file_stream = std::ifstream(
		path, std::ios::in | std::ios::binary | std::ios::ate
	);

	if (!file_stream.is_open()) {
		LOG(LogSeverity::WARN, "Error opening file stream at {}", path.string());
		return {};
	}
	const auto size = file_stream.tellg();

	std::vector<char> bytes; bytes.resize(size);
	file_stream.seekg(0, std::ios::beg);

	file_stream.read(bytes.data(), size);
	return bytes;
}
