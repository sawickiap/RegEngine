#include "BaseUtils.hpp"
#include "Renderer.hpp"
#include "Settings.hpp"
#include <pix3.h>
#include <WICTextureLoader.h>
#include "../ThirdParty/WinFontRender/WinFontRender.h"

static const D3D_FEATURE_LEVEL MY_D3D_FEATURE_LEVEL = D3D_FEATURE_LEVEL_12_0;
static const DXGI_FORMAT RENDER_TARGET_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;

extern VecSetting<glm::uvec2> g_Size;

static UintSetting g_FrameCount(SettingCategory::Startup, "FrameCount", 3);
static StringSetting g_FontFaceName(SettingCategory::Startup, "Font.FaceName", L"Sagoe UI");
static IntSetting g_FontHeight(SettingCategory::Startup, "Font.Height", 32);
static UintSetting g_FontFlags(SettingCategory::Startup, "Font.Flags", 0);

static BoolSetting g_UsePIXEvents(SettingCategory::Load, "UsePIXEvents", true);
static UintSetting g_SyncInterval(SettingCategory::Load, "SyncInterval", 1);
static StringSetting g_TextureFilePath(SettingCategory::Load, "TextureFilePath");

static Renderer* g_Renderer;

class PIXEventScope
{
public:
    // Warning! msg is actually a formatting string, so don't use '%'!
    PIXEventScope(ID3D12GraphicsCommandList* cmdList, const wstr_view& msg) :
        m_CmdList{cmdList}
    {
        if(g_UsePIXEvents.GetValue())
            PIXBeginEvent(m_CmdList, 0, msg.c_str());
    }
    ~PIXEventScope()
    {
        if(g_UsePIXEvents.GetValue())
            PIXEndEvent(m_CmdList);
    }

private:
    ID3D12GraphicsCommandList* m_CmdList;
};

#define HELPER_CAT_1(a, b) a ## b
#define HELPER_CAT_2(a, b) HELPER_CAT_1(a, b)
#define VAR_NAME_WITH_LINE(name) HELPER_CAT_2(name, __LINE__)
#define PIX_EVENT_SCOPE(cmdList, msg) PIXEventScope VAR_NAME_WITH_LINE(pixEventScope)((cmdList), (msg));

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
    fontDesc.FaceName = g_FontFaceName.GetValue();
    fontDesc.Height = g_FontHeight.GetValue();
    fontDesc.Flags = g_FontFlags.GetValue();
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
        CHECK_HR(g_Renderer->GetDevice()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, // pOptimizedClearValue
            IID_PPV_ARGS(&srcBuf)));
        SetD3D12ObjectName(srcBuf, L"Font texture source buffer");
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
            textureDataPtr = (char*)textureDataPtr + textureDataRowPitch;
            srcBufMappedPtr = (char*)srcBufMappedPtr + srcBufRowPitch;
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
        CHECK_HR(g_Renderer->GetDevice()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr, // pOptimizedClearValue
            IID_PPV_ARGS(&m_Texture)));
        SetD3D12ObjectName(m_Texture, L"Font");
    }

    // Copy the data.
    {
        const auto cmdList = g_Renderer->BeginUploadCommandList();

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

        g_Renderer->CompleteUploadCommandList();
    }

    // Setup texture SRV descriptor
    //g_Renderer->SetTexture(m_Texture.Get());
    
    ERR_CATCH_FUNC;
}

Font::~Font()
{
}

Renderer::Renderer(IDXGIFactory4* dxgiFactory, IDXGIAdapter1* adapter, HWND wnd) :
	m_DXGIFactory{dxgiFactory},
	m_Adapter{adapter},
	m_Wnd(wnd)
{
    g_Renderer = this;
}

void Renderer::Init()
{
    ERR_TRY;
    CHECK_BOOL(g_FrameCount.GetValue() >= 2 && g_FrameCount.GetValue() <= MAX_FRAME_COUNT);
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
    g_Renderer = nullptr;

    if(m_CmdQueue)
    {
	    try
	    {
		    m_CmdQueue->Signal(m_Fence.Get(), m_NextFenceValue);
            WaitForFenceOnCPU(m_NextFenceValue);
	    }
	    CATCH_PRINT_ERROR(;);
    }
}

ID3D12GraphicsCommandList* Renderer::BeginUploadCommandList()
{
    WaitForFenceOnCPU(m_UploadCmdListSubmittedFenceValue);
    CHECK_HR(m_UploadCmdList->Reset(m_CmdAllocator.Get(), NULL));
    return m_UploadCmdList.Get();
}

