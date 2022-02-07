#pragma once

#include "Descriptors.hpp"

/*
Represents a texture, initialized once, then available for sampling.
Also creates and keeps its SRV descriptor.
*/
class Texture
{
public:
    enum FLAGS
    {
        // Treat pixels as sRGB, create texture in sRGB format (affects LoadFromFile only).
        FLAG_SRGB = 0x1,
        // Generates mipmaps (affects LoadFromFile only).
        FLAG_GENERATE_MIPMAPS = 0x2,
    };

    ~Texture();
    void LoadFromFile(uint32_t flags, const wstr_view& filePath);
    void LoadFromMemory(uint32_t flags,
        const D3D12_RESOURCE_DESC& resDesc,
        const D3D12_SUBRESOURCE_DATA& data,
        const wstr_view& name);

    bool IsEmpty() const { return !m_Resource; }
    ID3D12Resource* GetResource() const { return m_Resource.Get(); }
    const D3D12_RESOURCE_DESC& GetDesc() const { return m_Desc; }
    uvec2 GetSize() const { return uvec2((uint32_t)GetDesc().Width, (uint32_t)GetDesc().Height); }
    Descriptor GetDescriptor() const { return m_Descriptor; }

private:
    // May be null in case m_Resource was created by DirectXTK12, without D3D12MA.
    // If not null, m_Resource == m_Allocation->GetResource().
    ComPtr<D3D12MA::Allocation> m_Allocation;
    ComPtr<ID3D12Resource> m_Resource;
    D3D12_RESOURCE_DESC m_Desc = {};
    Descriptor m_Descriptor;

    void CreateTexture(const wstr_view& name);
    // lastLevel = true issues a barrier to transition texture to D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE.
    void UploadMipLevel(uint32_t mipLevel, const uvec2& size, const D3D12_SUBRESOURCE_DATA& data,
        bool lastLevel);
    void CreateDescriptor();
};
