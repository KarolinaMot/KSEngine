#pragma once
#include <IModule.hpp>
#include <resources/ResourceHandle.hpp>
#include <memory>

namespace KSE {

//Module for handling platform specific windowing and input
class DeviceModule : public IModule {
public:

	//TODO: Add window size, name parameters
	DeviceModule(EngineClass& e);
	~DeviceModule();

	void SwapWindowBuffers(ResourceHandle<Framebuffer> rendered_frame);

	//TODO: add input here or on another InputModule

private:
	class Impl;
	std::unique_ptr<Impl> impl;
};


}