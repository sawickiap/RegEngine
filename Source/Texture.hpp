#pragma once

#include "Descriptors.hpp"

/*
Represents a texture, initialized once, then available for sampling.
Also creates and keeps its SRV descriptor.
*/
class Texture
{
public:
    ~Texture();
    void LoadFromFile(const wstr_view& filePath);
    void LoadFromMemory(
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

    void CreateDescriptor();
};
