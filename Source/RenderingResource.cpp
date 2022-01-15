#include "BaseUtils.hpp"
#include "RenderingResource.hpp"
#include "CommandList.hpp"
#include "Renderer.hpp"

static bool NewStatesAreSubset(D3D12_RESOURCE_STATES newStates, D3D12_RESOURCE_STATES oldStates)
{
    if((newStates == 0) != (oldStates == 0))
        return false;
    if((newStates & ~oldStates) != 0)
        return false;
    return true;
}

void RenderingResource::Init(
    const D3D12_RESOURCE_DESC& resDesc,
    D3D12_RESOURCE_STATES initialStates,
    const wstr_view& name,
    const D3D12_CLEAR_VALUE* optimizedClearValue)
{
    m_Desc = resDesc;
    m_States = initialStates;
    const D3D12_HEAP_PROPERTIES defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    CHECK_HR(g_Renderer->GetDevice()->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        initialStates,
        optimizedClearValue,
        IID_PPV_ARGS(&m_MyRes)));
    if(!name.empty())
        SetD3D12ObjectName(m_MyRes, name);
}

void RenderingResource::InitExternallyOwned(
    ID3D12Resource* res,
    D3D12_RESOURCE_STATES currentStates,
    const D3D12_RESOURCE_DESC* resDesc)
{
    assert(m_ExternallyOwnedRes == nullptr);
    m_ExternallyOwnedRes = res;
    if(resDesc)
        m_Desc = *resDesc;
    else
        m_Desc = res->GetDesc();
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
