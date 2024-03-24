#pragma once
#include <resources/ResourceHandle.hpp>
#include <glm/vec4.hpp>
#include <array>

namespace KSE {

enum class MaterialTextures {
	ALBEDO,
	NORMAL,
	METALLIC_ROUGHNESS,
	EMISSIVE,
	OCCLUSION,
	MAX_MATERIAL_TEXTURES
};

class Material {
public:
	static constexpr size_t MAX_TEXTURES = static_cast<size_t>(MaterialTextures::MAX_MATERIAL_TEXTURES);

	std::array<ResourceHandle<Texture>, MAX_TEXTURES> textures;
	std::array<glm::vec4, MAX_TEXTURES> multipliers;

};



}