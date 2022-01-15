#pragma once

static const uint32_t MAX_FRAME_COUNT = 10;

class Font;
class CommandList;

class Renderer
{
public:
	Renderer(IDXGIFactory4* dxgiFactory, IDXGIAdapter1* adapter, HWND wnd);
	void Init();
	~Renderer();

    ID3D12Device* GetDevice() { return m_Device.Get(); };
    ID3D12CommandAllocator* GetCmdAllocator() { return m_CmdAllocator.Get(); }
    void BeginUploadCommandList(CommandList& dstCmdList);
    // Closes, submits, and waits for the upload command list on the CPU to finish.
    void CompleteUploadCommandList(CommandList& cmdList);
    void SetTexture(ID3D12Resource* res);

	void Render();

private:
	struct FrameResources
	{
		ComPtr<ID3D12GraphicsCommandList> m_CmdList;
		ComPtr<ID3D12Resource> m_BackBuffer;
		UINT64 m_SubmittedFenceValue = 0;
	};

	struct Capabilities
	{
		UINT m_DescriptorSize_CVB_SRV_UAV = UINT32_MAX;
		UINT m_DescriptorSize_Sampler = UINT32_MAX;
		UINT m_DescriptorSize_RTV = UINT32_MAX;
		UINT m_DescriptorSize_DSV = UINT32_MAX;
	};

	IDXGIFactory4* const m_DXGIFactory;
	IDXGIAdapter1* const m_Adapter;
	const HWND m_Wnd;

	ComPtr<ID3D12Device> m_Device;
	Capabilities m_Capabilities;
	ComPtr<ID3D12CommandQueue> m_CmdQueue;
    ComPtr<ID3D12CommandAllocator> m_CmdAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_UploadCmdList;
    UINT64 m_UploadCmdListSubmittedFenceValue = 0;
	ComPtr<ID3D12Fence> m_Fence;
	UINT64 m_NextFenceValue = 1;
	ComPtr<IDXGISwapChain3> m_SwapChain;
	ComPtr<ID3D12DescriptorHeap> m_SwapChainRtvDescriptors;
	std::array<FrameResources, MAX_FRAME_COUNT> m_FrameResources;
	UINT m_FrameIndex = UINT32_MAX;
	unique_ptr<HANDLE, CloseHandleDeleter> m_FenceEvent;
	ComPtr<ID3D12RootSignature> m_RootSignature;
	ComPtr<ID3D12PipelineState> m_PipelineState;
    unique_ptr<Font> m_Font;
    ComPtr<ID3D12DescriptorHeap> m_DescriptorHeap;
    ComPtr<ID3D12Resource> m_Texture;

	void CreateDevice();
	void LoadCapabilities();
	void CreateCommandQueues();
	void CreateSwapChain();
	void CreateFrameResources();
	void CreateResources();
    void LoadTexture();

    void WaitForFenceOnCPU(UINT64 value);
};
