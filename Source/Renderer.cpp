#include "BaseUtils.hpp"
#include "Renderer.hpp"
#include "CommandList.hpp"
#include "RenderingResource.hpp"
#include "Texture.hpp"
#include "Mesh.hpp"
#include "ConstantBuffers.hpp"
#include "Shaders.hpp"
#include "Cameras.hpp"
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
static BoolSetting g_EnableExperimentalShaderModels(SettingCategory::Startup, "EnableExperimentalShaderModels", true);

static UintSetting g_SRVPersistentDescriptorMaxCount(SettingCategory::Startup,
    "SRVDescriptors.Persistent.MaxCount", 0);
static UintSetting g_SRVTemporaryDescriptorMaxCountPerFrame(SettingCategory::Startup,
    "SRVDescriptors.Temporary.MaxCountPerFrame", 0);
static UintSetting g_SamplerPersistentDescriptorMaxCount(SettingCategory::Startup,
    "SamplerDescriptors.Persistent.MaxCount", 0);
static UintSetting g_SamplerTemporaryDescriptorMaxCountPerFrame(SettingCategory::Startup,
    "SamplerDescriptors.Temporary.MaxCountPerFrame", 0);
static UintSetting g_RTVPersistentDescriptorMaxCount(SettingCategory::Startup,
    "RTVDescriptors.Persistent.MaxCount", 0);
static UintSetting g_DSVPersistentDescriptorMaxCount(SettingCategory::Startup,
    "DSVDescriptors.Persistent.MaxCount", 0);
// 0 = anisotropic filtering disabled, 1..16 = D3D12_SAMPLER_DESC::MaxAnisotropy.
static UintSetting g_MaxAnisotropy(SettingCategory::Startup, "MaxAnisotropy", 16);


static UintSetting g_SyncInterval(SettingCategory::Load, "SyncInterval", 1);
static StringSetting g_AssimpModelPath(SettingCategory::Load, "Assimp.ModelPath");
static StringSetting g_TexturePath(SettingCategory::Load, "TexturePath");
static FloatSetting g_AssimpScale(SettingCategory::Load, "Assimp.Scale", 1.f);
static MatSetting<mat4> g_AssimpTransform(SettingCategory::Load, "Assimp.Transform", glm::identity<mat4>());
static UintSetting g_BackFaceCullingMode(SettingCategory::Load, "BackFaceCullingMode", 0);
static VecSetting<vec3> g_DirectionToLight(SettingCategory::Load, "DirectionToLight", vec3(0.f, 1.f, 0.f));
static VecSetting<vec3> g_LightColor(SettingCategory::Load, "LightColor", vec3(0.9f));
static VecSetting<vec3> g_AmbientColor(SettingCategory::Load, "AmbientColor", vec3(0.1f));

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
    
    packed_vec3 m_DirToLight_View;
    uint32_t _padding1;
    
    packed_vec3 m_LightColor;
    uint32_t _padding2;
    
    packed_vec3 m_AmbientColor;
    uint32_t _padding3;
};

struct PerObjectConstants
{
    packed_mat4 m_WorldViewProj;
    packed_mat4 m_WorldView;
};

enum ROOT_PARAM
{
    ROOT_PARAM_PER_FRAME_CBV,
    ROOT_PARAM_PER_OBJECT_CBV,
    ROOT_PARAM_TEXTURE_SRV,
    ROOT_PARAM_SAMPLER,
    ROOT_PARAM_COUNT
};

