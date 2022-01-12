#pragma once

static const uint32_t FRAME_COUNT = 3;

class Font;

class Renderer
{
public:
	Renderer(IDXGIFactory4* dxgiFactory, IDXGIAdapter1* adapter, HWND wnd);
	void Init();
	~Renderer();

    ID3D12Device* GetDevice() { return m_device.Get(); };
    ID3D12GraphicsCommandList* BeginUploadCommandList();
    // Closes, submits, and waits for the upload command list on the CPU to finish.
    void CompleteUploadCommandList();

	void Render();

private:
	struct FrameResources
	{
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
    ComPtr<ID3D12CommandAllocator> m_cmdAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_uploadCmdList;
    UINT64 m_uploadCmdListSubmittedFenceValue = 0;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_nextFenceValue = 1;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12DescriptorHeap> m_swapChainRtvDescriptors;
	std::array<FrameResources, FRAME_COUNT> m_frameResources;
	UINT m_frameIndex = UINT32_MAX;
	unique_ptr<HANDLE, CloseHandleDeleter> m_fenceEvent;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_pipelineState;
    unique_ptr<Font> m_Font;

	void CreateDevice();
	void LoadCapabilities();
	void CreateCommandQueues();
	void CreateSwapChain();
	void CreateFrameResources();
	void CreateResources();

    void WaitForFenceOnCpu(UINT64 value);
};
