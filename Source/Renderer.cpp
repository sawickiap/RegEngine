#include "BaseUtils.hpp"
#include "Renderer.hpp"
#include "Settings.hpp"
#include <pix3.h>
#include <WICTextureLoader.h>
#include "../ThirdParty/WinFontRender/WinFontRender.h"

static const D3D_FEATURE_LEVEL MY_D3D_FEATURE_LEVEL = D3D_FEATURE_LEVEL_12_0;
static const DXGI_FORMAT RENDER_TARGET_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;

extern VecSetting<glm::uvec2> g_size;

static UintSetting g_frameCount(SettingCategory::Startup, "FrameCount", 3);
static StringSetting g_fontFaceName(SettingCategory::Startup, "Font.FaceName", L"Sagoe UI");
static IntSetting g_fontHeight(SettingCategory::Startup, "Font.Height", 32);
static UintSetting g_fontFlags(SettingCategory::Startup, "Font.Flags", 0);

static BoolSetting g_usePixEvents(SettingCategory::Load, "UsePixEvents", true);
static UintSetting g_syncInterval(SettingCategory::Load, "SyncInterval", 1);

static Renderer* g_renderer;

class PixEventScope
{
public:
    // Warning! msg is actually a formatting string, so don't use '%'!
    PixEventScope(ID3D12GraphicsCommandList* cmdList, const wstr_view& msg) :
        m_cmdList{cmdList}
    {
        if(g_usePixEvents.GetValue())
            PIXBeginEvent(m_cmdList, 0, msg.c_str());
    }
    ~PixEventScope()
    {
        if(g_usePixEvents.GetValue())
            PIXEndEvent(m_cmdList);
    }

private:
    ID3D12GraphicsCommandList* m_cmdList;
};

#define HELPER_CAT_1(a, b) a ## b
#define HELPER_CAT_2(a, b) HELPER_CAT_1(a, b)
#define VAR_NAME_WITH_LINE(name) HELPER_CAT_2(name, __LINE__)
#define PIX_EVENT_SCOPE(cmdList, msg) PixEventScope VAR_NAME_WITH_LINE(pixEventScope)((cmdList), (msg));

class Font
{
public:
    void Init();
    ~Font();

private:
    static const DXGI_FORMAT FORMAT = DXGI_FORMAT_R8_UNORM;

    WinFontRender::CFont m_WinFont;
    ComPtr<ID3D12Resource> m_Texture;
};

void Font::Init()
{
    ERR_TRY;

    // Create WinFontRender object.
    WinFontRender::SFontDesc fontDesc = {};
    fontDesc.FaceName = g_fontFaceName.GetValue();
    fontDesc.Height = g_fontHeight.GetValue();
    fontDesc.Flags = g_fontFlags.GetValue();
    CHECK_BOOL(m_WinFont.Init(fontDesc));

    // Fetch texture data.
    WinFontRender::uvec2 textureDataSize = WinFontRender::uvec2(0, 0);
    size_t textureDataRowPitch = SIZE_MAX;
    const void* textureDataPtr = nullptr;
    m_WinFont.GetTextureData(textureDataPtr, textureDataSize, textureDataRowPitch);

    // Create source buffer.
    const uint32_t srcBufRowPitch = std::max<uint32_t>(textureDataSize.x, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    const uint32_t srcBufSize = srcBufRowPitch * textureDataSize.y;
    ComPtr<ID3D12Resource> srcBuf;
    {
        const CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(srcBufSize);
        CD3DX12_HEAP_PROPERTIES heapProps{D3D12_HEAP_TYPE_UPLOAD};
        CHECK_HR(g_renderer->GetDevice()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, // pOptimizedClearValue
            IID_PPV_ARGS(&srcBuf)));
        SetD3d12ObjectName(srcBuf, L"Font texture source buffer");
    }

    // Fill source buffer.
    {
        CD3DX12_RANGE readEmptyRange{0, 0};
        void* srcBufMappedPtr = nullptr;
        CHECK_HR(srcBuf->Map(
            0, // Subresource
            &readEmptyRange, // pReadRange
            &srcBufMappedPtr));
        for(uint32_t y = 0; y < textureDataSize.y; ++y)
        {
            memcpy(srcBufMappedPtr, textureDataPtr, textureDataSize.x);
            textureDataPtr = (char*)textureDataPtr + textureDataSize.x;
            srcBufMappedPtr = (char*)srcBufMappedPtr + textureDataSize.x;
        }
        srcBuf->Unmap(0, // Subresource
            nullptr); // pWrittenRange
    }
    m_WinFont.FreeTextureData();

    // Create destination texture.
    {
        CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            FORMAT, // format
            textureDataSize.x, // width
            textureDataSize.y, // height
            1, // arraySize
            1); // mipLevels
        CD3DX12_HEAP_PROPERTIES heapProps{D3D12_HEAP_TYPE_DEFAULT};
        CHECK_HR(g_renderer->GetDevice()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr, // pOptimizedClearValue
            IID_PPV_ARGS(&m_Texture)));
        SetD3d12ObjectName(m_Texture, L"Font");
    }

    // Copy the data.
    {
        auto cmdList = g_renderer->BeginUploadCommandList();

        CD3DX12_TEXTURE_COPY_LOCATION dst{m_Texture.Get()};
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT srcFootprint = {0, // Offset
            {FORMAT, textureDataSize.x, textureDataSize.y, 1, srcBufRowPitch}};
        CD3DX12_TEXTURE_COPY_LOCATION src{srcBuf.Get(), srcFootprint};
        CD3DX12_BOX srcBox{
            0, 0, 0, // Left, Top, Front
            (LONG)textureDataSize.x, (LONG)textureDataSize.y, 1}; // Right, Bottom, Back

        cmdList->CopyTextureRegion(&dst,
            0, 0, 0, // DstX, DstY, DstZ
            &src, &srcBox);

        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_Texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmdList->ResourceBarrier(1, &barrier);

        g_renderer->CompleteUploadCommandList();
    }

    // Setup texture SRV descriptor
    g_renderer->SetTexture(m_Texture.Get());
    
    ERR_CATCH_FUNC;
}

