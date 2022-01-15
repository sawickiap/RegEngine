#pragma once

/*
Represents a texture, initialized once, then available for sampling.
*/
class Texture
{
public:
    void LoadFromFile(const wstr_view& filePath);
    void LoadFromMemory(
        const D3D12_RESOURCE_DESC& resDesc,
        const D3D12_SUBRESOURCE_DATA& data,
        const wstr_view& name);

    bool IsEmpty() const { return !m_Resource; }
    ID3D12Resource* GetResource() const { return m_Resource.Get(); }
    const D3D12_RESOURCE_DESC& GetDesc() const { return m_Desc; }
    uvec2 GetSize() const { return uvec2((uint32_t)GetDesc().Width, (uint32_t)GetDesc().Height); }

private:
    ComPtr<ID3D12Resource> m_Resource;
    D3D12_RESOURCE_DESC m_Desc;
};
