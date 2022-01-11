module;

#include "BaseUtils.hpp"
#include "WindowsUtils.h"
#include "MathUtils.hpp"
#include "D3d12Utils.hpp"
#include <pix3.h>

export module Renderer;

import BaseUtils;
import WindowsUtils;

static const D3D_FEATURE_LEVEL MY_D3D_FEATURE_LEVEL = D3D_FEATURE_LEVEL_12_0;
static const uint32_t FRAME_COUNT = 3;
static const DXGI_FORMAT RENDER_TARGET_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
static const int SIZE_X = 1024; // TODO: unify with same in Main.cpp
static const int SIZE_Y = 576; // TODO: unify with same in Main.cpp

export class PixEventScope
{
public:
    // Warning! msg is actually a formatting string, so don't use '%'!
    PixEventScope(ID3D12GraphicsCommandList* cmdList, const wstr_view& msg) :
        m_cmdList{cmdList}
    {
        PIXBeginEvent(m_cmdList, 0, msg.c_str());
    }
    ~PixEventScope()
    {
        PIXEndEvent(m_cmdList);
    }

private:
    ID3D12GraphicsCommandList* m_cmdList;
};

#define HELPER_CAT_1(a, b) a ## b
#define HELPER_CAT_2(a, b) HELPER_CAT_1(a, b)
#define VAR_NAME_WITH_LINE(name) HELPER_CAT_2(name, __LINE__)
#define PIX_EVENT_SCOPE(cmdList, msg) PixEventScope VAR_NAME_WITH_LINE(pixEventScope)((cmdList), (msg));

export class Renderer
{
public:
	Renderer(IDXGIFactory4* dxgiFactory, IDXGIAdapter1* adapter, HWND wnd);
	void Init();
	~Renderer();

	void Render();

private:
	struct FrameResources
	{
		ComPtr<ID3D12CommandAllocator> m_cmdAllocator;
		ComPtr<ID3D12GraphicsCommandList> m_cmdList;
		ComPtr<ID3D12Resource> m_backBuffer;
		UINT64 m_submittedFenceValue = 0;
	};

	struct Capabilities
	{
		UINT m_descriptorSize_CVB_SRV_UAV = UINT32_MAX;
		UINT m_descriptorSize_sampler = UINT32_MAX;
		UINT m_descriptorSize_RTV = UINT32_MAX;
		UINT m_descriptorSize_DSV = UINT32_MAX;
	};

	IDXGIFactory4* const m_dxgiFactory;
	IDXGIAdapter1* const m_adapter;
	const HWND m_wnd;

	ComPtr<ID3D12Device> m_device;
	Capabilities m_capabilities;
	ComPtr<ID3D12CommandQueue> m_cmdQueue;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_nextFenceValue = 1;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12DescriptorHeap> m_swapChainRtvDescriptors;
	std::array<FrameResources, FRAME_COUNT> m_frameResources;
	UINT m_frameIndex = UINT32_MAX;
	unique_ptr<HANDLE, CloseHandleDeleter> m_fenceEvent;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_pipelineState;

	void CreateDevice();
	void LoadCapabilities();
	void CreateCommandQueues();
	void CreateSwapChain();
	void CreateFrameResources();
	void CreateResources();
};

Renderer::Renderer(IDXGIFactory4* dxgiFactory, IDXGIAdapter1* adapter, HWND wnd) :
	m_dxgiFactory{dxgiFactory},
	m_adapter{adapter},
	m_wnd(wnd)
{
}

void Renderer::Init()
{
	CreateDevice();
	LoadCapabilities();
	CreateCommandQueues();
	CreateSwapChain();
	CreateFrameResources();
	CreateResources();
}

Renderer::~Renderer()
{
	try
	{
		m_cmdQueue->Signal(m_fence.Get(), m_nextFenceValue);
		CHECK_HR(m_fence->SetEventOnCompletion(m_nextFenceValue, m_fenceEvent.get()));
		WaitForSingleObject(m_fenceEvent.get(), INFINITE);
	}
	CATCH_PRINT_ERROR(;);
}

