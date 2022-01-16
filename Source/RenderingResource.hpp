#pragma once

class CommandList;

/*
Represents a resource used during rendering e.g. a render target texture.
Tracks its last D3D12_RESOURCE_STATES.
*/
class RenderingResource
{
public:
    void Init(
        const D3D12_RESOURCE_DESC& resDesc,
        D3D12_RESOURCE_STATES initialStates,
        const wstr_view& name,
        const D3D12_CLEAR_VALUE* optimizedClearValue = nullptr);
    // redDesc is optional, can be null. Then it's fetched from res->GetDesc().
    void InitExternallyOwned(
        ID3D12Resource* res,
        D3D12_RESOURCE_STATES currentStates,
        const D3D12_RESOURCE_DESC* resDesc = nullptr);
    
    ID3D12Resource* GetResource() const { return m_MyRes ? m_MyRes->GetResource() : m_ExternallyOwnedRes; }
    const D3D12_RESOURCE_DESC& GetDesc() const { return m_Desc; }
    
    void SetStates(CommandList& cmdList, D3D12_RESOURCE_STATES states);

private:
    ComPtr<D3D12MA::Allocation> m_MyRes;
    ID3D12Resource* m_ExternallyOwnedRes = nullptr;
    D3D12_RESOURCE_DESC m_Desc = D3D12_RESOURCE_DESC{};
    D3D12_RESOURCE_STATES m_States = D3D12_RESOURCE_STATE_COMMON;
};
