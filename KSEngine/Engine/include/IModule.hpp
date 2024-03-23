#pragma once

namespace KSE {

class EngineClass;

//Module interface
//Parts of the engine that are queried by Systems and other Modules
class IModule {
public:
	IModule(EngineClass& engine) : Engine(engine) {};
	IModule(const IModule&) = delete;
	virtual ~IModule() = default;

protected:
	EngineClass& Engine;
};

}