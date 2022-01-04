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

private:
	struct FrameResources
	{
		ComPtr<ID3D12CommandAllocator> m_cmdAllocator;
		ComPtr<ID3D12GraphicsCommandList> m_cmdList;
	};

	IDXGIFactory4* const m_dxgiFactory;
	IDXGIAdapter1* const m_adapter;
	const HWND m_wnd;

	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12CommandQueue> m_cmdQueue;
	ComPtr<IDXGISwapChain> m_swapChain;
	std::array<FrameResources, FRAME_COUNT> m_frameResources;

	void CreateDevice();
	void CreateCommandQueues();
	void CreateSwapChain();
	void CreateCommandAllocators();
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
	CreateCommandQueues();
	CreateSwapChain();
	//CreateSwapchainRenderTargets();
	CreateCommandAllocators();
}

Renderer::~Renderer()
{
}

void Renderer::CreateDevice()
{
    CHECK_HR( D3D12CreateDevice(
        m_adapter,
        MY_D3D_FEATURE_LEVEL,
        IID_PPV_ARGS(&m_device)) );
}

void Renderer::CreateCommandQueues()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	//queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;

	CHECK_HR(m_device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), &m_cmdQueue));
}

void Renderer::CreateSwapChain()
{
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = FRAME_COUNT;
	swapChainDesc.BufferDesc.Width = SIZE_X;
	swapChainDesc.BufferDesc.Height = SIZE_Y;
	swapChainDesc.BufferDesc.Format = RENDER_TARGET_FORMAT;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.OutputWindow = m_wnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = TRUE;

	CHECK_HR(m_dxgiFactory->CreateSwapChain(m_device.Get(), &swapChainDesc, &m_swapChain));
}

void Renderer::CreateCommandAllocators()
{
	for(uint32_t i = 0; i < FRAME_COUNT; ++i)
	{
		CHECK_HR(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
			&m_frameResources[i].m_cmdAllocator));
		CHECK_HR(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_frameResources[i].m_cmdAllocator.Get(), NULL,
			__uuidof(ID3D12CommandList), &m_frameResources[i].m_cmdList));
	}
}
