#pragma once
#include <resources/ResourceHandle.hpp>
#include <array>

namespace KSE {

enum class VertexAttributes : uint32_t {
	POSITION,
	NORMAL,
	TANGENT,
	TEXTURE_0,
	MAX_ATTRIBUTES
};

class Mesh {
public:
	static constexpr size_t MAX_VERTEX_ATTRIBUTES = static_cast<size_t>(VertexAttributes::MAX_ATTRIBUTES);

	std::array<ResourceHandle<Buffer>, MAX_VERTEX_ATTRIBUTES> vertex_attributes;
	ResourceHandle<Buffer> index_buffer;

	//TODO: Add more members for Index type, etc
};

}