Font::~Font()
{
}

Renderer::Renderer(IDXGIFactory4* dxgiFactory, IDXGIAdapter1* adapter, HWND wnd) :
	m_dxgiFactory{dxgiFactory},
	m_adapter{adapter},
	m_wnd(wnd)
{
    g_renderer = this;
}

void Renderer::Init()
{
    ERR_TRY;
    CHECK_BOOL(g_frameCount.GetValue() >= 2 && g_frameCount.GetValue() <= MAX_FRAME_COUNT);
	CreateDevice();
	LoadCapabilities();
	CreateCommandQueues();
	CreateSwapChain();
	CreateFrameResources();
	CreateResources();
    ERR_CATCH_MSG(L"Failed to initialize renderer.");
}

Renderer::~Renderer()
{
    g_renderer = nullptr;

    if(m_cmdQueue)
    {
	    try
	    {
		    m_cmdQueue->Signal(m_fence.Get(), m_nextFenceValue);
            WaitForFenceOnCpu(m_nextFenceValue);
	    }
	    CATCH_PRINT_ERROR(;);
    }
}

ID3D12GraphicsCommandList* Renderer::BeginUploadCommandList()
{
    WaitForFenceOnCpu(m_uploadCmdListSubmittedFenceValue);
    CHECK_HR(m_uploadCmdList->Reset(m_cmdAllocator.Get(), NULL));
    return m_uploadCmdList.Get();
}

void Renderer::CompleteUploadCommandList()
{
    CHECK_HR(m_uploadCmdList->Close());

	ID3D12CommandList* const baseCmdList = m_uploadCmdList.Get();
	m_cmdQueue->ExecuteCommandLists(1, &baseCmdList);

	m_uploadCmdListSubmittedFenceValue = m_nextFenceValue++;
	CHECK_HR(m_cmdQueue->Signal(m_fence.Get(), m_uploadCmdListSubmittedFenceValue));

    WaitForFenceOnCpu(m_uploadCmdListSubmittedFenceValue);
}