enum POSTPROCESSING_ROOT_PARAM
{
    POSTPROCESSING_ROOT_PARAM_TEXTURE,
    POSTPROCESSING_ROOT_PARAM_COUNT
};

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
        return;
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
    // Crashes with error, I don't know why :(
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

    LogMessage(L"Initializing renderer...\n");

    CHECK_BOOL(g_FrameCount.GetValue() >= 2 && g_FrameCount.GetValue() <= MAX_FRAME_COUNT);
    CHECK_BOOL(g_SRVPersistentDescriptorMaxCount.GetValue() > 0);
    CHECK_BOOL(g_SRVTemporaryDescriptorMaxCountPerFrame.GetValue() > 0);
    CHECK_BOOL(g_SamplerPersistentDescriptorMaxCount.GetValue() > 0);
    CHECK_BOOL(g_SamplerTemporaryDescriptorMaxCountPerFrame.GetValue() > 0);
    CHECK_BOOL(g_RTVPersistentDescriptorMaxCount.GetValue() > 0);
    CHECK_BOOL(g_DSVPersistentDescriptorMaxCount.GetValue() > 0);

    if(g_EnableExperimentalShaderModels.GetValue())
        D3D12EnableExperimentalFeatures(1, &D3D12ExperimentalShaderModels, nullptr, nullptr);

	CreateDevice();
    CreateMemoryAllocator();
	LoadCapabilities();
    m_SRVDescriptorManager = std::make_unique<DescriptorManager>();
    m_SRVDescriptorManager->Init(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        g_SRVPersistentDescriptorMaxCount.GetValue(),
        g_SRVTemporaryDescriptorMaxCountPerFrame.GetValue());
    m_SamplerDescriptorManager = std::make_unique<DescriptorManager>();
    m_SamplerDescriptorManager->Init(
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
        g_SamplerPersistentDescriptorMaxCount.GetValue(),
        g_SamplerTemporaryDescriptorMaxCountPerFrame.GetValue());
    m_RTVDescriptorManager = std::make_unique<DescriptorManager>();
    m_RTVDescriptorManager->Init(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        g_RTVPersistentDescriptorMaxCount.GetValue(),
        0);
    m_DSVDescriptorManager = std::make_unique<DescriptorManager>();
    m_DSVDescriptorManager->Init(
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        g_DSVPersistentDescriptorMaxCount.GetValue(),
        0);
    m_TemporaryConstantBufferManager = std::make_unique<TemporaryConstantBufferManager>();
    m_TemporaryConstantBufferManager->Init();
    m_StandardSamplers.Init();
    m_ShaderCompiler= std::make_unique<ShaderCompiler>();
    m_ShaderCompiler->Init();
	CreateCommandQueues();
	CreateSwapChain();
	CreateFrameResources();
	CreateResources();
    CreateStandardTextures();
    m_AssimpInit = std::make_unique<AssimpInit>();
    LoadModel();
    
    m_Camera = std::make_unique<OrbitingCamera>();
    m_Camera->SetAspectRatio(GetFinalResolutionF().x / GetFinalResolutionF().y);

    ERR_CATCH_MSG(L"Failed to initialize renderer.");
}

Renderer::~Renderer()
{
    if(m_CmdQueue)
    {
	    try
	    {
		    m_CmdQueue->Signal(m_Fence.Get(), m_NextFenceValue);
            WaitForFenceOnCPU(m_NextFenceValue);
	    }
	    CATCH_PRINT_ERROR(;);
    }

    ClearModel();

    for(uint32_t i = g_FrameCount.GetValue(); i--; )
        m_FrameResources[i].m_BackBuffer.reset();
}

void Renderer::Reload()
{
	m_CmdQueue->Signal(m_Fence.Get(), m_NextFenceValue);
    WaitForFenceOnCPU(m_NextFenceValue++);
    
    m_PipelineState.Reset();
    ClearModel();

    CreatePipelineState();
    CreatePostprocessingPipelineState();
    LoadModel();
}

uvec2 Renderer::GetFinalResolutionU()
{
    return g_Size.GetValue();
}

