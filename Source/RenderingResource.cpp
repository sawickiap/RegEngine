#include "BaseUtils.hpp"
#include "RenderingResource.hpp"
#include "CommandList.hpp"
#include "Renderer.hpp"

static bool NewStatesAreSubset(D3D12_RESOURCE_STATES newStates, D3D12_RESOURCE_STATES oldStates)
{
    if((newStates == D3D12_RESOURCE_STATE_COMMON) != (oldStates == D3D12_RESOURCE_STATE_COMMON))
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
    assert(!m_ExternallyOwnedRes && !m_MyRes);
    
    m_Desc = resDesc;
    m_States = initialStates;
    
    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
    allocDesc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
    CHECK_HR(g_Renderer->GetMemoryAllocator()->CreateResource(
        &allocDesc,
        &resDesc,
        initialStates,
        optimizedClearValue,
        &m_MyRes,
        IID_NULL, NULL)); // riidResource, ppvResource
    
    if(!name.empty())
        SetD3D12ObjectName(m_MyRes->GetResource(), name);

    InitDescriptors();
}

RenderingResource::~RenderingResource()
{
    g_Renderer->GetDSVDescriptorManager()->FreePersistent(m_DSV);
    g_Renderer->GetRTVDescriptorManager()->FreePersistent(m_RTV);
    g_Renderer->GetSRVDescriptorManager()->FreePersistent(m_UAV);
    g_Renderer->GetSRVDescriptorManager()->FreePersistent(m_SRV);
}

bool RenderingResource::HasCurrentState(D3D12_RESOURCE_STATES state) const
{
    return NewStatesAreSubset(state, m_States);
}

void RenderingResource::InitExternallyOwned(
    ID3D12Resource* res,
    D3D12_RESOURCE_STATES currentStates,
    const D3D12_RESOURCE_DESC* resDesc)
{
    assert(!m_ExternallyOwnedRes && !m_MyRes);
    
    m_ExternallyOwnedRes = res;
    if(resDesc)
        m_Desc = *resDesc;
    else
        m_Desc = res->GetDesc();
    m_States = currentStates;

    InitDescriptors();
}

D3D12_GPU_DESCRIPTOR_HANDLE RenderingResource::GetD3D12SRV() const
{
    assert(g_Renderer && g_Renderer->GetSRVDescriptorManager() && !m_SRV.IsNull());
    return g_Renderer->GetSRVDescriptorManager()->GetGPUHandle(m_SRV);
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderingResource::GetD3D12RTV() const
{
    assert(g_Renderer && g_Renderer->GetRTVDescriptorManager() && !m_RTV.IsNull());
    return g_Renderer->GetRTVDescriptorManager()->GetCPUHandle(m_RTV);
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderingResource::GetD3D12DSV() const
{
    assert(g_Renderer && g_Renderer->GetDSVDescriptorManager() && !m_DSV.IsNull());
    return g_Renderer->GetDSVDescriptorManager()->GetCPUHandle(m_DSV);
}

void RenderingResource::TransitionToStates(CommandList& cmdList, D3D12_RESOURCE_STATES states)
{
    if(!NewStatesAreSubset(states, m_States))
    {
        const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            GetResource(), m_States, states);
        cmdList.GetCmdList()->ResourceBarrier(1, &barrier);
        m_States = states;
    }
}

void RenderingResource::InitDescriptors()
{
    DescriptorManager* const SRVManager = g_Renderer->GetSRVDescriptorManager();
    DescriptorManager* const RTVManager = g_Renderer->GetRTVDescriptorManager();
    DescriptorManager* const DSVManager = g_Renderer->GetDSVDescriptorManager();
    ID3D12Device* dev = g_Renderer->GetDevice();
    
    if((m_Desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
        const D3D12_SHADER_RESOURCE_VIEW_DESC* viewDescPtr = nullptr;
        // Special formats
        if(m_Desc.Format == DXGI_FORMAT_D32_FLOAT)
        {
            viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            viewDesc.Format = DXGI_FORMAT_R32_FLOAT;
            viewDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
                D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
                D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1,
                D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2,
                D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3);
            viewDesc.Texture2D = {
                .MipLevels = 1 };
            viewDescPtr = &viewDesc;
        }
        m_SRV = SRVManager->AllocatePersistent(1);
        dev->CreateShaderResourceView(GetResource(), viewDescPtr, SRVManager->GetCPUHandle(m_SRV));
    }
    if((m_Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0)
    {
        m_UAV = SRVManager->AllocatePersistent(1);
        dev->CreateUnorderedAccessView(GetResource(), nullptr, nullptr, SRVManager->GetCPUHandle(m_UAV));
    }
    if((m_Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0)
    {
        m_RTV = RTVManager->AllocatePersistent(1);
        dev->CreateRenderTargetView(GetResource(), nullptr, RTVManager->GetCPUHandle(m_RTV));
    }
    if((m_Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0)
    {
        m_DSV = DSVManager->AllocatePersistent(1);
        dev->CreateDepthStencilView(GetResource(), nullptr, DSVManager->GetCPUHandle(m_DSV));
    }
}