void Renderer::Render()
{
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	FrameResources& frameRes = m_frameResources[m_frameIndex];
	ID3D12GraphicsCommandList* const cmdList = frameRes.m_cmdList.Get();

    PIX_EVENT_SCOPE(cmdList, L"FRAME");

	if(m_fence->GetCompletedValue() < frameRes.m_submittedFenceValue)
	{
		CHECK_HR(m_fence->SetEventOnCompletion(frameRes.m_submittedFenceValue, m_fenceEvent.get()));
		WaitForSingleObject(m_fenceEvent.get(), INFINITE);
	}

	CHECK_HR(frameRes.m_cmdAllocator->Reset());
	CHECK_HR(cmdList->Reset(frameRes.m_cmdAllocator.Get(), NULL));

	CD3DX12_RESOURCE_BARRIER presentToRenderTargetBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		frameRes.m_backBuffer.Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	cmdList->ResourceBarrier(1, &presentToRenderTargetBarrier);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle{
		m_swapChainRtvDescriptors->GetCPUDescriptorHandleForHeapStart(),
		(INT)m_frameIndex, m_capabilities.m_descriptorSize_RTV};

    float time = (float)GetTickCount() * 1e-3f;
	float color = sin(time) * 0.5f + 0.5f;
	//float pos = fmod(time * 100.f, (float)SIZE_X);
    {
        PIX_EVENT_SCOPE(cmdList, L"Clear");
	    const float clearRGBA[] = {color, 0.0f, 0.0f, 1.0f};
	    cmdList->ClearRenderTargetView(rtvDescriptorHandle, clearRGBA, 0, nullptr);
    }

	cmdList->OMSetRenderTargets(1, &rtvDescriptorHandle, TRUE, nullptr);

	cmdList->SetPipelineState(m_pipelineState.Get());

	cmdList->SetGraphicsRootSignature(m_rootSignature.Get());

    D3D12_VIEWPORT viewport = {0.f, 0.f, (float)SIZE_X, (float)SIZE_Y, 0.f, 1.f};
    cmdList->RSSetViewports(1, &viewport);

	const D3D12_RECT scissorRect = {0, 0, SIZE_X, SIZE_Y};
	cmdList->RSSetScissorRects(1, &scissorRect);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    vec2 shift = vec2(glm::mod(time, 1.f), glm::mod(time, 1.f));

    cmdList->SetGraphicsRoot32BitConstants(0, 2, glm::value_ptr(shift), 0);

	cmdList->DrawInstanced(3, 1, 0, 0);

	/*
	const float whiteRGBA[] = {1.0f, 1.0f, 1.0f, 1.0f};
	const D3D12_RECT clearRect = {(int)pos, 32, (int)pos + 32, 32 + 32};
	cmdList->ClearRenderTargetView(rtvDescriptorHandle, whiteRGBA, 1, &clearRect);
	*/

	CD3DX12_RESOURCE_BARRIER renderTargetToPresentBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		frameRes.m_backBuffer.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);
	cmdList->ResourceBarrier(1, &renderTargetToPresentBarrier);
	
	CHECK_HR(cmdList->Close());

	ID3D12CommandList* const baseCmdList = cmdList;
	m_cmdQueue->ExecuteCommandLists(1, &baseCmdList);

	frameRes.m_submittedFenceValue = m_nextFenceValue++;
	CHECK_HR(m_cmdQueue->Signal(m_fence.Get(), frameRes.m_submittedFenceValue));

	CHECK_HR(m_swapChain->Present(1, 0));
}

void Renderer::CreateDevice()
{
    CHECK_HR( D3D12CreateDevice(m_adapter, MY_D3D_FEATURE_LEVEL, IID_PPV_ARGS(&m_device)) );
    m_device->SetName(L"Device");
}

void Renderer::LoadCapabilities()
{
	m_capabilities.m_descriptorSize_CVB_SRV_UAV = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_capabilities.m_descriptorSize_sampler = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	m_capabilities.m_descriptorSize_RTV = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_capabilities.m_descriptorSize_DSV = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

void Renderer::CreateCommandQueues()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	//queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;
	CHECK_HR(m_device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), &m_cmdQueue));
    m_cmdQueue->SetName(L"Main command queue");
	
	CHECK_HR(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), &m_fence));
    m_fence->SetName(L"Main fence");
}

void Renderer::CreateSwapChain()
{
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = FRAME_COUNT;
	swapChainDesc.BufferDesc.Width = SIZE_X;
	swapChainDesc.BufferDesc.Height = SIZE_Y;
	swapChainDesc.BufferDesc.Format = RENDER_TARGET_FORMAT;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;//DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.OutputWindow = m_wnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = TRUE;

	ComPtr<IDXGISwapChain> swapChain;
	CHECK_HR(m_dxgiFactory->CreateSwapChain(m_cmdQueue.Get(), &swapChainDesc, &swapChain));
	CHECK_HR(swapChain->QueryInterface<IDXGISwapChain3>(&m_swapChain));
}

