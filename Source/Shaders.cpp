#include "BaseUtils.hpp"
#include "Shaders.hpp"
#include "Renderer.hpp"
#include "Settings.hpp"
#include "SmallFileCache.hpp"
#include "../ThirdParty/dxc_2021_12_08/inc/dxcapi.h"
#pragma comment(lib, "../ThirdParty/dxc_2021_12_08/lib/x64/dxcompiler.lib")

static StringSequenceSetting g_ShadersExtraParameters(SettingCategory::Load, "Shaders.ExtraParameters");
static BoolSetting g_ShadersEmbedDebugInformation(SettingCategory::Load, "Shaders.EmbedDebugInformation", false);

static constexpr uint32_t CODE_PAGE = DXC_CP_UTF8;

class IncludeHandler : public IDxcIncludeHandler
{
public:
    IncludeHandler(std::filesystem::path&& dir) :
        m_Directory(dir)
    {
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) { return E_FAIL; }
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return 1; }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return 0; }

    virtual HRESULT STDMETHODCALLTYPE LoadSource(
        _In_z_ LPCWSTR pFilename,
        _COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource);

private:
    std::filesystem::path m_Directory;
};

class ShaderPimpl
{
public:
    ComPtr<IDxcBlob> m_CompiledObject;
};

class ShaderCompilerPimpl
{
public:
    ComPtr<IDxcUtils> m_Utils;
    ComPtr<IDxcCompiler3> m_Compiler;
};

HRESULT STDMETHODCALLTYPE IncludeHandler::LoadSource(
    _In_z_ LPCWSTR pFilename,
    _COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource)
{
    assert(g_Renderer && g_Renderer->GetShaderCompiler());
    ShaderCompilerPimpl* compiler = g_Renderer->GetShaderCompiler()->m_Pimpl.get();

    std::filesystem::path filePath = m_Directory / pFilename;
    const wchar_t* const filePathNative = filePath.native().c_str();

    const auto contents = g_SmallFileCache->LoadFile(filePathNative);
    ComPtr<IDxcBlobEncoding> blobEncoding;
    HRESULT hr = compiler->m_Utils->CreateBlob(contents.data(), (uint32_t)contents.size(), CODE_PAGE, &blobEncoding);
    if(SUCCEEDED(hr))
        blobEncoding->QueryInterface(IID_PPV_ARGS(ppIncludeSource));
    return hr;
}

Shader::Shader() :
    m_Pimpl(std::make_unique<ShaderPimpl>())
{
}

Shader::~Shader()
{

}

void Shader::Init(ShaderType type, const wstr_view& filePath, const wstr_view& entryPointName)
{
    assert(g_Renderer && g_Renderer->GetShaderCompiler());
    ShaderCompilerPimpl* compiler = g_Renderer->GetShaderCompiler()->m_Pimpl.get();

    ERR_TRY;

    LogMessageF(L"Compiling shader from \"{}\"...", filePath);

    std::filesystem::path dir = StrToPath(filePath);
    dir = dir.parent_path();
    IncludeHandler includeHandler(std::move(dir));

    const std::vector<char> source = g_SmallFileCache->LoadFile(filePath);
    CHECK_BOOL(!source.empty());
    const DxcBuffer sourceDxcBuffer = {
        .Ptr = source.data(),
        .Size = source.size(),
        .Encoding = CODE_PAGE};

    const wchar_t* profileParam = nullptr;
    switch(type)
    {
    case ShaderType::Vertex: profileParam = L"-T vs_6_0"; break;
    case ShaderType::Pixel:  profileParam = L"-T ps_6_0"; break;
    default: assert(0);
    }

    const wstring entryPointParam = std::format(L"-E {}", entryPointName);

    std::vector<const wchar_t*> arguments = {
        profileParam,
        entryPointParam.c_str()};
    
    const size_t extraParamCount = g_ShadersExtraParameters.m_Strings.size();
    std::vector<wstring> extraParamsUnicode(extraParamCount);
    for(size_t i = 0; i < extraParamCount; ++i)
    {
        extraParamsUnicode[i] = ConvertCharsToUnicode(g_ShadersExtraParameters.m_Strings[i], CP_UTF8);
        arguments.push_back(extraParamsUnicode[i].c_str());
    }
    if(g_ShadersEmbedDebugInformation.GetValue())
    {
        arguments.push_back(L"-Zi");
        arguments.push_back(L"-Qembed_debug");
    }

    ComPtr<IDxcResult> result;
    // This seems to always succeed. Real success is returned by IDxcOperationResult::GetStatus.
    CHECK_HR(compiler->m_Compiler->Compile(
        &sourceDxcBuffer,
        arguments.data(), (uint32_t)arguments.size(),
        &includeHandler,
        IID_PPV_ARGS(&result)));
    CHECK_BOOL(result);
    HRESULT compilationResult = 0;
    CHECK_HR(result->GetStatus(&compilationResult));

    ComPtr<IDxcBlobEncoding> errors;
    // Intentionally ignoring result.
    result->GetErrorBuffer(&errors);
    str_view errorsView;
    if(errors && errors->GetBufferSize() > 0)
    {
        // Minus one to remove null terminator.
        errorsView = str_view((const char*)errors->GetBufferPointer(), errors->GetBufferSize() - 1);
        // Trim trailing whitespaces.
        while(!errorsView.empty() && isspace(errorsView.back()))
            errorsView = errorsView.substr(0, errorsView.length() - 1);
    }

    if(SUCCEEDED(compilationResult))
    {
        CHECK_HR(result->GetResult(&m_Pimpl->m_CompiledObject));
        CHECK_BOOL(m_Pimpl->m_CompiledObject && m_Pimpl->m_CompiledObject->GetBufferSize() > 0);
        
        if(errors && errors->GetBufferSize() > 0)
            LogWarningF(L"{}", errorsView);
    }
    else
    {
        if(errors && errors->GetBufferSize() > 0)
            FAIL(std::format(L"{}", errorsView));
        else
            FAIL(L"No error buffer.");
    }

    ERR_CATCH_MSG(std::format(L"Cannot compile shader from \"{}\".", filePath));
}

bool Shader::IsNull() const
{
    return !m_Pimpl->m_CompiledObject;
}

std::span<const char> Shader::GetCode() const
{
    assert(m_Pimpl->m_CompiledObject);
    return std::span<const char>(
        (const char*)m_Pimpl->m_CompiledObject->GetBufferPointer(),
        m_Pimpl->m_CompiledObject->GetBufferSize());
}

ShaderCompiler::ShaderCompiler() :
    m_Pimpl(std::make_unique<ShaderCompilerPimpl>())
{

}

void ShaderCompiler::Init()
{
    ERR_TRY;
    
    CHECK_HR(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_Pimpl->m_Utils)));
    CHECK_HR(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_Pimpl->m_Compiler)));
    
    ERR_CATCH_MSG(L"Cannot initialize shader compiler.");
}

ShaderCompiler::~ShaderCompiler()
{

}
