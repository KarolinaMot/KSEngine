#pragma once
#include <memory>

namespace KSE {

template<typename T>
class ResourceHandle {
public:
	ResourceHandle() = default;
	ResourceHandle(std::shared_ptr<T> new_res) : res(new_res) {};

	std::shared_ptr<T> Get() const { return res; }

private:
	std::shared_ptr<T> res{};
};

//Forward declare all resource types

//Common
class Mesh;
class Image;
class Model;

//Platform Specific
class Buffer;
class Texture;
class Framebuffer;
class Shader;

}