#pragma once

namespace KSE {

class EngineClass;

//System interface
//Parts of the engine that are updated every frame
//They are not meant to be accessible 
class ISystem {
public:
	ISystem(EngineClass& engine) : Engine(engine) {};
	ISystem(const ISystem&) = delete;

	virtual void Update() = 0;
	virtual ~ISystem() = default;

protected:
	EngineClass& Engine;
};

}