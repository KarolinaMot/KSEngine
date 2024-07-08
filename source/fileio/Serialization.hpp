#pragma once
#include <cereal/access.hpp>
#include <cereal/cereal.hpp>

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>

#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/vector.hpp>

#include <glm/gtc/quaternion.hpp>
#include <glm/mat2x2.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace KS
{

using JSONSaver = cereal::JSONOutputArchive;
using JSONLoader = cereal::JSONInputArchive;
using BinarySaver = cereal::BinaryOutputArchive;
using BinaryLoader = cereal::BinaryOutputArchive;

template <typename T>
struct ResourceHandle
{
    std::string id;

    template <typename A>
    void serialize(A& a) { a(id); }
};
}

// Serialization functions for GLM objects
namespace glm
{

template <class Archive>
void serialize(Archive& archive, glm::vec2& v) { archive(v.x, v.y); }
template <class Archive>
void serialize(Archive& archive, glm::vec3& v) { archive(v.x, v.y, v.z); }
template <class Archive>
void serialize(Archive& archive, glm::vec4& v) { archive(v.x, v.y, v.z, v.w); }
template <class Archive>
void serialize(Archive& archive, glm::ivec2& v) { archive(v.x, v.y); }
template <class Archive>
void serialize(Archive& archive, glm::ivec3& v) { archive(v.x, v.y, v.z); }
template <class Archive>
void serialize(Archive& archive, glm::ivec4& v) { archive(v.x, v.y, v.z, v.w); }
template <class Archive>
void serialize(Archive& archive, glm::uvec2& v) { archive(v.x, v.y); }
template <class Archive>
void serialize(Archive& archive, glm::uvec3& v) { archive(v.x, v.y, v.z); }
template <class Archive>
void serialize(Archive& archive, glm::uvec4& v) { archive(v.x, v.y, v.z, v.w); }
template <class Archive>
void serialize(Archive& archive, glm::dvec2& v) { archive(v.x, v.y); }
template <class Archive>
void serialize(Archive& archive, glm::dvec3& v) { archive(v.x, v.y, v.z); }
template <class Archive>
void serialize(Archive& archive, glm::dvec4& v) { archive(v.x, v.y, v.z, v.w); }

// glm matrices serialization
template <class Archive>
void serialize(Archive& archive, glm::mat2& m) { archive(m[0], m[1]); }
template <class Archive>
void serialize(Archive& archive, glm::dmat2& m) { archive(m[0], m[1]); }
template <class Archive>
void serialize(Archive& archive, glm::mat3& m) { archive(m[0], m[1], m[2]); }
template <class Archive>
void serialize(Archive& archive, glm::mat4& m) { archive(m[0], m[1], m[2], m[3]); }
template <class Archive>
void serialize(Archive& archive, glm::dmat4& m) { archive(m[0], m[1], m[2], m[3]); }

template <class Archive>
void serialize(Archive& archive, glm::quat& q) { archive(q.x, q.y, q.z, q.w); }
template <class Archive>
void serialize(Archive& archive, glm::dquat& q) { archive(q.x, q.y, q.z, q.w); }
}