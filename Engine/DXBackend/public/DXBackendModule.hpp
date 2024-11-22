#pragma once
#include <DXDevice.hpp>
#include <Engine.hpp>
#include <initialization/DXFactory.hpp>
#include <shader/DXShaderCompiler.hpp>

class DXBackendModule : public ModuleInterface
{
public:
    virtual ~DXBackendModule() = default;
    DXDevice& GetDevice() { return *device; }
    DXFactory& GetFactory() { return *factory; }
    DXShaderCompiler& GetShaderCompiler() { return *shader_compiler; }

private:
    void Initialize(Engine& e) override;
    void Shutdown(Engine& e) override;

    std::unique_ptr<DXFactory> factory {};
    std::unique_ptr<DXDevice> device {};
    std::unique_ptr<DXShaderCompiler> shader_compiler {};
};