void Renderer::CreateFrameResources()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc.NumDescriptors = FRAME_COUNT;
	CHECK_HR(m_device->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), &m_swapChainRtvDescriptors));
    m_swapChainRtvDescriptors->SetName(L"Swap chain RTV descriptors");

	HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	CHECK_BOOL(fenceEvent);
	m_fenceEvent.reset(fenceEvent);

	D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtvHandle = m_swapChainRtvDescriptors->GetCPUDescriptorHandleForHeapStart();
	for(uint32_t i = 0; i < FRAME_COUNT; ++i)
	{
		CHECK_HR(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
			&m_frameResources[i].m_cmdAllocator));
        m_frameResources[i].m_cmdAllocator->SetName(Format(L"Command allocator %u", i).c_str());

		CHECK_HR(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_frameResources[i].m_cmdAllocator.Get(), NULL,
			__uuidof(ID3D12CommandList), &m_frameResources[i].m_cmdList));
        m_frameResources[i].m_cmdList->SetName(Format(L"Command list %u", i).c_str());
		CHECK_HR(m_frameResources[i].m_cmdList->Close());

		CHECK_HR(m_swapChain->GetBuffer(i, __uuidof(ID3D12Resource), &m_frameResources[i].m_backBuffer));

		m_device->CreateRenderTargetView(m_frameResources[i].m_backBuffer.Get(), nullptr, backBufferRtvHandle);
		
		backBufferRtvHandle.ptr += m_capabilities.m_descriptorSize_RTV;
	}
}

static void SetDefaultRasterizerDesc(D3D12_RASTERIZER_DESC& outDesc)
{
    outDesc.FillMode = D3D12_FILL_MODE_SOLID;
    outDesc.CullMode = D3D12_CULL_MODE_NONE;
    outDesc.FrontCounterClockwise = FALSE;
    outDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    outDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    outDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    outDesc.DepthClipEnable = TRUE;
    outDesc.MultisampleEnable = FALSE;
    outDesc.AntialiasedLineEnable = FALSE;
    outDesc.ForcedSampleCount = 0;
    outDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
}

static void SetDefaultBlendDesc(D3D12_BLEND_DESC& outDesc)
{
    outDesc.AlphaToCoverageEnable = FALSE;
    outDesc.IndependentBlendEnable = FALSE;
    const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
        FALSE,FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL };
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        outDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
}

static void SetDefaultDepthStencilDesc(D3D12_DEPTH_STENCIL_DESC& outDesc)
{
    outDesc.DepthEnable = FALSE;
    outDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    outDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    outDesc.StencilEnable = FALSE;
    outDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    outDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp = {
        D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
    outDesc.FrontFace = defaultStencilOp;
    outDesc.BackFace = defaultStencilOp;
}

void Renderer::CreateResources()
{
	// Root signature
	{
        D3D12_ROOT_PARAMETER param = {};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        param.Constants.ShaderRegister = 0;
        param.Constants.Num32BitValues = 4;
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = 1;
        desc.pParameters = &param;
		desc.NumStaticSamplers = 0;
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
		ComPtr<ID3DBlob> blob;
		CHECK_HR(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr));
		CHECK_HR(m_device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
			__uuidof(ID3D12RootSignature), &m_rootSignature));
        m_rootSignature->SetName(L"Test root signature");
	}

	// Pipeline state
	{
		std::vector<char> vsCode = LoadFile(L"Data\\VS");
		std::vector<char> psCode = LoadFile(L"Data\\PS");

		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		//desc.InputLayout.NumElements = 0;
		//desc.InputLayout.pInputElementDescs = 
		desc.pRootSignature = m_rootSignature.Get();
		desc.VS.BytecodeLength = vsCode.size();
		desc.VS.pShaderBytecode = vsCode.data();
		desc.PS.BytecodeLength = psCode.size();
		desc.PS.pShaderBytecode = psCode.data();
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = RENDER_TARGET_FORMAT;
		desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleMask = UINT32_MAX;
		SetDefaultRasterizerDesc(desc.RasterizerState);
		SetDefaultBlendDesc(desc.BlendState);
		SetDefaultDepthStencilDesc(desc.DepthStencilState);
		CHECK_HR(m_device->CreateGraphicsPipelineState(&desc, __uuidof(ID3D12PipelineState), &m_pipelineState));
        m_pipelineState->SetName(L"Test pipeline state");
	}
}
