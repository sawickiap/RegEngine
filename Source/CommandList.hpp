#pragma once

#include <initializer_list>

class RenderingResource;

constexpr size_t RENDER_TARGET_MAX_COUNT = 8;

/*
Represents ID3D12GraphicsCommandList during command recording.
*/
class CommandList
{
public:
    void Init(
        ID3D12CommandAllocator* cmdAllocator,
        ID3D12GraphicsCommandList* cmdList);
    void Execute(ID3D12CommandQueue* cmdQueue);
    
    ID3D12GraphicsCommandList* GetCmdList() const { return m_CmdList; }

    // Warning! msg is actually a formatting string, so don't use '%'!
    void BeginPIXEvent(const wstr_view& msg);
    void EndPIXEvent();

    // # States that are tracked internally to remove redundant calls.
    void SetPipelineState(ID3D12PipelineState* pipelineState);
    void SetRootSignature(ID3D12RootSignature* rootSignature);
    void SetViewport(const D3D12_VIEWPORT& viewport);
    void SetScissorRect(const D3D12_RECT& scissorRect);
    void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY primitiveTopology);
    // Any of render-target or depth-stencil pointers can be null.
    void SetRenderTargets(
        RenderingResource* depthStencil,
        std::initializer_list<RenderingResource*> renderTargets);
    void SetRenderTargets(
        RenderingResource* depthStencil,
        RenderingResource* renderTarget);

private:
    ID3D12GraphicsCommandList* m_CmdList = nullptr;
    struct State
    {
        ID3D12PipelineState* m_PipelineState = nullptr;
        ID3D12RootSignature* m_RootSignature = nullptr;
        D3D12_VIEWPORT m_Viewport = CD3DX12_VIEWPORT(FLT_MIN, FLT_MIN, FLT_MAX, FLT_MAX);
        D3D12_RECT m_ScissorRect = CD3DX12_RECT(LONG_MIN, LONG_MIN, LONG_MAX, LONG_MAX);
        D3D12_PRIMITIVE_TOPOLOGY m_PritimitveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    } m_State;
};

class PIXEventScope
{
public:
    // Warning! msg is actually a formatting string, so don't use '%'!
    PIXEventScope(CommandList& cmdList, const wstr_view& msg) :
        m_CmdList{cmdList}
    {
        m_CmdList.BeginPIXEvent(msg);
    }
    ~PIXEventScope()
    {
        m_CmdList.EndPIXEvent();
    }

private:
    CommandList& m_CmdList;
};
