#pragma once
#include <Engine.hpp>
#include <Window.hpp>
#include <glm/vec2.hpp>

class ApplicationModule : public ModuleInterface
{
public:
    virtual ~ApplicationModule() = default;

    Window& GetMainWindow() { return *main_window; }

private:
    void Initialize(Engine& e) override;
    void Shutdown(Engine& e) override;

    void ProcessInput(Engine& e);

    std::unique_ptr<Window> main_window;
};