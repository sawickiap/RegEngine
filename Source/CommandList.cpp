#include "BaseUtils.hpp"
#include "CommandList.hpp"
#include "Settings.hpp"
#include <pix3.h>

static BoolSetting g_UsePIXEvents(SettingCategory::Load, "UsePIXEvents", true);

void CommandList::Init(
    ID3D12CommandAllocator* cmdAllocator,
    ID3D12GraphicsCommandList* cmdList)
{
    assert(m_CmdList == nullptr);
    m_CmdList = cmdList;

    CHECK_HR(m_CmdList->Reset(cmdAllocator, nullptr));
}

void CommandList::Execute(ID3D12CommandQueue* cmdQueue)
{
    CHECK_HR(m_CmdList->Close());
    ID3D12CommandList* cmdListBase = m_CmdList;
	cmdQueue->ExecuteCommandLists(1, &cmdListBase);
    m_CmdList = nullptr;
    m_State = State{};
}

void CommandList::BeginPIXEvent(const wstr_view& msg)
{
    if(g_UsePIXEvents.GetValue())
        PIXBeginEvent(m_CmdList, 0, msg.c_str());
}

void CommandList::EndPIXEvent()
{
    if(g_UsePIXEvents.GetValue())
        PIXEndEvent(m_CmdList);
}

void CommandList::SetPipelineState(ID3D12PipelineState* pipelineState)
{
    assert(m_CmdList);
    if(pipelineState != m_State.m_PipelineState)
    {
        m_CmdList->SetPipelineState(pipelineState);
        m_State.m_PipelineState = pipelineState;
    }
}

void CommandList::SetRootSignature(ID3D12RootSignature* rootSignature)
{
    assert(m_CmdList);
    if(rootSignature != m_State.m_RootSignature)
    {
        m_CmdList->SetGraphicsRootSignature(rootSignature);
        m_State.m_RootSignature = rootSignature;
    }
}

void CommandList::SetViewport(const D3D12_VIEWPORT& viewport)
{
    assert(m_CmdList);
    if(viewport != m_State.m_Viewport)
    {
        m_CmdList->RSSetViewports(1, &viewport);
        m_State.m_Viewport = viewport;
    }
}

void CommandList::SetScissorRect(const D3D12_RECT& scissorRect)
{
    assert(m_CmdList);
    if(scissorRect.left != m_State.m_ScissorRect.left ||
        scissorRect.top != m_State.m_ScissorRect.top ||
        scissorRect.right != m_State.m_ScissorRect.right ||
        scissorRect.bottom != m_State.m_ScissorRect.bottom)
    {
        m_CmdList->RSSetScissorRects(1, &scissorRect);
        m_State.m_ScissorRect = scissorRect;
    }
}

void CommandList::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY primitiveTopology)
{
    assert(m_CmdList);
    if(primitiveTopology != m_State.m_PritimitveTopology)
    {
        m_CmdList->IASetPrimitiveTopology(primitiveTopology);
        m_State.m_PritimitveTopology = primitiveTopology;
    }
}

void CommandList::SetDescriptorHeaps(
    ID3D12DescriptorHeap* descriptorHeap_CBV_SRV_UAV,
    ID3D12DescriptorHeap* descriptorHeap_Sampler)
{
    assert(m_CmdList);
    if(descriptorHeap_CBV_SRV_UAV != m_State.m_DescriptorHeap_CBV_SRV_UAV ||
        descriptorHeap_Sampler != m_State.m_DescriptorHeap_Sampler)
    {
        m_State.m_DescriptorHeap_CBV_SRV_UAV = descriptorHeap_CBV_SRV_UAV;
        m_State.m_DescriptorHeap_Sampler = descriptorHeap_Sampler;
        /*
        Documentation of SetDescriptorHeaps says:

        Only one descriptor heap of each type can be set at one time, which means a
        maximum of 2 heaps (one sampler, one CBV/SRV/UAV) can be set at one time.
        All previously set heaps are unset by the call. At most one heap of each
        shader-visible type can be set in the call.
        */
        if(m_State.m_DescriptorHeap_CBV_SRV_UAV && m_State.m_DescriptorHeap_Sampler)
        {
            ID3D12DescriptorHeap* heaps[] = {m_State.m_DescriptorHeap_CBV_SRV_UAV, m_State.m_DescriptorHeap_Sampler};
            m_CmdList->SetDescriptorHeaps(2, heaps);
        }
        else if(m_State.m_DescriptorHeap_CBV_SRV_UAV)
            m_CmdList->SetDescriptorHeaps(1, &m_State.m_DescriptorHeap_CBV_SRV_UAV);
        else if(m_State.m_DescriptorHeap_Sampler)
            m_CmdList->SetDescriptorHeaps(1, &m_State.m_DescriptorHeap_Sampler);
        else
            m_CmdList->SetDescriptorHeaps(0, nullptr);
    }
}