void Renderer::SetTexture(ID3D12Resource* res)
{
    m_device->CreateShaderResourceView(res,
        nullptr, // pDesc
        m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
}

void Renderer::Render()
{
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	FrameResources& frameRes = m_frameResources[m_frameIndex];
	ID3D12GraphicsCommandList* const cmdList = frameRes.m_cmdList.Get();

    WaitForFenceOnCpu(frameRes.m_submittedFenceValue);

	CHECK_HR(cmdList->Reset(m_cmdAllocator.Get(), NULL));
    {
        PIX_EVENT_SCOPE(cmdList, L"FRAME");

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

        D3D12_VIEWPORT viewport = {0.f, 0.f, (float)g_size.GetValue().x, (float)g_size.GetValue().y, 0.f, 1.f};
        cmdList->RSSetViewports(1, &viewport);

	    const D3D12_RECT scissorRect = {0, 0, (LONG)g_size.GetValue().x, (LONG)g_size.GetValue().y};
	    cmdList->RSSetScissorRects(1, &scissorRect);

	    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        const mat4x4 world = glm::rotate(glm::identity<mat4x4>(), time, vec3(0.f, 0.f, 1.f));
        const mat4x4 view = glm::lookAtLH(
            vec3(0.f, 2.f, 0.5f), // eye
            vec3(0.f), // center
            vec3(0.f, 0.f, 1.f)); // up
        const mat4x4 proj = glm::perspectiveFovLH(
            glm::radians(80.0f), // fov
            (float)g_size.GetValue().x, (float)g_size.GetValue().y,
            0.5f, // zNear
            100.f); // zFar
        mat4x4 worldViewProj = proj * view * world;

        ID3D12DescriptorHeap* const descriptorHeap = m_descriptorHeap.Get();
        cmdList->SetDescriptorHeaps(1, &descriptorHeap);
        cmdList->SetGraphicsRootDescriptorTable(1, m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());

        cmdList->SetGraphicsRoot32BitConstants(0, 16, glm::value_ptr(worldViewProj), 0);

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
    }
	CHECK_HR(cmdList->Close());

	ID3D12CommandList* const baseCmdList = cmdList;
	m_cmdQueue->ExecuteCommandLists(1, &baseCmdList);

	frameRes.m_submittedFenceValue = m_nextFenceValue++;
	CHECK_HR(m_cmdQueue->Signal(m_fence.Get(), frameRes.m_submittedFenceValue));

	CHECK_HR(m_swapChain->Present(g_syncInterval.GetValue(), 0));
}

void Renderer::CreateDevice()
{
    CHECK_HR( D3D12CreateDevice(m_adapter, MY_D3D_FEATURE_LEVEL, IID_PPV_ARGS(&m_device)) );
    SetD3d12ObjectName(m_device, L"Device");
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
    SetD3d12ObjectName(m_cmdQueue, L"Main command queue");
	
	CHECK_HR(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), &m_fence));
    SetD3d12ObjectName(m_fence, L"Main fence");

	CHECK_HR(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
		&m_cmdAllocator));
    SetD3d12ObjectName(m_cmdAllocator, L"Command allocator");

	CHECK_HR(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_cmdAllocator.Get(), NULL,
		IID_PPV_ARGS(&m_uploadCmdList)));
	CHECK_HR(m_uploadCmdList->Close());
    SetD3d12ObjectName(m_uploadCmdList, L"Upload command list");
}

void Renderer::CreateSwapChain()
{
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = g_frameCount.GetValue();
	swapChainDesc.BufferDesc.Width = g_size.GetValue().x;
	swapChainDesc.BufferDesc.Height = g_size.GetValue().y;
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
	desc.NumDescriptors = g_frameCount.GetValue();
	CHECK_HR(m_device->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), &m_swapChainRtvDescriptors));
    SetD3d12ObjectName(m_swapChainRtvDescriptors, L"Swap chain RTV descriptors");

	HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	CHECK_BOOL(fenceEvent);
	m_fenceEvent.reset(fenceEvent);

	D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtvHandle = m_swapChainRtvDescriptors->GetCPUDescriptorHandleForHeapStart();
	for(uint32_t i = 0; i < g_frameCount.GetValue(); ++i)
	{
		CHECK_HR(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_cmdAllocator.Get(), NULL,
			IID_PPV_ARGS(&m_frameResources[i].m_cmdList)));
		CHECK_HR(m_frameResources[i].m_cmdList->Close());
        SetD3d12ObjectName(m_frameResources[i].m_cmdList, Format(L"Command list %u", i).c_str());

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
    ERR_TRY;

	// Root signature
	{
        D3D12_ROOT_PARAMETER params[2] = {};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[0].Constants.ShaderRegister = 0;
        params[0].Constants.Num32BitValues = 16;
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        CD3DX12_DESCRIPTOR_RANGE textureDescriptorRange{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0};
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[1].DescriptorTable.NumDescriptorRanges = 1;
        params[1].DescriptorTable.pDescriptorRanges = &textureDescriptorRange;
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_STATIC_SAMPLER_DESC staticSampler{0};

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = 2;
		desc.NumStaticSamplers = 0;
        desc.pParameters = params;
        desc.NumStaticSamplers = 1;
        desc.pStaticSamplers = &staticSampler;
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
		ComPtr<ID3DBlob> blob;
		CHECK_HR(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr));
		CHECK_HR(m_device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
			__uuidof(ID3D12RootSignature), &m_rootSignature));
        SetD3d12ObjectName(m_rootSignature, L"Test root signature");
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
        SetD3d12ObjectName(m_pipelineState, L"Test pipeline state");
	}

    // Descriptor heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 1;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        CHECK_HR(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_descriptorHeap)));
        SetD3d12ObjectName(m_descriptorHeap, L"Descriptor heap");
    }

    // Font
    {
        m_Font = std::make_unique<Font>();
        m_Font->Init();
    }

    ERR_CATCH_FUNC;
}

void Renderer::WaitForFenceOnCpu(UINT64 value)
{
	if(m_fence->GetCompletedValue() < value)
	{
		CHECK_HR(m_fence->SetEventOnCompletion(value, m_fenceEvent.get()));
		WaitForSingleObject(m_fenceEvent.get(), INFINITE);
	}
}
