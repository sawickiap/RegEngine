#pragma once

enum class ShaderType
{
    Vertex, Pixel, Compute, Count
};

class ShaderPimpl;
class ShaderCompilerPimpl;

class Shader
{
public:
    Shader();
    ~Shader();
    void Init(ShaderType type, const wstr_view& filePath, const wstr_view& entryPointName);
    bool IsNull() const;
    std::span<const char> GetCode() const;

private:
    unique_ptr<ShaderPimpl> m_Pimpl;
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