void Renderer::CompleteUploadCommandList()
{
    CHECK_HR(m_UploadCmdList->Close());

	ID3D12CommandList* const baseCmdList = m_UploadCmdList.Get();
	m_CmdQueue->ExecuteCommandLists(1, &baseCmdList);

	m_UploadCmdListSubmittedFenceValue = m_NextFenceValue++;
	CHECK_HR(m_CmdQueue->Signal(m_Fence.Get(), m_UploadCmdListSubmittedFenceValue));

    WaitForFenceOnCPU(m_UploadCmdListSubmittedFenceValue);
}

void Renderer::SetTexture(ID3D12Resource* res)
{
    m_Device->CreateShaderResourceView(res,
        nullptr, // pDesc
        m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart());
}

void Renderer::Render()
{
	m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
	FrameResources& frameRes = m_FrameResources[m_FrameIndex];
	ID3D12GraphicsCommandList* const cmdList = frameRes.m_CmdList.Get();

    WaitForFenceOnCPU(frameRes.m_SubmittedFenceValue);

	CHECK_HR(cmdList->Reset(m_CmdAllocator.Get(), NULL));
    {
        PIX_EVENT_SCOPE(cmdList, L"FRAME");

        CD3DX12_RESOURCE_BARRIER presentToRenderTargetBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		    frameRes.m_BackBuffer.Get(),
		    D3D12_RESOURCE_STATE_PRESENT,
		    D3D12_RESOURCE_STATE_RENDER_TARGET);
	    cmdList->ResourceBarrier(1, &presentToRenderTargetBarrier);

	    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle{
		    m_SwapChainRtvDescriptors->GetCPUDescriptorHandleForHeapStart(),
		    (INT)m_FrameIndex, m_Capabilities.m_DescriptorSize_RTV};

        float time = (float)GetTickCount() * 1e-3f;
	    float color = sin(time) * 0.5f + 0.5f;
	    //float pos = fmod(time * 100.f, (float)SIZE_X);
        {
            PIX_EVENT_SCOPE(cmdList, L"Clear");
	        const float clearRGBA[] = {color, 0.0f, 0.0f, 1.0f};
	        cmdList->ClearRenderTargetView(rtvDescriptorHandle, clearRGBA, 0, nullptr);
        }

	    cmdList->OMSetRenderTargets(1, &rtvDescriptorHandle, TRUE, nullptr);

	    cmdList->SetPipelineState(m_PipelineState.Get());

	    cmdList->SetGraphicsRootSignature(m_RootSignature.Get());

        D3D12_VIEWPORT viewport = {0.f, 0.f, (float)g_Size.GetValue().x, (float)g_Size.GetValue().y, 0.f, 1.f};
        cmdList->RSSetViewports(1, &viewport);

	    const D3D12_RECT scissorRect = {0, 0, (LONG)g_Size.GetValue().x, (LONG)g_Size.GetValue().y};
	    cmdList->RSSetScissorRects(1, &scissorRect);

	    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        const mat4x4 world = glm::rotate(glm::identity<mat4x4>(), time, vec3(0.f, 0.f, 1.f));
        const mat4x4 view = glm::lookAtLH(
            vec3(0.f, 2.f, 0.5f), // eye
            vec3(0.f), // center
            vec3(0.f, 0.f, 1.f)); // up
        const mat4x4 proj = glm::perspectiveFovLH(
            glm::radians(80.0f), // fov
            (float)g_Size.GetValue().x, (float)g_Size.GetValue().y,
            0.5f, // zNear
            100.f); // zFar
        mat4x4 worldViewProj = proj * view * world;

        ID3D12DescriptorHeap* const descriptorHeap = m_DescriptorHeap.Get();
        cmdList->SetDescriptorHeaps(1, &descriptorHeap);
        cmdList->SetGraphicsRootDescriptorTable(1, m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart());

        cmdList->SetGraphicsRoot32BitConstants(0, 16, glm::value_ptr(worldViewProj), 0);

	    cmdList->DrawInstanced(4, 1, 0, 0);

	    /*
	    const float whiteRGBA[] = {1.0f, 1.0f, 1.0f, 1.0f};
	    const D3D12_RECT clearRect = {(int)pos, 32, (int)pos + 32, 32 + 32};
	    cmdList->ClearRenderTargetView(rtvDescriptorHandle, whiteRGBA, 1, &clearRect);
	    */

	    CD3DX12_RESOURCE_BARRIER renderTargetToPresentBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		    frameRes.m_BackBuffer.Get(),
		    D3D12_RESOURCE_STATE_RENDER_TARGET,
		    D3D12_RESOURCE_STATE_PRESENT);
	    cmdList->ResourceBarrier(1, &renderTargetToPresentBarrier);
    }
	CHECK_HR(cmdList->Close());

	ID3D12CommandList* const baseCmdList = cmdList;
	m_CmdQueue->ExecuteCommandLists(1, &baseCmdList);

	frameRes.m_SubmittedFenceValue = m_NextFenceValue++;
	CHECK_HR(m_CmdQueue->Signal(m_Fence.Get(), frameRes.m_SubmittedFenceValue));

	CHECK_HR(m_SwapChain->Present(g_SyncInterval.GetValue(), 0));
}

