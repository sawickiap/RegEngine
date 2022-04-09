#pragma once

enum class ShaderType
{
    Vertex, Pixel, Compute, Count
};

class ShaderPimpl;
class MultiShaderPimpl;
class ShaderCompilerPimpl;

class Shader
{
public:
    Shader();
    ~Shader();
    // On error: throws exception.
    void Init(ShaderType type, const wstr_view& filePath, const wstr_view& entryPointName,
        std::span<const wstr_view> macroNames, std::span<const uint32_t> macroValues);
    void Init(ShaderType type, const wstr_view& filePath, const wstr_view& entryPointName);
    bool IsNull() const;
    std::span<const char> GetCode() const;

private:
    unique_ptr<ShaderPimpl> m_Pimpl;
};

class MultiShader
{
public:
    static size_t HashMacroValues(std::span<const uint32_t> macroValues);

    MultiShader();
    ~MultiShader();
    // Doesn't fail in this one.
    void Init(ShaderType type, const wstr_view& filePath, const wstr_view& entryPointName,
        std::span<const wstr_view> macroNames);
    void Clear();
    
    // On error: prints error, returns null.
    const Shader* GetShader(std::span<const uint32_t> macroValues);

private:
    unique_ptr<MultiShaderPimpl> m_Pimpl;
};

class ShaderCompiler
{
public:
    ShaderCompiler();
    void Init();
    ~ShaderCompiler();

private:
    friend class Shader;
    friend class IncludeHandler;

    unique_ptr<ShaderCompilerPimpl> m_Pimpl;
};
