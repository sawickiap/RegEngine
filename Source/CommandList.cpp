#include "BaseUtils.hpp"
#include "CommandList.hpp"
#include "RenderingResource.hpp"
#include "Descriptors.hpp"
#include "Settings.hpp"
#include "Renderer.hpp"
#include <pix3.h>

static BoolSetting g_UsePIXEvents(SettingCategory::Load, "UsePIXEvents", true);

void CommandList::Init(
    ID3D12CommandAllocator* cmdAllocator,
    ID3D12GraphicsCommandList* cmdList)
{
    assert(g_Renderer);
    assert(m_CmdList == nullptr);
    m_CmdList = cmdList;

    CHECK_HR(m_CmdList->Reset(cmdAllocator, nullptr));

    ID3D12DescriptorHeap* descriptorHeaps[] = {
        g_Renderer->GetSRVDescriptorManager()->GetHeap(),
        g_Renderer->GetSamplerDescriptorManager()->GetHeap()};
    m_CmdList->SetDescriptorHeaps(2, descriptorHeaps);
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

void CommandList::SetRenderTargets(RenderingResource* depthStencil, std::initializer_list<RenderingResource*> renderTargets)
{
    DescriptorManager* const RTVDescriptorManager = g_Renderer->GetRTVDescriptorManager();
    DescriptorManager* const DSVDescriptorManager = g_Renderer->GetDSVDescriptorManager();

    D3D12_CPU_DESCRIPTOR_HANDLE RTVDescriptors[RENDER_TARGET_MAX_COUNT] = {};
    D3D12_CPU_DESCRIPTOR_HANDLE DSVDescriptor = {};

    if(depthStencil)
    {
        assert(depthStencil->HasCurrentState(D3D12_RESOURCE_STATE_DEPTH_WRITE));
        assert(!depthStencil->GetDSV().IsNull());
        DSVDescriptor = DSVDescriptorManager->GetCPUHandle(depthStencil->GetDSV());
    }
    auto RTIt = renderTargets.begin();
    for(size_t i = 0; i < renderTargets.size(); ++i, ++RTIt)
    {
        if(*RTIt)
        {
            assert((*RTIt)->HasCurrentState(D3D12_RESOURCE_STATE_RENDER_TARGET));
            assert(!(*RTIt)->GetRTV().IsNull());
            RTVDescriptors[i] = RTVDescriptorManager->GetCPUHandle((*RTIt)->GetRTV());
        }
    }

    m_CmdList->OMSetRenderTargets(
        (UINT)renderTargets.size(),
        RTVDescriptors,
        FALSE, // RTsSingleHandleToDescriptorRange
        depthStencil ? &DSVDescriptor : nullptr);
}

void CommandList::SetRenderTargets(RenderingResource* depthStencil, RenderingResource* renderTarget)
{
    SetRenderTargets(depthStencil, {renderTarget});
}
