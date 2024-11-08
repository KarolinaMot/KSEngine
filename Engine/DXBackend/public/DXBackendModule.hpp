#pragma once
#include <DXDevice.hpp>
#include <Engine.hpp>
#include <initialization/DXFactory.hpp>

class DXBackendModule : public ModuleInterface
{
public:
    virtual ~DXBackendModule() = default;
    DXDevice& GetDevice() { return *device; }

private:
    void Initialize(Engine& e) override;
    void Shutdown(Engine& e) override;

    std::unique_ptr<DXFactory> factory {};
    std::unique_ptr<DXDevice> device {};
};

void Test();