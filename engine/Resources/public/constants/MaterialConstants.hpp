#pragma once

#include <Common.hpp>
#include <glm/vec4.hpp>

namespace MaterialConstants
{

// BASE COLOUR
constexpr auto BASE_COLOUR_FACTOR_NAME = "BASE_COLOUR";
constexpr glm::vec4 BASE_COLOUR_FACTOR_DEFAULT = { 1.0f, 1.0f, 1.0f, 1.0f };

// NORMAL, EMISSIVE, ALPHA CUTOFF
constexpr auto NEA_FACTORS_NAME = "NEA_FACTOR";
constexpr glm::vec4 NEA_FACTORS_DEFAULT = { 1.0f, 1.0f, 0.0f, 0.0f };

// OCCLUSION, ROUGHNESS, METALLIC
constexpr auto ORM_FACTORS_NAME = "ORM_FACTORS";
constexpr glm::vec4 ORM_FACTORS_DEFAULT = { 1.0f, 0.5f, 0.5f, 0.0f };

constexpr auto DOUBLE_SIDED_FLAG_NAME = "DOUBLE_SIDED";
constexpr bool DOUBLE_SIDED_DEFAULT = false;

constexpr auto BASE_TEXTURE_NAME = "BASE_TEXTURE";
constexpr auto NORMAL_TEXTURE_NAME = "NORMAL_TEXTURE";
constexpr auto OCCLUSION_TEXTURE_NAME = "OCCLUSION_TEXTURE";
constexpr auto METALLIC_TEXTURE_NAME = "METALLIC_TEXTURE";
constexpr auto EMISSIVE_TEXTURE_NAME = "EMISSIVE_TEXTURE";

}
