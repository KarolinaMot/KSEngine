#include <FileIO.hpp>
#include <Log.hpp>
#include <shader/DXShaderCompiler.hpp>

DXShaderCompiler::DXShaderCompiler(const wchar_t* shader_model_version)
{
    shader_version = shader_model_version;
    CheckDX(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utilities)));
    CheckDX(utilities->CreateDefaultIncludeHandler(&include_handler));
    CheckDX(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)));
}

std::optional<DXShader> DXShaderCompiler::CompileFromBytes(const std::vector<std::byte>& shader_source, DXShader::Type type, const wchar_t* entry_point)
{
    auto src_buf = DxcBuffer { shader_source.data(), shader_source.size(), 0 };

    std::wstring shader_type {};

    switch (type)
    {
    case DXShader::Type::VERTEX:
        shader_type = (L"vs_" + shader_version);
        break;
    case DXShader::Type::PIXEL:
        shader_type = (L"ps_" + shader_version);
        break;
    case DXShader::Type::COMPUTE:
        shader_type = (L"cs_" + shader_version);
        break;
    default:
        Log("Shader Compilation Error: Unrecognized Shader Type!");
        break;
    }

    std::vector<const wchar_t*> arguments;

    arguments.push_back(L"-E");
    arguments.push_back(entry_point);

    arguments.push_back(L"-T");
    arguments.push_back(shader_type.c_str());

    for (auto& include_path : include_paths)
    {
        arguments.push_back(L"-I");
        arguments.push_back(include_path.c_str());
    }

    ComPtr<IDxcResult> compilation_result;

    CheckDX(compiler->Compile(
        &src_buf, arguments.data(),
        static_cast<uint32_t>(arguments.size()),
        include_handler.Get(), IID_PPV_ARGS(&compilation_result)));

    if (compilation_result->HasOutput(DXC_OUT_OBJECT))
    {
        // Print warnings if any
        if (compilation_result->HasOutput(DXC_OUT_ERRORS))
        {
            ComPtr<IDxcBlobUtf8> compilation_warns {};
            CheckDX(compilation_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&compilation_warns), NULL));
            if (compilation_warns && compilation_warns->GetStringLength() > 0)
            {
                Log("Shader Compilation Warnings: {}", (char*)compilation_warns->GetBufferPointer());
            }
        }

        ComPtr<IDxcBlob> shader_bytecode;
        CheckDX(compilation_result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader_bytecode), NULL));
        return DXShader { type, shader_bytecode };
    }
    else if (compilation_result->HasOutput(DXC_OUT_ERRORS))
    {
        ComPtr<IDxcBlobUtf8> compilation_errors {};
        CheckDX(compilation_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&compilation_errors), NULL));
        if (compilation_errors && compilation_errors->GetStringLength() > 0)
        {
            Log("Shader Compilation Errors: {}", (char*)compilation_errors->GetBufferPointer());
        }
        return std::nullopt;
    }
    else
    {
        Log("Shader Compilation Error: UNKNOWN");
        return std::nullopt;
    }
}

std::optional<DXShader> DXShaderCompiler::CompileFromPath(const std::string& path, DXShader::Type type, const wchar_t* entry_point)
{
    if (auto stream = FileIO::OpenReadStream(path))
    {
        auto dump = FileIO::DumpFullStream(stream.value());
        return CompileFromBytes(dump, type, entry_point);
    }
    else
    {
        Log("Could not read from file: {}", path);
        return std::nullopt;
    }
}