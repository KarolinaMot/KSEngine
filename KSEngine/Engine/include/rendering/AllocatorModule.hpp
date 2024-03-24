#pragma once
#include <IModule.hpp>
#include <resources/ResourceHandle.hpp>

namespace KSE {

//Module for handling platform specific allocations
//Mostly used for allocating GPU resources
class AllocatorModule : public IModule {
public:
	AllocatorModule(EngineClass& e);
	~AllocatorModule();

	//TODO: specify and define these parameters
	ResourceHandle<Buffer> AllocateBuffer();

	//TODO: same as above but join the texture and the sampler
	ResourceHandle<Texture> AllocateTexture();

private:
	class Impl;
	std::unique_ptr<Impl> impl;
};


}