void Renderer::CreateDevice()
{
    CHECK_HR( D3D12CreateDevice(m_Adapter, MY_D3D_FEATURE_LEVEL, IID_PPV_ARGS(&m_Device)) );
    SetD3D12ObjectName(m_Device, L"Device");
}

void Renderer::LoadCapabilities()
{
	m_Capabilities.m_DescriptorSize_CVB_SRV_UAV = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_Capabilities.m_DescriptorSize_Sampler = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	m_Capabilities.m_DescriptorSize_RTV = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_Capabilities.m_DescriptorSize_DSV = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

void Renderer::CreateCommandQueues()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	//queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;
	CHECK_HR(m_Device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), &m_CmdQueue));
    SetD3D12ObjectName(m_CmdQueue, L"Main command queue");
	
	CHECK_HR(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), &m_Fence));
    SetD3D12ObjectName(m_Fence, L"Main fence");

	CHECK_HR(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
		&m_CmdAllocator));
    SetD3D12ObjectName(m_CmdAllocator, L"Command allocator");

	CHECK_HR(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_CmdAllocator.Get(), NULL,
		IID_PPV_ARGS(&m_UploadCmdList)));
	CHECK_HR(m_UploadCmdList->Close());
    SetD3D12ObjectName(m_UploadCmdList, L"Upload command list");
}

void Renderer::CreateSwapChain()
{
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = g_FrameCount.GetValue();
	swapChainDesc.BufferDesc.Width = g_Size.GetValue().x;
	swapChainDesc.BufferDesc.Height = g_Size.GetValue().y;
	swapChainDesc.BufferDesc.Format = RENDER_TARGET_FORMAT;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;//DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.OutputWindow = m_Wnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = TRUE;

	ComPtr<IDXGISwapChain> swapChain;
	CHECK_HR(m_DXGIFactory->CreateSwapChain(m_CmdQueue.Get(), &swapChainDesc, &swapChain));
	CHECK_HR(swapChain->QueryInterface<IDXGISwapChain3>(&m_SwapChain));
}

void Renderer::CreateFrameResources()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc.NumDescriptors = g_FrameCount.GetValue();
	CHECK_HR(m_Device->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), &m_SwapChainRtvDescriptors));
    SetD3D12ObjectName(m_SwapChainRtvDescriptors, L"Swap chain RTV descriptors");

	HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	CHECK_BOOL(fenceEvent);
	m_FenceEvent.reset(fenceEvent);

	D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtvHandle = m_SwapChainRtvDescriptors->GetCPUDescriptorHandleForHeapStart();
	for(uint32_t i = 0; i < g_FrameCount.GetValue(); ++i)
	{
		CHECK_HR(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_CmdAllocator.Get(), NULL,
			IID_PPV_ARGS(&m_FrameResources[i].m_CmdList)));
		CHECK_HR(m_FrameResources[i].m_CmdList->Close());
        SetD3D12ObjectName(m_FrameResources[i].m_CmdList, Format(L"Command list %u", i).c_str());

		CHECK_HR(m_SwapChain->GetBuffer(i, __uuidof(ID3D12Resource), &m_FrameResources[i].m_BackBuffer));

		m_Device->CreateRenderTargetView(m_FrameResources[i].m_BackBuffer.Get(), nullptr, backBufferRtvHandle);
		
		backBufferRtvHandle.ptr += m_Capabilities.m_DescriptorSize_RTV;
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
		CHECK_HR(m_Device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
			__uuidof(ID3D12RootSignature), &m_RootSignature));
        SetD3D12ObjectName(m_RootSignature, L"Test root signature");
	}

	// Pipeline state
	{
		std::vector<char> vsCode = LoadFile(L"Data\\VS");
		std::vector<char> psCode = LoadFile(L"Data\\PS");

		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		//desc.InputLayout.NumElements = 0;
		//desc.InputLayout.pInputElementDescs = 
		desc.pRootSignature = m_RootSignature.Get();
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
		CHECK_HR(m_Device->CreateGraphicsPipelineState(&desc, __uuidof(ID3D12PipelineState), &m_PipelineState));
        SetD3D12ObjectName(m_PipelineState, L"Test pipeline state");
	}

    // Descriptor heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 1;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        CHECK_HR(m_Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_DescriptorHeap)));
        SetD3D12ObjectName(m_DescriptorHeap, L"Descriptor heap");
    }

    // Font
    {
        m_Font = std::make_unique<Font>();
        m_Font->Init();
    }

    LoadTexture();

    ERR_CATCH_FUNC;
}