vec2 Renderer::GetFinalResolutionF()
{
    return vec2((float)g_Size.GetValue().x, (float)g_Size.GetValue().y);
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

    m_SRVDescriptorManager->NewFrame();
    m_SamplerDescriptorManager->NewFrame();
    m_RTVDescriptorManager->NewFrame();
    m_DSVDescriptorManager->NewFrame();
    m_TemporaryConstantBufferManager->NewFrame();

    CommandList cmdList;
    cmdList.Init(m_CmdAllocator.Get(), frameRes.m_CmdList.Get());
    {
        PIX_EVENT_SCOPE(cmdList, L"FRAME");

        D3D12_VIEWPORT viewport = {0.f, 0.f, GetFinalResolutionF().x, GetFinalResolutionF().y, 0.f, 1.f};
        cmdList.SetViewport(viewport);

	    const D3D12_RECT scissorRect = {0, 0, (LONG)GetFinalResolutionU().x, (LONG)GetFinalResolutionU().y};
	    cmdList.SetScissorRect(scissorRect);
        
        frameRes.m_BackBuffer->TransitionToStates(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

        float time = (float)GetTickCount() * 1e-3f;
	    //float pos = fmod(time * 100.f, (float)SIZE_X);

        {
            PIX_EVENT_SCOPE(cmdList, L"Clears");

            m_ColorRenderTarget->TransitionToStates(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	        const float clearRGBA[] = {0.f, 0.0f, 0.25f, 1.0f};
	        cmdList.GetCmdList()->ClearRenderTargetView(
                m_ColorRenderTarget->GetD3D12RTV(),
                clearRGBA,
                0,
                nullptr);
            cmdList.GetCmdList()->ClearDepthStencilView(
                m_DepthTexture->GetD3D12DSV(),
                D3D12_CLEAR_FLAG_DEPTH,
                0.f, // depth
                0, // stencil
                0, nullptr); // NumRects, prects
        }

        cmdList.SetRenderTargets(m_DepthTexture.get(), m_ColorRenderTarget.get());

        if(m_PipelineState && m_RootSignature)
        {
            PIX_EVENT_SCOPE(cmdList, L"3D");

            cmdList.SetPipelineState(m_PipelineState.Get());
	        cmdList.SetRootSignature(m_RootSignature.Get());

            // Set per-frame constants.
            {
                const vec3 dirToLight_World = g_DirectionToLight.GetValue();
                const vec3 dirToLight_View = glm::normalize(vec3(m_Camera->GetView() * vec4(dirToLight_World, 0.f)));

                PerFrameConstants myData = {};
                myData.m_FrameIndex = m_FrameIndex;
                myData.m_SceneTime = time;
                myData.m_DirToLight_View = dirToLight_View;
                myData.m_LightColor = g_LightColor.GetValue();
                myData.m_AmbientColor = g_AmbientColor.GetValue();

                void* mappedPtr = nullptr;
                D3D12_GPU_DESCRIPTOR_HANDLE descriptorHandle;
                m_TemporaryConstantBufferManager->CreateBuffer(sizeof(myData), mappedPtr, descriptorHandle);
                memcpy(mappedPtr, &myData, sizeof(myData));

                cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(ROOT_PARAM_PER_FRAME_CBV, descriptorHandle);
            }

            cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(ROOT_PARAM_SAMPLER,
                m_StandardSamplers.GetD3D12(D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP));

            vec3 scaleVec = vec3(g_AssimpScale.GetValue());
            mat4 globalXform = glm::scale(g_AssimpTransform.GetValue(), scaleVec);
            //globalXform = glm::rotate(globalXform, glm::half_pi<float>(), vec3(1.f, 0.f, 0.f));
            RenderEntity(cmdList, globalXform, m_RootEntity);
        }

        if(m_PostprocessingRootSignature && m_PostprocessingPipelineState)
        {
            PIX_EVENT_SCOPE(cmdList, L"Postprocessing");
            m_ColorRenderTarget->TransitionToStates(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            cmdList.SetRenderTargets(nullptr, frameRes.m_BackBuffer.get());
            cmdList.SetPipelineState(m_PostprocessingPipelineState.Get());
            cmdList.SetRootSignature(m_PostprocessingRootSignature.Get());
            cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(
                POSTPROCESSING_ROOT_PARAM_TEXTURE, m_ColorRenderTarget->GetD3D12SRV());
            cmdList.GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            cmdList.GetCmdList()->DrawInstanced(3, 1, 0, 0);

        }

        frameRes.m_BackBuffer->TransitionToStates(cmdList, D3D12_RESOURCE_STATE_PRESENT);
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
	swapChainDesc.BufferDesc.Width = GetFinalResolutionU().x;
	swapChainDesc.BufferDesc.Height = GetFinalResolutionU().y;
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

	HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	CHECK_BOOL(fenceEvent);
	m_FenceEvent.reset(fenceEvent);

	for(uint32_t i = 0; i < g_FrameCount.GetValue(); ++i)
	{
		CHECK_HR(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_CmdAllocator.Get(), NULL,
			IID_PPV_ARGS(&m_FrameResources[i].m_CmdList)));
		CHECK_HR(m_FrameResources[i].m_CmdList->Close());
        SetD3D12ObjectName(m_FrameResources[i].m_CmdList, std::format(L"Command list {}", i));

        ID3D12Resource* backBuffer = nullptr;
		CHECK_HR(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));
        m_FrameResources[i].m_BackBuffer = std::make_unique<RenderingResource>();
        m_FrameResources[i].m_BackBuffer->InitExternallyOwned(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
	}
}

static void SetDefaultRasterizerDesc(D3D12_RASTERIZER_DESC& outDesc)
{
    switch(g_BackFaceCullingMode.GetValue())
    {
    case 0:
        outDesc.CullMode = D3D12_CULL_MODE_NONE;
        outDesc.FrontCounterClockwise = FALSE;
        break;
    case 1:
        outDesc.CullMode = D3D12_CULL_MODE_BACK;
        outDesc.FrontCounterClockwise = FALSE;
        break;
    default:
        outDesc.CullMode = D3D12_CULL_MODE_BACK;
        outDesc.FrontCounterClockwise = TRUE;
    }
    outDesc.FillMode = D3D12_FILL_MODE_SOLID;
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
            GetFinalResolutionU().x, GetFinalResolutionU().y,
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
        D3D12_ROOT_PARAMETER params[ROOT_PARAM_COUNT] = {};

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

        const CD3DX12_DESCRIPTOR_RANGE samplerDescRange{D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0};
        params[ROOT_PARAM_SAMPLER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[ROOT_PARAM_SAMPLER].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        params[ROOT_PARAM_SAMPLER].DescriptorTable.NumDescriptorRanges = 1;
        params[ROOT_PARAM_SAMPLER].DescriptorTable.pDescriptorRanges = &samplerDescRange;

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = (uint32_t)_countof(params);
        desc.pParameters = params;
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

    // Font
    {
        m_Font = std::make_unique<Font>();
        m_Font->Init();
    }

    {
        D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            GetFinalResolutionU().x, GetFinalResolutionU().y,
            1, // arraySize
            1, // mipLevels
            1, 0, // sampleCount, sampleQuality
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        m_ColorRenderTarget = std::make_unique<RenderingResource>();
        m_ColorRenderTarget->Init(resDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, L"ColorRenderTarget");
    }

	CreatePipelineState();
	CreatePostprocessingPipelineState();

    ERR_CATCH_FUNC;
}

void Renderer::CreatePipelineState()
{
    assert(m_ColorRenderTarget);

    m_PipelineState.Reset();

    ERR_TRY
    ERR_TRY

    Shader vs, ps;
    vs.Init(ShaderType::Vertex, L"Data/VS.hlsl", L"main");
    ps.Init(ShaderType::Pixel,  L"Data/PS.hlsl", L"main");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	desc.InputLayout.NumElements = Vertex::GetInputElementCount();
	desc.InputLayout.pInputElementDescs = Vertex::GetInputElements();
	desc.pRootSignature = m_RootSignature.Get();
	desc.VS.BytecodeLength = vs.GetCode().size();
	desc.VS.pShaderBytecode = vs.GetCode().data();
	desc.PS.BytecodeLength = ps.GetCode().size();
	desc.PS.pShaderBytecode = ps.GetCode().data();
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = m_ColorRenderTarget->GetDesc().Format;
	desc.DSVFormat = DEPTH_STENCIL_FORMAT;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = UINT32_MAX;
	SetDefaultRasterizerDesc(desc.RasterizerState);
	SetDefaultBlendDesc(desc.BlendState);
	SetDefaultDepthStencilDesc(desc.DepthStencilState);
	CHECK_HR(m_Device->CreateGraphicsPipelineState(&desc, __uuidof(ID3D12PipelineState), &m_PipelineState));
    SetD3D12ObjectName(m_PipelineState, L"Test pipeline state");

    ERR_CATCH_MSG(L"Cannot create pipeline state.");
    } CATCH_PRINT_ERROR(;);
}

void Renderer::CreatePostprocessingPipelineState()
{
    m_PostprocessingRootSignature.Reset();
    m_PostprocessingPipelineState.Reset();

    ERR_TRY
    ERR_TRY

    // Root signature
    {
        D3D12_ROOT_PARAMETER params[POSTPROCESSING_ROOT_PARAM_COUNT] = {};

        const CD3DX12_DESCRIPTOR_RANGE descRange{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0};
        params[POSTPROCESSING_ROOT_PARAM_TEXTURE].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[POSTPROCESSING_ROOT_PARAM_TEXTURE].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        params[POSTPROCESSING_ROOT_PARAM_TEXTURE].DescriptorTable.NumDescriptorRanges = 1;
        params[POSTPROCESSING_ROOT_PARAM_TEXTURE].DescriptorTable.pDescriptorRanges = &descRange;

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = (uint32_t)_countof(params);
        desc.pParameters = params;
		desc.Flags = //D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
		ComPtr<ID3DBlob> blob;
		CHECK_HR(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr));
		CHECK_HR(m_Device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
			__uuidof(ID3D12RootSignature), &m_PostprocessingRootSignature));
        SetD3D12ObjectName(m_PostprocessingRootSignature, L"Postprocessing root signature");
	}

    Shader vs, ps;
    vs.Init(ShaderType::Vertex, L"Data/FullScreenQuadVS.hlsl", L"main");
    ps.Init(ShaderType::Pixel,  L"Data/PostprocessingPS.hlsl", L"main");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	//desc.InputLayout.NumElements = ;
	//desc.InputLayout.pInputElementDescs = ;
	desc.pRootSignature = m_PostprocessingRootSignature.Get();
	desc.VS.BytecodeLength = vs.GetCode().size();
	desc.VS.pShaderBytecode = vs.GetCode().data();
	desc.PS.BytecodeLength = ps.GetCode().size();
	desc.PS.pShaderBytecode = ps.GetCode().data();
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = RENDER_TARGET_FORMAT;
	desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = UINT32_MAX;

    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.DepthClipEnable = TRUE;

    const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
        FALSE,FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL };
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        desc.BlendState.RenderTarget[i] = defaultRenderTargetBlendDesc;

    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
    desc.DepthStencilState.StencilEnable = FALSE;
    desc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    desc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp = {
        D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
    desc.DepthStencilState.FrontFace = defaultStencilOp;
    desc.DepthStencilState.BackFace = defaultStencilOp;

	CHECK_HR(m_Device->CreateGraphicsPipelineState(&desc, __uuidof(ID3D12PipelineState), &m_PostprocessingPipelineState));
    SetD3D12ObjectName(m_PostprocessingPipelineState, L"Test pipeline state");

    ERR_CATCH_MSG(L"Cannot create postprocessing pipeline state.");
    } CATCH_PRINT_ERROR(;);
}

