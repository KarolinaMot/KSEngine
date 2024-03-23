#pragma once
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <vector>
#include <concepts>

#include <IModule.hpp>
#include <ISystem.hpp>

namespace KSE {

//Main engine class, acting as a service locator for all subsystems
//The engine is divided in four parts: Modules, Initialization, Runtime, Cleanup
//Modules: represent APIs that can be queried by all other systems
//(example: sound system with functions for playing sound)
//Systems: represent Logic that must be executed every frame (TODO: add system priorities?)
//(example: moving a character every frame based on input and deltatime)
//Initialization: engine must be configured with AddModule<T> ot AddSystem<T> functions
//Cleanup: All modules are destroyed in the opposite order of initialization
class EngineClass {
public:
	
	EngineClass() = default;

	~EngineClass() {
		for (auto r_it = module_deallocation_order.rbegin(); r_it != module_deallocation_order.rend(); ++r_it)
			modules.at(*r_it).reset();
	};

	EngineClass(const EngineClass&) = delete;

	template<typename T, typename... Args>
		requires std::is_base_of_v<ISystem, T> && std::constructible_from<T, EngineClass&, Args...> 

	EngineClass& AddSystem(Args&&... args) {
		systems.emplace_back(std::make_shared<T>(*this, std::forward<Args>(args)...));
		return *this;
	}

	template<typename T, typename... Args>
		requires std::is_base_of_v<IModule, T>&& std::constructible_from<T, EngineClass&, Args...>

	EngineClass& AddModule(Args&&... args) {
		modules.emplace(std::type_index(typeid(T)), std::make_shared<T>(*this, std::forward<Args>(args)...));
		module_deallocation_order.emplace_back(std::type_index(typeid(T)));
		return *this;
	}

	template<typename T>
	std::weak_ptr<T> GetModule() {
		auto type_index = std::type_index(typeid(T));
		if (auto it = modules.find(type_index); it != modules.end()) {
			return std::dynamic_pointer_cast<T>(it->second);
		}
		return {};
	}

	//Executes event loop and all systems per frame
	int Run();

	//Sets the engine to close on next frame, returning code value in Run()
	void RequestExit(int code = 0) { should_close = true; exit_code = code; }

private:
	bool should_close = false;
	int exit_code = 0;

	std::unordered_map<std::type_index, std::shared_ptr<IModule>> modules;
	std::vector<std::type_index> module_deallocation_order;
	std::vector<std::shared_ptr<ISystem>> systems;
};

}