void Renderer::LoadTexture()
{
    m_Texture.Reset();
    if(g_TextureFilePath.GetValue().empty())
        return;
    ERR_TRY;

    LogMessageF(L"Loading texture from \"%.*s\"...", STR_TO_FORMAT(g_TextureFilePath.GetValue()));

    unique_ptr<uint8_t[]> decodedData;
    D3D12_SUBRESOURCE_DATA subresource = {};
    CHECK_HR(DirectX::LoadWICTextureFromFileEx(
        m_Device.Get(),
        g_TextureFilePath.GetValue().c_str(),
        0, // maxSize
        D3D12_RESOURCE_FLAG_NONE,
        DirectX::WIC_LOADER_FORCE_SRGB,
        &m_Texture,
        decodedData,
        subresource));
    CHECK_BOOL(m_Texture && decodedData && subresource.pData && subresource.RowPitch);
    CHECK_BOOL(subresource.pData == decodedData.get());
    SetD3D12ObjectName(m_Texture, Format(L"Texture from file: %.*s", STR_TO_FORMAT(g_TextureFilePath.GetValue())));

    const D3D12_RESOURCE_DESC textureDesc = m_Texture->GetDesc();
    const uint32_t bytesPerPixel = DXGIFormatToBytesPerPixel(textureDesc.Format);
    CHECK_BOOL(bytesPerPixel > 0);
    const uvec2 textureSize = uvec2((uint32_t)textureDesc.Width, (uint32_t)textureDesc.Height);

    // Create source buffer.
    const uint32_t srcBufRowPitch = std::max<uint32_t>(textureSize.x * bytesPerPixel, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    const uint32_t srcBufSize = srcBufRowPitch * textureSize.y;
    ComPtr<ID3D12Resource> srcBuf;
    {
        const CD3DX12_RESOURCE_DESC srcBufDesc = CD3DX12_RESOURCE_DESC::Buffer(srcBufSize);
        CD3DX12_HEAP_PROPERTIES heapProps{D3D12_HEAP_TYPE_UPLOAD};
        CHECK_HR(g_Renderer->GetDevice()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &srcBufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, // pOptimizedClearValue
            IID_PPV_ARGS(&srcBuf)));
        SetD3D12ObjectName(srcBuf, L"Texture source buffer");
    }

    // Map source buffer and memcpy data to it from decodedData.
    {
        const uint8_t* textureDataPtr = decodedData.get();
        CD3DX12_RANGE readEmptyRange{0, 0};
        void* srcBufMappedPtr = nullptr;
        CHECK_HR(srcBuf->Map(
            0, // Subresource
            &readEmptyRange, // pReadRange
            &srcBufMappedPtr));
        for(uint32_t y = 0; y < textureSize.y; ++y)
        {
            memcpy(srcBufMappedPtr, textureDataPtr, textureSize.x * bytesPerPixel);
            textureDataPtr += subresource.RowPitch;
            srcBufMappedPtr = (char*)srcBufMappedPtr + srcBufRowPitch;
        }
        srcBuf->Unmap(0, // Subresource
            nullptr); // pWrittenRange
    }
    decodedData.reset();

    // Copy the data.
    {
        auto cmdList = g_Renderer->BeginUploadCommandList();

        CD3DX12_TEXTURE_COPY_LOCATION dst{m_Texture.Get()};
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT srcFootprint = {0, // Offset
            {textureDesc.Format, (uint32_t)textureDesc.Width, (uint32_t)textureDesc.Height, 1, srcBufRowPitch}};
        CD3DX12_TEXTURE_COPY_LOCATION src{srcBuf.Get(), srcFootprint};
        CD3DX12_BOX srcBox{
            0, 0, 0, // Left, Top, Front
            (LONG)textureDesc.Width, (LONG)textureDesc.Height, 1}; // Right, Bottom, Back

        cmdList->CopyTextureRegion(&dst,
            0, 0, 0, // DstX, DstY, DstZ
            &src, &srcBox);

        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_Texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmdList->ResourceBarrier(1, &barrier);

        g_Renderer->CompleteUploadCommandList();
    }

    // Setup texture SRV descriptor
    g_Renderer->SetTexture(m_Texture.Get());

    ERR_CATCH_MSG(Format(L"Cannot load texture from \"%.*s\".", STR_TO_FORMAT(g_TextureFilePath.GetValue())));
}

void Renderer::WaitForFenceOnCPU(UINT64 value)
{
	if(m_Fence->GetCompletedValue() < value)
	{
		CHECK_HR(m_Fence->SetEventOnCompletion(value, m_FenceEvent.get()));
		WaitForSingleObject(m_FenceEvent.get(), INFINITE);
	}
}
