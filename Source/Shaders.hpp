#pragma once

enum class ShaderType
{
    Vertex, Pixel, Count
};

class ShaderPimpl;

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
