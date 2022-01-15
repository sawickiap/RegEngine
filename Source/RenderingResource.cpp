#include "BaseUtils.hpp"
#include "RenderingResource.hpp"
#include "CommandList.hpp"

static bool NewStatesAreSubset(D3D12_RESOURCE_STATES newStates, D3D12_RESOURCE_STATES oldStates)
{
    if((newStates == 0) != (oldStates == 0))
        return false;
    if((newStates & ~oldStates) != 0)
        return false;
    return true;
}

void RenderingResource::InitExternallyOwned(
    ID3D12Resource* res,
    D3D12_RESOURCE_STATES currentStates)
{
    assert(m_ExternallyOwnedRes == nullptr);
    m_ExternallyOwnedRes = res;
    m_States = currentStates;
}

void RenderingResource::SetStates(CommandList& cmdList, D3D12_RESOURCE_STATES states)
{
    if(!NewStatesAreSubset(states, m_States))
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            GetResource(), m_States, states);
        cmdList.GetCmdList()->ResourceBarrier(1, &barrier);
        m_States = states;
    }
}
