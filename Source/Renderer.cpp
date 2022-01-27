#include "BaseUtils.hpp"
#include "CommandList.hpp"
#include "RenderingResource.hpp"
#include "Texture.hpp"
#include "Mesh.hpp"
#include "Descriptors.hpp"
#include "ConstantBuffers.hpp"
#include "Renderer.hpp"
#include "Settings.hpp"
#include "AssimpUtils.hpp"
#include "../ThirdParty/WinFontRender/WinFontRender.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/LogStream.hpp>
#include <assimp/Logger.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/vector2.h>
#include <assimp/vector3.h>

static const D3D_FEATURE_LEVEL MY_D3D_FEATURE_LEVEL = D3D_FEATURE_LEVEL_12_0;
static const DXGI_FORMAT RENDER_TARGET_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
static const DXGI_FORMAT DEPTH_STENCIL_FORMAT = DXGI_FORMAT_D32_FLOAT;

extern VecSetting<glm::uvec2> g_Size;

enum ASSIMP_LOG_SEVERITY
{
    ASSIMP_LOG_SEVERITY_NONE,
    ASSIMP_LOG_SEVERITY_ERROR,
    ASSIMP_LOG_SEVERITY_WARNING,
    ASSIMP_LOG_SEVERITY_INFO,
    ASSIMP_LOG_SEVERITY_DEBUG,
};

UintSetting g_FrameCount(SettingCategory::Startup, "FrameCount", 3);
static StringSetting g_FontFaceName(SettingCategory::Startup, "Font.FaceName", L"Sagoe UI");
static IntSetting g_FontHeight(SettingCategory::Startup, "Font.Height", 32);
static UintSetting g_FontFlags(SettingCategory::Startup, "Font.Flags", 0);
static UintSetting g_AssimpLogSeverity(SettingCategory::Startup, "Assimp.LogSeverity", 3);
static StringSetting g_AssimpLogFilePath(SettingCategory::Startup, "Assimp.LogFilePath");

static UintSetting g_SyncInterval(SettingCategory::Load, "SyncInterval", 1);
static StringSetting g_AssimpModelPath(SettingCategory::Load, "Assimp.ModelPath");
static FloatSetting g_AssimpScale(SettingCategory::Load, "Assimp.Scale", 1.f);

Renderer* g_Renderer;

#define HELPER_CAT_1(a, b) a ## b
#define HELPER_CAT_2(a, b) HELPER_CAT_1(a, b)
#define VAR_NAME_WITH_LINE(name) HELPER_CAT_2(name, __LINE__)
#define PIX_EVENT_SCOPE(cmdList, msg) PIXEventScope VAR_NAME_WITH_LINE(pixEventScope)((cmdList), (msg));

struct PerFrameConstants
{
    uint32_t m_FrameIndex;
    float m_SceneTime;
    uint32_t _padding0[2];
};

struct PerObjectConstants
{
    packed_mat4 m_WorldViewProj;
};

enum ROOT_PARAM
{
    ROOT_PARAM_PER_FRAME_CBV,
    ROOT_PARAM_PER_OBJECT_CBV,
    ROOT_PARAM_TEXTURE_SRV,
};

// Source: "Reversed-Z in OpenGL", https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/
static mat4 MakeInfReversedZProjLH(float fovY_radians, float aspectWbyH, float zNear)
{
    float f = 1.0f / tan(fovY_radians / 2.0f);
    return mat4(
        f / aspectWbyH, 0.0f,  0.0f,  0.0f,
        0.0f,    f,  0.0f,  0.0f,
        0.0f, 0.0f,  0.0f,  1.0f,
        0.0f, 0.0f, zNear,  0.0f);
}

class AssimpInit
{
public:
    AssimpInit();
    ~AssimpInit();
};

class Font
{
public:
    void Init();
    ~Font();
    Texture* GetTexture() const { return m_Texture.get(); }

private:
    static const DXGI_FORMAT FORMAT = DXGI_FORMAT_R8_UNORM;

    WinFontRender::CFont m_WinFont;
    unique_ptr<Texture> m_Texture;
};

class AssimpLogStream : public Assimp::LogStream
{
public:
    void write(const char* message) override;
};
static AssimpLogStream g_AssimpLogStream;

