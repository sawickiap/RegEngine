#include "BaseUtils.hpp"
#include "Shaders.hpp"
#include "Renderer.hpp"
#include "Settings.hpp"
#include "SmallFileCache.hpp"
#include "../ThirdParty/dxc_2021_12_08/inc/dxcapi.h"
#pragma comment(lib, "../ThirdParty/dxc_2021_12_08/lib/x64/dxcompiler.lib")
#include <unordered_map>

static StringSequenceSetting g_ShadersExtraParameters(SettingCategory::Load, "Shaders.ExtraParameters");
static BoolSetting g_ShadersEmbedDebugInformation(SettingCategory::Load, "Shaders.EmbedDebugInformation", false);

static constexpr uint32_t CODE_PAGE = DXC_CP_UTF8;

static const wchar_t* TYPE_NAMES[] = {
    L"Vertex", L"Pixel", L"Compute"
};
static_assert(_countof(TYPE_NAMES) == (size_t)ShaderType::Count);

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

class MultiShaderPimpl
{
public:
    // it.second = null means shader compilation fails.
    using MapType = std::unordered_map<size_t, unique_ptr<Shader>>;

    ShaderType m_Type = ShaderType::Count;
    wstring m_FilePath;
    wstring m_EntryPointName;
    std::vector<wstring> m_MacroNames;
    std::vector<wstr_view> m_MacroNameViews;
    MapType m_Shaders;

    bool IsInitialized() const { return m_Type != ShaderType::Count && !m_FilePath.empty(); }
    wstring MacrosToDebugStr(std::span<const uint32_t> macroValues) const;
};

wstring MultiShaderPimpl::MacrosToDebugStr(std::span<const uint32_t> macroValues) const
{
    wstring result;
    const size_t count = m_MacroNames.size();
    assert(count == macroValues.size());
    for(size_t i = 0; i < count; ++i)
    {
        if(i > 0)
            result += L' ';
        result += std::format(L"{}={}", m_MacroNames[i], macroValues[i]);
    }
    return result;
}

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

void Shader::Init(ShaderType type, const wstr_view& filePath, const wstr_view& entryPointName,
    std::span<const wstr_view> macroNames, std::span<const uint32_t> macroValues)
{
    assert(g_Renderer && g_Renderer->GetShaderCompiler());
    assert(macroNames.size() == macroValues.size());
    ShaderCompilerPimpl* compiler = g_Renderer->GetShaderCompiler()->m_Pimpl.get();

    ERR_TRY;

    const size_t explicitMacroCount = macroNames.size();
    if(explicitMacroCount == 0)
        LogMessageF(L"Compiling {} shader from \"{}\"...", TYPE_NAMES[(size_t)type], filePath);
    else
    {
        wstring explicitMacroDebugStr;
        for(size_t i = 0; i < explicitMacroCount; ++i)
            explicitMacroDebugStr += std::format(L" {}={}", macroNames[i], macroValues[i]);
        LogMessageF(L"Compiling {} shader from \"{}\" with macros:{}...", TYPE_NAMES[(size_t)type], filePath, explicitMacroDebugStr);
    }

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
    const wchar_t* shaderTypeMacroParam = nullptr;
    switch(type)
    {
    case ShaderType::Vertex:  profileParam = L"-T vs_6_0"; shaderTypeMacroParam = L"-D VERTEX_SHADER=1"; break;
    case ShaderType::Pixel:   profileParam = L"-T ps_6_0"; shaderTypeMacroParam = L"-D PIXEL_SHADER=1"; break;
    case ShaderType::Compute: profileParam = L"-T cs_6_0"; shaderTypeMacroParam = L"-D COMPUTE_SHADER=1"; break;
    default: assert(0);
    }

    const wstring entryPointParam = std::format(L"-E {}", entryPointName);

    std::vector<const wchar_t*> arguments = {
        profileParam,
        shaderTypeMacroParam,
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

    for(size_t i = 0; i < explicitMacroCount; ++i)
    {
        extraParamsUnicode.push_back(std::format(L"-D {}={}", macroNames[i], macroValues[i]));
        arguments.push_back(extraParamsUnicode.back().c_str());
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
        
        if(!errorsView.empty())
            LogWarningF(L"{}", errorsView);
    }
    else
    {
        if(!errorsView.empty())
            FAIL(std::format(L"{}", errorsView));
        else
            FAIL(L"No error buffer.");
    }

    ERR_CATCH_MSG(std::format(L"Cannot compile shader from \"{}\".", filePath));
}

void Shader::Init(ShaderType type, const wstr_view& filePath, const wstr_view& entryPointName)
{
    Init(type, filePath, entryPointName, {}, {});
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

size_t MultiShader::HashMacroValues(std::span<const uint32_t> macroValues)
{
    size_t h = 0x262521a102765664llu;
    for(auto val : macroValues)
        h = CombineHash(h, val);
    return h;
}

MultiShader::MultiShader() :
    m_Pimpl(std::make_unique<MultiShaderPimpl>())
{

}

MultiShader::~MultiShader()
{

}

void MultiShader::Init(ShaderType type, const wstr_view& filePath, const wstr_view& entryPointName,
    std::span<const wstr_view> macroNames)
{
    m_Pimpl->m_Type = type;
    m_Pimpl->m_FilePath = filePath;
    m_Pimpl->m_EntryPointName = entryPointName;

    const size_t macroCount = macroNames.size();
    m_Pimpl->m_MacroNames.resize(macroCount);
    m_Pimpl->m_MacroNameViews.resize(macroCount);
    for(size_t i = 0; i < macroCount; ++i)
    {
        m_Pimpl->m_MacroNames[i].assign(macroNames[i].data(), macroNames[i].length());
        m_Pimpl->m_MacroNameViews[i] = wstr_view{m_Pimpl->m_MacroNames[i]};
    }
}

void MultiShader::Clear()
{
    m_Pimpl->m_Shaders.clear();
}

const Shader* MultiShader::GetShader(std::span<const uint32_t> macroValues)
{
    assert(m_Pimpl->IsInitialized());
    assert(macroValues.size() == m_Pimpl->m_MacroNames.size());
    const size_t h = HashMacroValues(macroValues);
    
    const auto it = m_Pimpl->m_Shaders.find(h);
    if(it != m_Pimpl->m_Shaders.end())
        return it->second.get();
    
    try
    {
        ERR_TRY

        unique_ptr<Shader> shader = std::make_unique<Shader>();
        Shader* const shaderPtr = shader.get();
        shader->Init(m_Pimpl->m_Type, m_Pimpl->m_FilePath, m_Pimpl->m_EntryPointName,
            m_Pimpl->m_MacroNameViews, macroValues);
        m_Pimpl->m_Shaders.insert({h, std::move(shader)});
        return shaderPtr;

        ERR_CATCH_MSG(std::format(L"Cannot compile multishader \"{}\" with macros: {}",
            m_Pimpl->m_FilePath, m_Pimpl->MacrosToDebugStr(macroValues)));
    }
    catch(const Exception& ex)
    {
        ex.Print();
        m_Pimpl->m_Shaders.insert({h, nullptr});
        return nullptr;
    }
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

/*
Macros predefined in shader code:

Depending on shader type
    VERTEX_SHADER=1
    PIXEL_SHADER=1
    COMPUTE_SHADER=1
*/
