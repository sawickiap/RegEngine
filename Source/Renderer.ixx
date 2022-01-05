module;

#include "BaseUtils.hpp"
#include "WindowsUtils.h"
#include "D3d12Utils.hpp"

export module Renderer;

static const D3D_FEATURE_LEVEL MY_D3D_FEATURE_LEVEL = D3D_FEATURE_LEVEL_12_0;
static const uint32_t FRAME_COUNT = 3;
static const DXGI_FORMAT RENDER_TARGET_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
static const int SIZE_X = 1024; // TODO: unify with same in Main.cpp
static const int SIZE_Y = 576; // TODO: unify with same in Main.cpp

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

	void CreateDevice();
	void LoadCapabilities();
	void CreateCommandQueues();
	void CreateSwapChain();
	void CreateFrameResources();
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

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle{m_swapChainRtvDescriptors->GetCPUDescriptorHandleForHeapStart(),
		(INT)m_frameIndex, m_capabilities.m_descriptorSize_RTV};

	float time = (float)GetTickCount() * 1e-3f;
	float color = sin(time) * 0.5f + 0.5f;
	float pos = fmod(time * 100.f, (float)SIZE_X);

	const float clearRGBA[] = {color, 0.0f, 0.0f, 1.0f};
	cmdList->ClearRenderTargetView(rtvDescriptorHandle, clearRGBA, 0, nullptr);

	const float whiteRGBA[] = {1.0f, 1.0f, 1.0f, 1.0f};
	const D3D12_RECT clearRect = {(int)pos, 32, (int)pos + 32, 32 + 32};
	cmdList->ClearRenderTargetView(rtvDescriptorHandle, whiteRGBA, 1, &clearRect);

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
	
	CHECK_HR(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), &m_fence));
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

	HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	CHECK_BOOL(fenceEvent);
	m_fenceEvent.reset(fenceEvent);

	D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtvHandle = m_swapChainRtvDescriptors->GetCPUDescriptorHandleForHeapStart();
	for(uint32_t i = 0; i < FRAME_COUNT; ++i)
	{
		CHECK_HR(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
			&m_frameResources[i].m_cmdAllocator));
		CHECK_HR(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_frameResources[i].m_cmdAllocator.Get(), NULL,
			__uuidof(ID3D12CommandList), &m_frameResources[i].m_cmdList));
		CHECK_HR(m_frameResources[i].m_cmdList->Close());
		CHECK_HR(m_swapChain->GetBuffer(i, __uuidof(ID3D12Resource), &m_frameResources[i].m_backBuffer));
		m_device->CreateRenderTargetView(m_frameResources[i].m_backBuffer.Get(), nullptr, backBufferRtvHandle);
		
		backBufferRtvHandle.ptr += m_capabilities.m_descriptorSize_RTV;
	}
}