AssimpInit::AssimpInit()
{
    using namespace Assimp;
    uint32_t errorSeverity = 0;
    Logger::LogSeverity logSeverity = Logger::NORMAL;

    switch(g_AssimpLogSeverity.GetValue())
    {
    case ASSIMP_LOG_SEVERITY_NONE:
        break;
    case ASSIMP_LOG_SEVERITY_ERROR:
        errorSeverity = Logger::Err;
        break;
    case ASSIMP_LOG_SEVERITY_WARNING:
        errorSeverity = Logger::Err | Logger::Warn;
        break;
    case ASSIMP_LOG_SEVERITY_INFO:
        errorSeverity = Logger::Err | Logger::Warn | Logger::Info;
        break;
    case ASSIMP_LOG_SEVERITY_DEBUG:
        errorSeverity = Logger::Err | Logger::Warn | Logger::Debugging;
        logSeverity = Logger::VERBOSE;
        break;
    default:
        assert(0);
    }

    Assimp::DefaultLogger::create(ConvertUnicodeToChars(g_AssimpLogFilePath.GetValue(), CP_ACP).c_str(), logSeverity);
    Assimp::DefaultLogger::get()->attachStream(&g_AssimpLogStream, errorSeverity);
}

AssimpInit::~AssimpInit()
{
    // Crashes with error:
    // HEAP[RegEngine.exe]: Invalid address specified to RtlValidateHeap( 00000265EA700000, 00007FF667F24038 )
    //Assimp::DefaultLogger::kill();
}

void AssimpLogStream::write(const char* message)
{
    LogInfo(ConvertCharsToUnicode(message, CP_ACP));
}

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

    CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        FORMAT, // format
        textureDataSize.x, // width
        textureDataSize.y, // height
        1, // arraySize
        1); // mipLevels
    m_Texture = std::make_unique<Texture>();
    D3D12_SUBRESOURCE_DATA subresourceData = {};
    subresourceData.pData = textureDataPtr;
    subresourceData.RowPitch = textureDataRowPitch;
    m_Texture->LoadFromMemory(resDesc, subresourceData, L"Font texture");

    m_WinFont.FreeTextureData();

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
    CreateMemoryAllocator();
	LoadCapabilities();
    m_ShaderResourceDescriptorManager = std::make_unique<ShaderResourceDescriptorManager>();
    m_ShaderResourceDescriptorManager->Init();
    m_TemporaryConstantBufferManager = std::make_unique<TemporaryConstantBufferManager>();
    m_TemporaryConstantBufferManager->Init();
	CreateCommandQueues();
	CreateSwapChain();
	CreateFrameResources();
	CreateResources();
    m_AssimpInit = std::make_unique<AssimpInit>();
    LoadModel();
    LoadTexture();

    ERR_CATCH_MSG(L"Failed to initialize renderer.");
}