void Renderer::CreateStandardTextures()
{
    const wchar_t* NAMES[] = {
        L"Standard texture - Transparent",
        L"Standard texture - Black",
        L"Standard texture - Gray",
        L"Standard texture - White",
        L"Standard texture - Red",
        L"Standard texture - Green",
        L"Standard texture - Blue",
        L"Standard texture - Yellow",
        L"Standard texture - Fuchsia",
    };
    // We are little endian, so it is 0xAABBGGRR
    using PixelType = uint32_t;
    const PixelType COLORS[] = {
        0x00000000, 0xFF000000, 0xFF808080, 0xFFFFFFFF, 0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFF00FFFF, 0xFFFF00FF,
    };
    static_assert(_countof(NAMES) == (size_t)StandardTexture::Count);
    static_assert(_countof(COLORS) == (size_t)StandardTexture::Count);

    constexpr uint32_t SIZE = 8;
    constexpr uint32_t ROW_PITCH = std::max<uint32_t>(SIZE * sizeof(PixelType), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

    std::vector<char> buf(SIZE * ROW_PITCH * sizeof(PixelType));

    D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        SIZE, SIZE, // width, height
        1, // arraySize
        1); // mipLevels
    D3D12_SUBRESOURCE_DATA data = {
        .pData = buf.data(),
        .RowPitch = ROW_PITCH,
        .SlicePitch = 0};

    for(size_t textureIndex = 0; textureIndex < (size_t)StandardTexture::Count; ++textureIndex)
    {
        const PixelType color = COLORS[textureIndex];
        char* rowPtr = buf.data();
        for(uint32_t y = 0; y < SIZE; ++y, rowPtr += ROW_PITCH)
        {
            PixelType* pixelPtr = (PixelType*)rowPtr;
            for(uint32_t x = 0; x < SIZE; ++x, ++pixelPtr)
                *pixelPtr = color;
        }

        unique_ptr<Texture>& texPtr = m_StandardTextures[textureIndex];
        texPtr = std::make_unique<Texture>();
        texPtr->LoadFromMemory(resDesc, data, NAMES[textureIndex]);
    }
}

