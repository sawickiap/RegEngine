#pragma once

class CommandList;

/*
Represents a resource used during rendering e.g. a render target texture.
Tracks its last D3D12_RESOURCE_STATES.
*/
class RenderingResource
{
public:
    void InitExternallyOwned(
        ID3D12Resource* res,
        D3D12_RESOURCE_STATES currentStates);
    ID3D12Resource* GetResource() const { return m_ExternallyOwnedRes; }
    void SetStates(CommandList& cmdList, D3D12_RESOURCE_STATES states);

private:
    ID3D12Resource* m_ExternallyOwnedRes = nullptr;
    D3D12_RESOURCE_STATES m_States = D3D12_RESOURCE_STATE_COMMON;
};
