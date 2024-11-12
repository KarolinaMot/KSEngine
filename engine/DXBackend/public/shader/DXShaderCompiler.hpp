#include <Common.hpp>
#include <DXCommon.hpp>

#include <optional>
#include <shader/DXShader.hpp>
#include <string>

class DXShaderCompiler
{
public:
    static inline constexpr auto DEFAULT_ENTRY_POINT = L"main";
    static inline constexpr auto DEFAULT_SHADER_MODEL_VERSION = L"6_3";

    DXShaderCompiler(const wchar_t* shader_model_version = DEFAULT_SHADER_MODEL_VERSION);

    NON_COPYABLE(DXShaderCompiler);
    NON_MOVABLE(DXShaderCompiler);

    void AddIncludeDirectory(const std::wstring& path) { include_paths.emplace_back(path); }
    std::optional<DXShader> CompileFromBytes(const std::vector<std::byte>& shader_source, DXShader::Type type, const wchar_t* entry_point = DEFAULT_ENTRY_POINT);
    std::optional<DXShader> CompileFromPath(const std::string& path, DXShader::Type type, const wchar_t* entry_point = DEFAULT_ENTRY_POINT);

private:
    std::wstring shader_version {};
    std::vector<std::wstring> include_paths {};

    ComPtr<IDxcUtils> utilities;
    ComPtr<IDxcCompiler3> compiler;
    ComPtr<IDxcIncludeHandler> include_handler;
};