void Renderer::ClearModel()
{
    m_Textures.clear();
    m_Materials.clear();
    m_Meshes.clear();
    m_RootEntity = Entity{};
}

void Renderer::LoadModel()
{
    ClearModel();

    /*
    RegEngine coordinate system is left-handed: X right, Y back, Z up.
    Assimp coordinate system is right-handed: X right, Y up, Z back.
    Thus, we have to:
    - Swap Y and Z components in position.s
    - Flip winding order.
    */

    const wstr_view filePath = g_AssimpModelPath.GetValue();
    LogMessageF(L"Loading model from \"{}\"...", filePath);

    ERR_TRY;
    ERR_TRY;

    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            ConvertUnicodeToChars(filePath, CP_ACP).c_str(),
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_SortByPType |
            aiProcess_FlipWindingOrder |
            aiProcess_GenSmoothNormals |
            aiProcess_FlipUVs); // To make UV space origin in the upper-left corner.
        // aiProcess_CalcTangentSpace
        // aiProcess_MakeLeftHanded
        if(!scene)
            FAIL(ConvertCharsToUnicode(importer.GetErrorString(), CP_ACP));
        
        //PrintAssimpSceneInfo(scene);

        for(uint32_t i = 0; i < scene->mNumMeshes; ++i)
            LoadModelMesh(scene, scene->mMeshes[i]);
        
        const aiNode* node = scene->mRootNode;
        if(node)
            LoadModelNode(m_RootEntity, scene, node);

        const std::filesystem::path modelDir = std::filesystem::path(filePath.begin(), filePath.end()).parent_path();
        for(uint32_t i = 0; i < scene->mNumMaterials; ++i)
            LoadMaterial(modelDir, scene, i, scene->mMaterials[i]);
    }

    ERR_CATCH_MSG(std::format(L"Cannot load model from \"{}\".", filePath));
    } CATCH_PRINT_ERROR(ClearModel(););
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
    CHECK_BOOL(assimpMesh->HasNormals());

    std::vector<Vertex> vertices(vertexCount);
    for(uint32_t i = 0; i < vertexCount; ++i)
    {
        const aiVector3D pos = assimpMesh->mVertices[i];
        const aiVector3D texCoord = assimpMesh->mTextureCoords[0][i];
        const aiVector3D normal = assimpMesh->mNormals[i];
        vertices[i].m_Position = packed_vec3(pos.x, pos.z, pos.y); // Intentionally swapping Y with Z.
        vertices[i].m_Normal = packed_vec3(normal.x, normal.z, normal.y); // Same here.
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

    SceneMesh mesh;
    mesh.m_MaterialIndex = assimpMesh->mMaterialIndex;
    mesh.m_Mesh = std::make_unique<Mesh>();
    mesh.m_Mesh->Init(
        L"Mesh",
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        std::span<const Vertex>(vertices.begin(), vertices.end()),
        std::span<const Mesh::IndexType>(indices.begin(), indices.end()));
    m_Meshes.push_back(std::move(mesh));
}

