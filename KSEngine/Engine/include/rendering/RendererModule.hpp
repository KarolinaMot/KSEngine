#pragma once
#include <IModule.hpp>
#include <resources/ResourceHandle.hpp>

namespace KSE {

//Module for handling platform specific allocations
//Mostly used for allocating GPU resources
class RendererModule : public IModule {
public:
	RendererModule(EngineClass& e);
	~RendererModule();

	//Outputs a Framebuffer resource
	ResourceHandle<Framebuffer> RenderFrame();

private:
	class Impl;
	std::unique_ptr<Impl> impl;
};


}