Renderer::~Renderer()
{
    g_Renderer = nullptr;

    if(m_TextureSRVDescriptor)
    {
        m_ShaderResourceDescriptorManager->FreePersistentDescriptor(*m_TextureSRVDescriptor);
        m_TextureSRVDescriptor.reset();
    }
    if(m_FontTextureSRVDescriptor)
    {
        m_ShaderResourceDescriptorManager->FreePersistentDescriptor(*m_FontTextureSRVDescriptor);
        m_FontTextureSRVDescriptor.reset();
    }

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

void Renderer::BeginUploadCommandList(CommandList& dstCmdList)
{
    WaitForFenceOnCPU(m_UploadCmdListSubmittedFenceValue);
    dstCmdList.Init(m_CmdAllocator.Get(), m_UploadCmdList.Get());
}

void Renderer::CompleteUploadCommandList(CommandList& cmdList)
{
    cmdList.Execute(m_CmdQueue.Get());

	m_UploadCmdListSubmittedFenceValue = m_NextFenceValue++;
	CHECK_HR(m_CmdQueue->Signal(m_Fence.Get(), m_UploadCmdListSubmittedFenceValue));

    WaitForFenceOnCPU(m_UploadCmdListSubmittedFenceValue);
}

void Renderer::Render()
{
	m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
	FrameResources& frameRes = m_FrameResources[m_FrameIndex];
    WaitForFenceOnCPU(frameRes.m_SubmittedFenceValue);

    m_ShaderResourceDescriptorManager->NewFrame();
    m_TemporaryConstantBufferManager->NewFrame();

    CommandList cmdList;
    cmdList.Init(m_CmdAllocator.Get(), frameRes.m_CmdList.Get());
    {
        PIX_EVENT_SCOPE(cmdList, L"FRAME");

        cmdList.SetDescriptorHeaps(m_ShaderResourceDescriptorManager->GetDescriptorHeap(), nullptr);

        frameRes.m_BackBuffer->SetStates(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

        float time = (float)GetTickCount() * 1e-3f;
	    float color = sin(time) * 0.5f + 0.5f;
	    //float pos = fmod(time * 100.f, (float)SIZE_X);
        const D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE{
		    m_SwapChainRTVDescriptors->GetCPUDescriptorHandleForHeapStart(),
		    (INT)m_FrameIndex, m_Capabilities.m_DescriptorSize_RTV};
        const D3D12_CPU_DESCRIPTOR_HANDLE dsvDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE{
            m_DSVDescriptor->GetCPUDescriptorHandleForHeapStart()};

        {
            PIX_EVENT_SCOPE(cmdList, L"Clear");

	        const float clearRGBA[] = {color, 0.0f, 0.0f, 1.0f};
	        cmdList.GetCmdList()->ClearRenderTargetView(rtvDescriptorHandle, clearRGBA, 0, nullptr);
            
            cmdList.GetCmdList()->ClearDepthStencilView(
                dsvDescriptorHandle,
                D3D12_CLEAR_FLAG_DEPTH,
                0.f, // depth
                0, // stencil
                0, nullptr); // NumRects, prects
        }

	    cmdList.GetCmdList()->OMSetRenderTargets(1, &rtvDescriptorHandle, TRUE, &dsvDescriptorHandle);

	    cmdList.SetPipelineState(m_PipelineState.Get());

	    cmdList.SetRootSignature(m_RootSignature.Get());

        // Set per-frame constants.
        {
            PerFrameConstants myData = {};
            myData.m_FrameIndex = m_FrameIndex;
            myData.m_SceneTime = time;

            void* mappedPtr = nullptr;
            D3D12_GPU_DESCRIPTOR_HANDLE descriptorHandle;
            m_TemporaryConstantBufferManager->CreateBuffer(sizeof(myData), mappedPtr, descriptorHandle);
            memcpy(mappedPtr, &myData, sizeof(myData));

            cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(ROOT_PARAM_PER_FRAME_CBV, descriptorHandle);
        }
        
        D3D12_VIEWPORT viewport = {0.f, 0.f, (float)g_Size.GetValue().x, (float)g_Size.GetValue().y, 0.f, 1.f};
        cmdList.SetViewport(viewport);

	    const D3D12_RECT scissorRect = {0, 0, (LONG)g_Size.GetValue().x, (LONG)g_Size.GetValue().y};
	    cmdList.SetScissorRect(scissorRect);

        const mat4 world = glm::rotate(glm::identity<mat4>(), time, vec3(0.f, 0.f, 1.f));
        const mat4 view = glm::lookAtLH(
            vec3(0.f, 3.f, 0.5f), // eye
            vec3(0.f), // center
            vec3(0.f, 0.f, 1.f)); // up
        const mat4 proj = MakeInfReversedZProjLH(
            glm::radians(80.f),
            (float)g_Size.GetValue().x / (float)g_Size.GetValue().y,
            0.5f); // zNear
        //const mat4 worldViewProj = proj * view * world;
        m_ViewProj = proj * view * world;

        // A) Testing texture SRV descriptors as persistent.
        /*
        const D3D12_GPU_DESCRIPTOR_HANDLE textureDescriptorHandle = fmod(time, 0.5f) > 0.25f ?
            m_ShaderResourceDescriptorManager->GetDescriptorGPUHandle(*m_FontTextureSRVDescriptor) :
            m_ShaderResourceDescriptorManager->GetDescriptorGPUHandle(*m_TextureSRVDescriptor);
        cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(ROOT_PARAM_TEXTURE_SRV, textureDescriptorHandle);
        */
        // B) Testing texture SRV descriptors as temporary.
        {
            ShaderResourceDescriptor textureSRVDescriptor = m_ShaderResourceDescriptorManager->AllocateTemporaryDescriptor(1);
            ID3D12Resource* const textureRes = /*fmod(time, 1.f) > 0.5f ?
                m_Font->GetTexture()->GetResource() :*/
                m_Texture->GetResource();
            m_Device->CreateShaderResourceView(textureRes, nullptr,
                m_ShaderResourceDescriptorManager->GetDescriptorCPUHandle(textureSRVDescriptor));
            cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(ROOT_PARAM_TEXTURE_SRV,
                m_ShaderResourceDescriptorManager->GetDescriptorGPUHandle(textureSRVDescriptor));
        }

        vec3 scaleVec = vec3(g_AssimpScale.GetValue());
        mat4 globalXform = glm::scale(glm::identity<mat4>(), scaleVec);
        //globalXform = glm::rotate(globalXform, glm::half_pi<float>(), vec3(1.f, 0.f, 0.f));
        RenderEntity(cmdList, globalXform, m_RootEntity);

	    /*
	    const float whiteRGBA[] = {1.0f, 1.0f, 1.0f, 1.0f};
	    const D3D12_RECT clearRect = {(int)pos, 32, (int)pos + 32, 32 + 32};
	    cmdList->ClearRenderTargetView(rtvDescriptorHandle, whiteRGBA, 1, &clearRect);
	    */

        frameRes.m_BackBuffer->SetStates(cmdList, D3D12_RESOURCE_STATE_PRESENT);
    }
    cmdList.Execute(m_CmdQueue.Get());

	frameRes.m_SubmittedFenceValue = m_NextFenceValue++;
	CHECK_HR(m_CmdQueue->Signal(m_Fence.Get(), frameRes.m_SubmittedFenceValue));

	CHECK_HR(m_SwapChain->Present(g_SyncInterval.GetValue(), 0));
}

void Renderer::CreateDevice()
{
    CHECK_HR( D3D12CreateDevice(m_Adapter, MY_D3D_FEATURE_LEVEL, IID_PPV_ARGS(&m_Device)) );
    SetD3D12ObjectName(m_Device, L"Device");
}

void Renderer::CreateMemoryAllocator()
{
    assert(m_Device && m_Adapter && !m_MemoryAllocator);

    D3D12MA::ALLOCATOR_DESC desc = {};
    desc.pDevice = m_Device.Get();
    desc.pAdapter = m_Adapter;
    CHECK_HR(D3D12MA::CreateAllocator(&desc, &m_MemoryAllocator));
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
	CHECK_HR(m_Device->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), &m_SwapChainRTVDescriptors));
    SetD3D12ObjectName(m_SwapChainRTVDescriptors, L"Swap chain RTV descriptors");

	HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	CHECK_BOOL(fenceEvent);
	m_FenceEvent.reset(fenceEvent);

	D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtvHandle = m_SwapChainRTVDescriptors->GetCPUDescriptorHandleForHeapStart();
	for(uint32_t i = 0; i < g_FrameCount.GetValue(); ++i)
	{
		CHECK_HR(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_CmdAllocator.Get(), NULL,
			IID_PPV_ARGS(&m_FrameResources[i].m_CmdList)));
		CHECK_HR(m_FrameResources[i].m_CmdList->Close());
        SetD3D12ObjectName(m_FrameResources[i].m_CmdList, Format(L"Command list %u", i).c_str());

        ID3D12Resource* backBuffer = nullptr;
		CHECK_HR(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));
        m_FrameResources[i].m_BackBuffer = std::make_unique<RenderingResource>();
        m_FrameResources[i].m_BackBuffer->InitExternallyOwned(backBuffer, D3D12_RESOURCE_STATE_PRESENT);

		m_Device->CreateRenderTargetView(m_FrameResources[i].m_BackBuffer->GetResource(), nullptr, backBufferRtvHandle);
		
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
    outDesc.DepthEnable = TRUE;
    outDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    outDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
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

    // Depth texture and DSV
    {
        const D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            DEPTH_STENCIL_FORMAT,
            g_Size.GetValue().x, g_Size.GetValue().y,
            1, // arraySize
            1, // mipLevels
            1, // sampleCount
            0, // sampleQuality
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = DEPTH_STENCIL_FORMAT;
        clearValue.DepthStencil.Depth = 0.f;
        m_DepthTexture = std::make_unique<RenderingResource>();
        m_DepthTexture->Init(resDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, L"Depth", &clearValue);

	    D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	    descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	    descHeapDesc.NumDescriptors = 1;
	    CHECK_HR(m_Device->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&m_DSVDescriptor)));
        SetD3D12ObjectName(m_DSVDescriptor, L"DSV descriptor");

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = resDesc.Format;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        m_Device->CreateDepthStencilView(m_DepthTexture->GetResource(), &dsvDesc, m_DSVDescriptor->GetCPUDescriptorHandleForHeapStart());
    }

    /*
    {
        const Vertex vertices[] = {
            // X-Z plane
            { {0.f, 0.f, 0.f}, {0.f, 1.f}, {1.f, 1.f, 1.f, 1.f} },
            { {1.f, 0.f, 0.f}, {1.f, 1.f}, {1.f, 1.f, 1.f, 1.f} },
            { {0.f, 0.f, 1.f}, {0.f, 0.f}, {1.f, 1.f, 1.f, 1.f} },
            { {1.f, 0.f, 1.f}, {1.f, 0.f}, {1.f, 1.f, 1.f, 1.f} },
            // Y-Z plane
            { {0, 0.f, 0.f}, {0.f, 1.f}, {1.f, 1.f, 1.f, 1.f} },
            { {0, 1.f, 0.f}, {1.f, 1.f}, {1.f, 1.f, 1.f, 1.f} },
            { {0, 0.f, 1.f}, {0.f, 0.f}, {1.f, 1.f, 1.f, 1.f} },
            { {0, 1.f, 1.f}, {1.f, 0.f}, {1.f, 1.f, 1.f, 1.f} },
        };
        const uint16_t indices[] = {
            // X-Z plane
            0, 1, 2, 2, 1, 3,
            // Y-Z plane
            4, 5, 6, 6, 5, 7,
        };

        m_Mesh = std::make_unique<Mesh>();
        m_Mesh->Init(
            D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
            vertices, (uint32_t)_countof(vertices),
            indices, (uint32_t)_countof(indices));
    }
    */

	// Root signature
	{
        D3D12_ROOT_PARAMETER params[3] = {};

        const CD3DX12_DESCRIPTOR_RANGE perFrameCBVDescRange{D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0};
        params[ROOT_PARAM_PER_FRAME_CBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[ROOT_PARAM_PER_FRAME_CBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[ROOT_PARAM_PER_FRAME_CBV].DescriptorTable.NumDescriptorRanges = 1;
        params[ROOT_PARAM_PER_FRAME_CBV].DescriptorTable.pDescriptorRanges = &perFrameCBVDescRange;

        const CD3DX12_DESCRIPTOR_RANGE perObjectCBVDescRange{D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1};
        params[ROOT_PARAM_PER_OBJECT_CBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[ROOT_PARAM_PER_OBJECT_CBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        params[ROOT_PARAM_PER_OBJECT_CBV].DescriptorTable.NumDescriptorRanges = 1;
        params[ROOT_PARAM_PER_OBJECT_CBV].DescriptorTable.pDescriptorRanges = &perObjectCBVDescRange;

        const CD3DX12_DESCRIPTOR_RANGE textureSRVDescRange{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0};
        params[ROOT_PARAM_TEXTURE_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[ROOT_PARAM_TEXTURE_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        params[ROOT_PARAM_TEXTURE_SRV].DescriptorTable.NumDescriptorRanges = 1;
        params[ROOT_PARAM_TEXTURE_SRV].DescriptorTable.pDescriptorRanges = &textureSRVDescRange;

        CD3DX12_STATIC_SAMPLER_DESC staticSampler{0};

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = (uint32_t)_countof(params);
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
		desc.InputLayout.NumElements = Vertex::GetInputElementCount();
		desc.InputLayout.pInputElementDescs = Vertex::GetInputElements();
		desc.pRootSignature = m_RootSignature.Get();
		desc.VS.BytecodeLength = vsCode.size();
		desc.VS.pShaderBytecode = vsCode.data();
		desc.PS.BytecodeLength = psCode.size();
		desc.PS.pShaderBytecode = psCode.data();
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = RENDER_TARGET_FORMAT;
		desc.DSVFormat = DEPTH_STENCIL_FORMAT;
		desc.SampleDesc.Count = 1;
		desc.SampleMask = UINT32_MAX;
		SetDefaultRasterizerDesc(desc.RasterizerState);
		SetDefaultBlendDesc(desc.BlendState);
		SetDefaultDepthStencilDesc(desc.DepthStencilState);
		CHECK_HR(m_Device->CreateGraphicsPipelineState(&desc, __uuidof(ID3D12PipelineState), &m_PipelineState));
        SetD3D12ObjectName(m_PipelineState, L"Test pipeline state");
	}

    // Font
    {
        m_Font = std::make_unique<Font>();
        m_Font->Init();
    }

    m_FontTextureSRVDescriptor = std::make_unique<ShaderResourceDescriptor>(
        m_ShaderResourceDescriptorManager->AllocatePersistentDescriptor(1));
    m_Device->CreateShaderResourceView(
        m_Font->GetTexture()->GetResource(),
        nullptr, // pDesc
        m_ShaderResourceDescriptorManager->GetDescriptorCPUHandle(*m_FontTextureSRVDescriptor));

    ERR_CATCH_FUNC;
}

void Renderer::LoadModel()
{
    const wstr_view filePath = g_AssimpModelPath.GetValue();
    LogMessageF(L"Loading model from \"%.*hs\"...", STR_TO_FORMAT(filePath));
    ERR_TRY;

    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            ConvertUnicodeToChars(filePath, CP_ACP).c_str(),
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_SortByPType |
            aiProcess_FlipUVs); // To make UV space origin in the upper-left corner.
        // aiProcess_CalcTangentSpace
        // aiProcess_FlipWindingOrder
        // aiProcess_MakeLeftHanded
        if(!scene)
            FAIL(ConvertCharsToUnicode(importer.GetErrorString(), CP_ACP));
        
        PrintAssimpSceneInfo(scene);

        for(uint32_t i = 0; i < scene->mNumMeshes; ++i)
            LoadModelMesh(scene, scene->mMeshes[i]);
        
        const aiNode* node = scene->mRootNode;
        if(node)
            LoadModelNode(m_RootEntity, scene, node);

        // Parse material for texture path.
        {
            CHECK_BOOL(scene->mNumMaterials == 1);
            const aiMaterial* mat = scene->mMaterials[0];
            CHECK_BOOL(mat->GetTextureCount(aiTextureType_DIFFUSE) == 1);
            aiString path;
            aiTextureMapping mapping;
            uint32_t uvindex;
            float blend;
            aiTextureOp op;
            aiTextureMapMode mapmodes[3];
            const aiReturn ret = mat->GetTexture(aiTextureType_DIFFUSE, 0,
                &path, &mapping, &uvindex, &blend, &op, mapmodes);
            CHECK_BOOL(ret == aiReturn_SUCCESS);
            
            const std::filesystem::path modelDir = std::filesystem::path(filePath.begin(), filePath.end()).parent_path();
            m_TexturePath = modelDir / path.C_Str();
        }
    }

    ERR_CATCH_MSG(Format(L"Cannot load model from \"%.*hs\".", STR_TO_FORMAT(filePath)));
}

void Renderer::LoadModelNode(Entity& outEntity, const aiScene* scene, const aiNode* node)
{
    // Matrix is Assimp is also right-to-left ordered (translation in last column, vectors are column, transform is mat * vec),
    // but it is stored as row-major instead of column-major like in GLM.
    outEntity.m_Transform = glm::transpose(glm::make_mat4(&node->mTransformation.a1));

    for(uint32_t i = 0; i < node->mNumMeshes; ++i)
        outEntity.m_Meshes.push_back((size_t)node->mMeshes[i]);
    
    for(uint32_t i = 0; i < node->mNumChildren; ++i)
    {
        unique_ptr<Entity> childEntity = std::make_unique<Entity>();
        LoadModelNode(*childEntity, scene, node->mChildren[i]);
        outEntity.m_Children.push_back(std::move(childEntity));
    }
}

void Renderer::LoadModelMesh(const aiScene* scene, const aiMesh* assimpMesh)
{
    const uint32_t vertexCount = assimpMesh->mNumVertices;
    const uint32_t faceCount = assimpMesh->mNumFaces;
    CHECK_BOOL(vertexCount > 0 && faceCount > 0);
    CHECK_BOOL(assimpMesh->mPrimitiveTypes == aiPrimitiveType_TRIANGLE);
    CHECK_BOOL(assimpMesh->HasTextureCoords(0) || !assimpMesh->HasTextureCoords(1));
    CHECK_BOOL(assimpMesh->mNumUVComponents[0] == 2 && assimpMesh->mNumUVComponents[1] == 0);

    std::vector<Vertex> vertices(vertexCount);
    for(uint32_t i = 0; i < vertexCount; ++i)
    {
        const aiVector3D pos = assimpMesh->mVertices[i];
        const aiVector3D texCoord = assimpMesh->mTextureCoords[0][i];
        vertices[i].m_Position = packed_vec3(pos.x, pos.y, pos.z);
        vertices[i].m_TexCoord = packed_vec2(texCoord.x, texCoord.y);
        vertices[i].m_Color = packed_vec4(1.f, 1.f, 1.f, 1.f);
    }
    
    std::vector<Mesh::IndexType> indices(faceCount * 3);
    uint32_t indexIndex = 0;
    for(uint32_t i = 0; i < faceCount; ++i)
    {
        assert(assimpMesh->mFaces[i].mNumIndices == 3);
        for(uint32_t j = 0; j < 3; ++j)
        {
            assert(assimpMesh->mFaces[i].mIndices[j] <= std::numeric_limits<Mesh::IndexType>::max());
            indices[indexIndex++] = (Mesh::IndexType)assimpMesh->mFaces[i].mIndices[j];
        }
    }

    unique_ptr<Mesh> rendererMesh = std::make_unique<Mesh>();
    rendererMesh->Init(
        L"Mesh",
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        std::span<const Vertex>(vertices.begin(), vertices.end()),
        std::span<const Mesh::IndexType>(indices.begin(), indices.end()));
    m_Meshes.push_back(std::move(rendererMesh));
}

void Renderer::LoadTexture()
{
    CHECK_BOOL(!m_TexturePath.empty());
    m_Texture = std::make_unique<Texture>();
    m_Texture->LoadFromFile(m_TexturePath.native());

    m_TextureSRVDescriptor = std::make_unique<ShaderResourceDescriptor>(
        m_ShaderResourceDescriptorManager->AllocatePersistentDescriptor(1));
    m_Device->CreateShaderResourceView(
        m_Texture->GetResource(),
        nullptr, // pDesc
        m_ShaderResourceDescriptorManager->GetDescriptorCPUHandle(*m_TextureSRVDescriptor));
}

void Renderer::WaitForFenceOnCPU(UINT64 value)
{
	if(m_Fence->GetCompletedValue() < value)
	{
		CHECK_HR(m_Fence->SetEventOnCompletion(value, m_FenceEvent.get()));
		WaitForSingleObject(m_FenceEvent.get(), INFINITE);
	}
}

void Renderer::RenderEntity(CommandList& cmdList, const mat4& parentXform, const Entity& entity)
{
    const mat4 entityXform = parentXform * entity.m_Transform;

    if(!entity.m_Meshes.empty())
    {
        PerObjectConstants perObjConstants = {};
        perObjConstants.m_WorldViewProj = m_ViewProj * entityXform;

        void* perObjectConstantsPtr = nullptr;
        D3D12_GPU_DESCRIPTOR_HANDLE perObjectConstantsDescriptorHandle;
        m_TemporaryConstantBufferManager->CreateBuffer(sizeof(perObjConstants), perObjectConstantsPtr, perObjectConstantsDescriptorHandle);
        memcpy(perObjectConstantsPtr, &perObjConstants, sizeof(perObjConstants));

        cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(ROOT_PARAM_PER_OBJECT_CBV, perObjectConstantsDescriptorHandle);

        for(size_t meshIndex : entity.m_Meshes)
            RenderEntityMesh(cmdList, entity, meshIndex);
    }

    for(const auto& childEntity : entity.m_Children)
        RenderEntity(cmdList, entityXform, *childEntity);
}

void Renderer::RenderEntityMesh(CommandList& cmdList, const Entity& entity, size_t meshIndex)
{
    const Mesh* const mesh = m_Meshes[meshIndex].get();
	cmdList.SetPrimitiveTopology(mesh->GetTopology());

    const D3D12_VERTEX_BUFFER_VIEW vbView = mesh->GetVertexBufferView();
    cmdList.GetCmdList()->IASetVertexBuffers(0, 1, &vbView);

    if(mesh->HasIndices())
    {
        const D3D12_INDEX_BUFFER_VIEW ibView = mesh->GetIndexBufferView();
        cmdList.GetCmdList()->IASetIndexBuffer(&ibView);
    	cmdList.GetCmdList()->DrawIndexedInstanced(mesh->GetIndexCount(), 1, 0, 0, 0);
    }
    else
    {
        cmdList.GetCmdList()->IASetIndexBuffer(nullptr);
    	cmdList.GetCmdList()->DrawInstanced(mesh->GetVertexCount(), 1, 0, 0);
    }
}
