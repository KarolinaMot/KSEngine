#include <Engine.hpp>

int KSE::EngineClass::Run()
{
	while (!should_close)
	{
		for (auto&& system : systems) system->Update();
	}

	return exit_code;
}
