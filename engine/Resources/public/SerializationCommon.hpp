#include <Common.hpp>

#include <cereal/cereal.hpp>

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>

using JSONSaver = cereal::JSONOutputArchive;
using JSONLoader = cereal::JSONInputArchive;
using BinarySaver = cereal::BinaryOutputArchive;
using BinaryLoader = cereal::BinaryInputArchive;