const aiMaterialProperty* FindMaterialProperty(const aiMaterial* material, const str_view& key)
{
    for(uint32_t i = 0; i < material->mNumProperties; ++i)
    {
        const aiMaterialProperty* prop = material->mProperties[i];
        if(str_view(prop->mKey.C_Str()) == key)
            return prop;
    }
    return nullptr;
}
bool GetStringMaterialProperty(string& out, const aiMaterial* material, const str_view& key)
{
    const aiMaterialProperty* prop = FindMaterialProperty(material, key);
    if(!prop)
        return false;
    if(prop->mType != aiPTI_String)
        return false;
    const aiString* s = (const aiString*)prop->mData;
    out = string(s->data, s->length);
    return true;
}

void Renderer::LoadMaterial(const std::filesystem::path& modelDir, const aiScene* scene, uint32_t materialIndex, const aiMaterial* material)
{
    ERR_TRY;

    string texturePath;
    // Canonical way - GetTexture.
    if(material->GetTextureCount(aiTextureType_DIFFUSE) == 1)
    {
        aiString s;
        aiTextureMapping mapping;
        uint32_t uvindex;
        float blend;
        aiTextureOp op;
        aiTextureMapMode mapmodes[3];
        const aiReturn ret = material->GetTexture(aiTextureType_DIFFUSE, 0,
            &s, &mapping, &uvindex, &blend, &op, mapmodes);
        if(ret == aiReturn_SUCCESS)
            texturePath = s.C_Str();
    }
    // Method found in a model from Turbosquid made in Maya.
    if(texturePath.empty())
    {
        GetStringMaterialProperty(texturePath, material, "$raw.Maya|baseColor|file");
    }

    std::filesystem::path texturePathP;
    if(!texturePath.empty())
        texturePathP = StrToPath(ConvertCharsToUnicode(texturePath, CP_ACP));
    else if(!g_TexturePath.GetValue().empty())
        // "TexturePath" setting - relative to working directory not model path!
        texturePathP = std::filesystem::absolute(StrToPath(g_TexturePath.GetValue()));

    if(texturePathP.empty())
        m_Materials.push_back(SceneMaterial{});
    else
    {
        if(!texturePathP.is_absolute())
            texturePathP = modelDir / texturePathP;

        wstring processedPath = std::filesystem::weakly_canonical(texturePathP);
        ToUpperCase(processedPath);

        // Find existing texture.
        for(size_t i = 0, count = m_Textures.size(); i < count; ++i)
        {
            if(m_Textures[i].m_ProcessedPath == processedPath)
            {
               m_Materials.push_back(SceneMaterial{i});
               return;
            }
        }

        // Not found - load new texture.
        SceneTexture tex;
        tex.m_ProcessedPath = std::move(processedPath);
        tex.m_Texture = std::make_unique<Texture>();
        tex.m_Texture->LoadFromFile(
            Texture::FLAG_SRGB | Texture::FLAG_GENERATE_MIPMAPS | Texture::FLAG_CACHE_LOAD | Texture::FLAG_CACHE_SAVE,
            texturePathP.native());
        m_Textures.push_back(std::move(tex));
        m_Materials.push_back(SceneMaterial{m_Textures.size() - 1});
    }

    ERR_CATCH_MSG(std::format(L"Cannot load material {}.", materialIndex));
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
    // I thought this is the right way
    const mat4 entityXform = parentXform * entity.m_Transform;
    // It seems that possibly this works better
    //const mat4 entityXform = entity.m_Transform * parentXform;

    if(!entity.m_Meshes.empty())
    {
        PerObjectConstants perObjConstants = {};
        perObjConstants.m_WorldViewProj = m_Camera->GetViewProjection() * entityXform;
        perObjConstants.m_WorldView = m_Camera->GetView() * entityXform;

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
    const Mesh* const mesh = m_Meshes[meshIndex].m_Mesh.get();
    const size_t materialIndex = m_Meshes[meshIndex].m_MaterialIndex;
    assert(materialIndex < m_Materials.size());
    const SceneMaterial& mat = m_Materials[materialIndex];
    Texture* texture = mat.m_TextureIndex != SIZE_MAX ? m_Textures[mat.m_TextureIndex].m_Texture.get() : nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE textureDescriptorHandle;
    if(texture)
    {
        textureDescriptorHandle =
            m_SRVDescriptorManager->GetGPUHandle(m_Textures[mat.m_TextureIndex].m_Texture->GetDescriptor());
    }
    else
    {
        textureDescriptorHandle =
            m_SRVDescriptorManager->GetGPUHandle(m_StandardTextures[(size_t)StandardTexture::Gray]->GetDescriptor());
    }
    cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(ROOT_PARAM_TEXTURE_SRV, textureDescriptorHandle);

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

void StandardSamplers::Init()
{
    ERR_TRY;

    assert(g_Renderer && g_Renderer->GetSamplerDescriptorManager());
    DescriptorManager* descMngr = g_Renderer->GetSamplerDescriptorManager();
    m_Descriptors = descMngr->AllocatePersistent(COUNT);

    const uint32_t maxAnisotropy = g_MaxAnisotropy.GetValue();
    CHECK_BOOL(maxAnisotropy <= 16);

    D3D12_SAMPLER_DESC desc = {};
    desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    desc.MaxAnisotropy = std::max<uint32_t>(maxAnisotropy, 1);
    desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    desc.MaxLOD = FLT_MAX;

    uint32_t descIndex = 0;
    for(uint32_t filterIndex = 0; filterIndex < 4; ++filterIndex)
    {
        switch(filterIndex)
        {
        case 0: desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; break;
        case 1: desc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT; break;
        case 2: desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; break;
        case 3: desc.Filter = maxAnisotropy > 0 ? D3D12_FILTER_ANISOTROPIC : D3D12_FILTER_MIN_MAG_MIP_LINEAR; break;
        default: assert(0);
        }

        for(uint32_t addressIndex = 0; addressIndex < 2; ++addressIndex, ++descIndex)
        {
            switch(addressIndex)
            {
            case 0: desc.AddressU = desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; break;
            case 1: desc.AddressU = desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; break;
            default: assert(0);
            }

            const D3D12_CPU_DESCRIPTOR_HANDLE CPUDescHandle = descMngr->GetCPUHandle(m_Descriptors, descIndex);
            g_Renderer->GetDevice()->CreateSampler(&desc, CPUDescHandle);
        }
    }

    ERR_CATCH_FUNC;
}

StandardSamplers::~StandardSamplers()
{
    assert(g_Renderer && g_Renderer->GetSamplerDescriptorManager());
    g_Renderer->GetSamplerDescriptorManager()->FreePersistent(m_Descriptors);
}

Descriptor StandardSamplers::Get(D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE address) const
{
    uint32_t index = 0;
    switch(filter)
    {
    case D3D12_FILTER_MIN_MAG_MIP_POINT:        index = 0; break;
    case D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT: index = 1; break;
    case D3D12_FILTER_MIN_MAG_MIP_LINEAR:       index = 2; break;
    case D3D12_FILTER_ANISOTROPIC:              index = 3; break;
    default: assert(0);
    }

    index *= 2;
    switch(address)
    {
    case D3D12_TEXTURE_ADDRESS_MODE_WRAP: break;
    case D3D12_TEXTURE_ADDRESS_MODE_CLAMP: index += 1; break;
    default: assert(0);
    }

    Descriptor result = m_Descriptors;
    result.m_Index += index;
    return result;
}

D3D12_GPU_DESCRIPTOR_HANDLE StandardSamplers::GetD3D12(D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE address) const
{
    Descriptor desc = Get(filter, address);
    assert(g_Renderer && g_Renderer->GetSamplerDescriptorManager());
    return g_Renderer->GetSamplerDescriptorManager()->GetGPUHandle(desc);
}
