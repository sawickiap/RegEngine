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
#include "ImGuiUtils.hpp"
#include "Streams.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/LogStream.hpp>
#include <assimp/Logger.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/vector2.h>
#include <assimp/vector3.h>
#include "../WorkingDir/Shaders/Include/ShaderConstants.h"
#include "../ThirdParty/imgui-1.87/backends/imgui_impl_dx12.h"

static const D3D_FEATURE_LEVEL MY_D3D_FEATURE_LEVEL = D3D_FEATURE_LEVEL_12_0;
static const DXGI_FORMAT RENDER_TARGET_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
static const DXGI_FORMAT DEPTH_STENCIL_FORMAT = DXGI_FORMAT_D32_FLOAT;

static BoolSetting g_D3D12DebugLayer_Enabled(
    SettingCategory::Startup, "D3D12DebugLayer.Enabled", false);
static BoolSetting g_D3D12DebugLayer_SynchronizedCommandQueueValidation_Enabled(
    SettingCategory::Startup, "D3D12DebugLayer.SynchronizedCommandQueueValidation.Enabled", true);
static BoolSetting g_D3D12DebugLayer_AllowBehaviorChangingDebugAids(
    SettingCategory::Startup, "D3D12DebugLayer.AllowBehaviorChangingDebugAids", false);
static BoolSetting g_D3D12DebugLayer_ConservativeResourceStateTracking(
    SettingCategory::Startup, "D3D12DebugLayer.ConservativeResourceStateTracking", false);
static BoolSetting g_D3D12DebugLayer_DisableVirtualizedBundlesValidation(
    SettingCategory::Startup, "D3D12DebugLayer.DisableVirtualizedBundlesValidation", false);
static BoolSetting g_D3D12DebugLayer_ReportLiveDeviceObjects(
    SettingCategory::Startup, "D3D12DebugLayer.ReportLiveDeviceObjects", true);
static FloatSetting g_D3D12DebugLayer_SlowdownPerformanceFactor(
    SettingCategory::Startup, "D3D12DebugLayer.SlowdownPerformanceFactor", 0.f);
static BoolSetting g_D3D12DebugLayer_GPUBasedValidation_Enabled(
    SettingCategory::Startup, "D3D12DebugLayer.GPUBasedValidation.Enabled", false);
static BoolSetting g_D3D12DebugLayer_GPUBasedValidation_StateTracking_Enabled(
    SettingCategory::Startup, "D3D12DebugLayer.GPUBasedValidation.StateTracking.Enabled", true);
static UintSetting g_D3D12DebugLayer_GPUBasedValidation_MaxMessagesPerCommandList(
    SettingCategory::Startup, "D3D12DebugLayer.GPUBasedValidation.MaxMessagesPerCommandList", 256);
static UintSetting g_D3D12DebugLayer_GPUBasedValidation_DefaultShaderPatchMode(
    SettingCategory::Startup, "D3D12DebugLayer.GPUBasedValidation.DefaultShaderPatchMode", 2);
static UintSetting g_D3D12DebugLayer_GPUBasedValidation_PipelineStateCreateFlags(
    SettingCategory::Startup, "D3D12DebugLayer.GPUBasedValidation.PipelineStateCreateFlags", 0);

extern VecSetting<glm::uvec2> g_Size;

enum ASSIMP_LOG_SEVERITY
{
    ASSIMP_LOG_SEVERITY_NONE,
    ASSIMP_LOG_SEVERITY_ERROR,
    ASSIMP_LOG_SEVERITY_WARNING,
    ASSIMP_LOG_SEVERITY_INFO,
    ASSIMP_LOG_SEVERITY_DEBUG,
};

static const DXGI_FORMAT GBUFFER_FORMATS[] = {
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    DXGI_FORMAT_R16G16B16A16_FLOAT,
};
static const wchar_t* GBUFFER_NAMES[] = {
    L"Albedo",
    L"Normal",
};
static_assert(_countof(GBUFFER_FORMATS) == (size_t)GBuffer::Count);
static_assert(_countof(GBUFFER_NAMES) == (size_t)GBuffer::Count);

UintSetting g_FrameCount(SettingCategory::Startup, "FrameCount", 3);
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

static BoolSetting g_AssimpPrintSceneInfo(SettingCategory::Load, "Assimp.PrintSceneInfo", false);
static UintSetting g_SyncInterval(SettingCategory::Runtime, "SyncInterval", 1);
static StringSetting g_AssimpModelPath(SettingCategory::Load, "Assimp.ModelPath");
static StringSetting g_TexturePath(SettingCategory::Load, "TexturePath");
static StringSetting g_NormalTexturePath(SettingCategory::Load, "NormalTexturePath");
static FloatSetting g_AssimpScale(SettingCategory::Load, "Assimp.Scale", 1.f);
static MatSetting<mat4> g_AssimpTransform(SettingCategory::Load, "Assimp.Transform", glm::identity<mat4>());
static BoolSetting g_AssimpNegateBitangent(SettingCategory::Load, "Assimp.NegateBitangent", true);
static BoolSetting g_AssimpUseOptimizingFlags(SettingCategory::Load, "Assimp.UseOptimizingFlags", false);
static UintSetting g_BackFaceCullingMode(SettingCategory::Load, "BackFaceCullingMode", 0);

static Vec4ColorSetting g_BackgroundColor(SettingCategory::Runtime, "Background.Color", vec4(0.f, 0.f, 0.f, 1.f));
static VecSetting<vec3> g_DirectionToLight(SettingCategory::Load, "DirectionToLight", vec3(0.f, 1.f, 0.f));
static Vec3ColorSetting g_LightColor(SettingCategory::Load, "LightColor", vec3(0.9f));
static Vec3ColorSetting g_AmbientColor(SettingCategory::Load, "AmbientColor", vec3(0.1f));

static BoolSetting g_AlbedoTexturesEnabled(SettingCategory::Volatile, "Renderer.Debug.AlbedoTextures.Enabled", true);
static BoolSetting g_NormalMapsEnabled(SettingCategory::Volatile, "Renderer.Debug.NormalMaps.Enabled", true);
static BoolSetting g_BackfaceCullingEnabled(SettingCategory::Volatile, "Renderer.Debug.BackfaceCulling.Enabled", true);

Renderer* g_Renderer;

#define HELPER_CAT_1(a, b) a ## b
#define HELPER_CAT_2(a, b) HELPER_CAT_1(a, b)
#define VAR_NAME_WITH_LINE(name) HELPER_CAT_2(name, __LINE__)
#define PIX_EVENT_SCOPE(cmdList, msg) PIXEventScope VAR_NAME_WITH_LINE(pixEventScope)((cmdList), (msg));

static const uint32_t ASSIMP_READ_FLAGS =
    aiProcess_Triangulate |
    aiProcess_JoinIdenticalVertices |
    aiProcess_SortByPType |
    aiProcess_GenSmoothNormals |
    aiProcess_CalcTangentSpace |
    aiProcess_ValidateDataStructure |
    // This step flips all UV coordinates along the y-axis and adjusts
    // material settings and bitangents accordingly.
    aiProcess_FlipUVs;
// aiProcess_MakeLeftHanded
// aiProcess_ConvertToLeftHanded

static const uint32_t ASSIMP_OPTIMIZING_FLAGS =
    aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph | aiProcess_RemoveRedundantMaterials;

static uint32_t GetAssimpFlags()
{
    uint32_t flags = ASSIMP_READ_FLAGS;
    if(g_AssimpUseOptimizingFlags.GetValue())
        flags |= ASSIMP_OPTIMIZING_FLAGS;
    return flags;
}

struct PerFrameConstants
{
    uint32_t m_FrameIndex;
    float m_SceneTime;
    packed_vec2 m_RenderResolution;
    
    packed_vec2 m_RenderResolutionInv;
    uint32_t _padding0[2];

    packed_mat4 m_Proj;
    packed_mat4 m_ProjInv;
    
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

struct PerMaterialConstants
{
    uint32_t m_Flags;
    float m_AlphaCutoff;
    uint32_t _padding0[2];

    packed_vec3 m_Color;
    uint32_t _padding1;
};

struct LightConstants
{
    packed_vec3 m_Color;
    uint32_t m_Type;

    packed_vec3 m_DirectionToLight_Position;
    uint32_t _padding0;
};

class AssimpInit
{
public:
    AssimpInit();
    ~AssimpInit();
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

    Assimp::DefaultLogger::create(g_AssimpLogFilePath.GetValue().c_str(), logSeverity);
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

StandardRootSignature::StandardRootSignature()
{
    constexpr uint32_t PARAM_COUNT = CBV_COUNT + SRV_COUNT + SAMPLER_COUNT;
    D3D12_DESCRIPTOR_RANGE descRanges[PARAM_COUNT];
    D3D12_ROOT_PARAMETER params[PARAM_COUNT];
    uint32_t paramIndex = 0;
    for(uint32_t CBVIndex = 0; CBVIndex < CBV_COUNT; ++CBVIndex, ++paramIndex)
    {
        descRanges[paramIndex] = {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
            .NumDescriptors = 1,
            .BaseShaderRegister = CBVIndex};
        params[paramIndex] = {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable = {
                .NumDescriptorRanges = 1,
                .pDescriptorRanges = descRanges + paramIndex},
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL};
    }
    for(uint32_t SRVIndex = 0; SRVIndex < SRV_COUNT; ++SRVIndex, ++paramIndex)
    {
        descRanges[paramIndex] = {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            .NumDescriptors = 1,
            .BaseShaderRegister = SRVIndex};
        params[paramIndex] = {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable = {
                .NumDescriptorRanges = 1,
                .pDescriptorRanges = descRanges + paramIndex},
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL};
    }
    for(uint32_t samplerIndex = 0; samplerIndex < SAMPLER_COUNT; ++samplerIndex, ++paramIndex)
    {
        descRanges[paramIndex] = {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
            .NumDescriptors = 1,
            .BaseShaderRegister = samplerIndex};
        params[paramIndex] = {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable = {
                .NumDescriptorRanges = 1,
                .pDescriptorRanges = descRanges + paramIndex},
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL};
    }

	D3D12_ROOT_SIGNATURE_DESC desc = {
		.NumParameters = PARAM_COUNT,
        .pParameters = params,
		.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS};
	ComPtr<ID3DBlob> blob;
	CHECK_HR(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr));
	CHECK_HR(g_Renderer->GetDevice()->CreateRootSignature(
        0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));
    SetD3D12ObjectName(m_RootSignature, L"Standard root signature");
}

#ifndef _RENDERER_IMPL

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

    if(g_D3D12DebugLayer_Enabled.GetValue())
        EnableDebugLayer();

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

    {
        wstr_view MACRO_NAMES[] = {
            L"ALPHA_TEST",
            L"HAS_MATERIAL_COLOR",
            L"HAS_ALBEDO_TEXTURE",
            L"HAS_NORMAL_TEXTURE",
        };
        m_GBufferMultiPixelShader = std::make_unique<MultiShader>();
        m_GBufferMultiPixelShader->Init(ShaderType::Pixel, L"Shaders/GBuffer.hlsl", L"MainPS", MACRO_NAMES);
    }

	CreateCommandQueues();
	CreateSwapChain();
	CreateFrameResources();
	CreateResources();
    CreateStandardTextures();
    InitImGui();
    
    m_AssimpInit = std::make_unique<AssimpInit>();
    LoadModel(false);
    //CreateProceduralModel();
    
    //m_Camera = std::make_unique<OrbitingCamera>();
    m_Camera = std::make_unique<FlyingCamera>();
    m_Camera->SetAspectRatio(GetFinalResolutionF().x / GetFinalResolutionF().y);
    //m_Camera->SetDistance(3.f);

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

    ShutdownImGui();

    for(uint32_t i = g_FrameCount.GetValue(); i--; )
        m_FrameResources[i].m_BackBuffer.reset();
}

void Renderer::Reload(bool refreshAll)
{
	m_CmdQueue->Signal(m_Fence.Get(), m_NextFenceValue);
    WaitForFenceOnCPU(m_NextFenceValue++);
    
    ClearGBufferShaders();
    ClearModel();

    CreatePostprocessingPipelineState();
    CreateLightingPipelineStates();
    LoadModel(refreshAll);
    //CreateProceduralModel();
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
    CHECK_HR(m_UploadCmdAllocator->Reset());
    dstCmdList.Init(m_UploadCmdAllocator.Get(), m_UploadCmdList.Get());
}

void Renderer::CompleteUploadCommandList(CommandList& cmdList)
{
    cmdList.Execute(m_CmdQueue.Get());

	m_UploadCmdListSubmittedFenceValue = m_NextFenceValue++;
	CHECK_HR(m_CmdQueue->Signal(m_Fence.Get(), m_UploadCmdListSubmittedFenceValue));

    WaitForFenceOnCPU(m_UploadCmdListSubmittedFenceValue);
}

void Renderer::ImGui_D3D12MAStatistics()
{
    D3D12MA::Budget b[2] = {};
    m_MemoryAllocator->GetBudget(b, b + 1);
    for(size_t i = 0; i < 2; ++i)
    {
        if(ImGui::TreeNodeEx(i ? "Non-local" : "Local", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const double usagePercent = b[i].BudgetBytes ?
                (double)b[i].UsageBytes / (double)b[i].BudgetBytes * 100. : 0.;
            const double allocationBytesPercent = b[i].Stats.BlockBytes ?
                (double)b[i].Stats.AllocationBytes / (double)b[i].Stats.BlockBytes * 100. : 0.;
            
            ImVec4 usageColor;
            if(usagePercent > 90.)
                usageColor = {1.f, 0.f, 0.f, 1.f}; // Red
            else if(usagePercent > 70.)
                usageColor = {1.f, 1.f, 0.f, 1.f}; // Yellow
            else
                usageColor = {0.f, 1.f, 0.f, 1.f}; // Green

            ImGui::Text("Blocks: %u, %s",
                b[i].Stats.BlockCount,
                ConvertUnicodeToChars(SizeToStr(b[i].Stats.BlockBytes), CP_UTF8).c_str());
            ImGui::Text("Allocations: %u, %s (%.1f%%)",
                b[i].Stats.AllocationCount,
                ConvertUnicodeToChars(SizeToStr(b[i].Stats.AllocationBytes), CP_UTF8).c_str(),
                allocationBytesPercent);
            ImGui::TextColored(usageColor, "Budget: %s, usage: %s (%.1f%%)",
                ConvertUnicodeToChars(SizeToStr(b[i].BudgetBytes), CP_UTF8).c_str(),
                ConvertUnicodeToChars(SizeToStr(b[i].UsageBytes), CP_UTF8).c_str(),
                usagePercent);
            ImGui::TreePop();
        }
    }

    if(ImGui::Button("JSON dump"))
        SaveD3D12MAJSONDump();
}

void Renderer::Render()
{
    ERR_TRY

	m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
	FrameResources& frameRes = m_FrameResources[m_FrameIndex];
    WaitForFenceOnCPU(frameRes.m_SubmittedFenceValue);

    m_SRVDescriptorManager->NewFrame();
    m_SamplerDescriptorManager->NewFrame();
    m_RTVDescriptorManager->NewFrame();
    m_DSVDescriptorManager->NewFrame();
    m_TemporaryConstantBufferManager->NewFrame();

    CommandList cmdList;
    CHECK_HR(frameRes.m_CmdAllocator->Reset());
    cmdList.Init(frameRes.m_CmdAllocator.Get(), frameRes.m_CmdList.Get());
    {
        PIX_EVENT_SCOPE(cmdList, L"FRAME");

        D3D12_VIEWPORT viewport = {0.f, 0.f, GetFinalResolutionF().x, GetFinalResolutionF().y, 0.f, 1.f};
        cmdList.SetViewport(viewport);

	    const D3D12_RECT scissorRect = {0, 0, (LONG)GetFinalResolutionU().x, (LONG)GetFinalResolutionU().y};
	    cmdList.SetScissorRect(scissorRect);
        
        frameRes.m_BackBuffer->TransitionToStates(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

        float time = (float)GetTickCount() * 1e-3f;
	    //float pos = fmod(time * 100.f, (float)SIZE_X);

        // Set per-frame constants.
        D3D12_GPU_DESCRIPTOR_HANDLE perFrameConstants;
        {
            const vec3 dirToLight_World = g_DirectionToLight.GetValue();
            const vec3 dirToLight_View = glm::normalize(vec3(m_Camera->GetView() * vec4(dirToLight_World, 0.f)));

            PerFrameConstants myData = {};
            myData.m_FrameIndex = m_FrameIndex;
            myData.m_SceneTime = time;
            myData.m_RenderResolution = GetFinalResolutionF();
            myData.m_RenderResolutionInv = packed_vec2(1.f / myData.m_RenderResolution.x, 1.f / myData.m_RenderResolution.y);
            myData.m_Proj = m_Camera->GetProjection();
            myData.m_ProjInv = m_Camera->GetProjectionInverse();
            myData.m_DirToLight_View = dirToLight_View;
            myData.m_LightColor = g_LightColor.GetValue();
            myData.m_AmbientColor = m_AmbientEnabled ? (packed_vec3)g_AmbientColor.GetValue() : packed_vec3(0.f, 0.f, 0.f);

            void* mappedPtr = nullptr;
            m_TemporaryConstantBufferManager->CreateBuffer(sizeof(myData), mappedPtr, perFrameConstants);
            memcpy(mappedPtr, &myData, sizeof(myData));
        }

        {
            PIX_EVENT_SCOPE(cmdList, L"Clears");

            for(size_t i = 0; i < (size_t)GBuffer::Count; ++i)
            {
                m_GBuffers[i]->TransitionToStates(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	            const float clearRGBA[] = {0.f, 0.f, 0.f, 0.f};
	            cmdList.GetCmdList()->ClearRenderTargetView(
                    m_GBuffers[i]->GetD3D12RTV(),
                    clearRGBA,
                    0, nullptr); // NumRects, pRects
            }

            m_DepthTexture->TransitionToStates(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            cmdList.GetCmdList()->ClearDepthStencilView(
                m_DepthTexture->GetD3D12DSV(),
                D3D12_CLEAR_FLAG_DEPTH,
                0.f, // depth
                0, // stencil
                0, nullptr); // NumRects, pRects
        }

        {
            PIX_EVENT_SCOPE(cmdList, L"G-buffer");

            static_assert((size_t)GBuffer::Count == 2);
            cmdList.SetRenderTargets(m_DepthTexture.get(),
                {m_GBuffers[0].get(), m_GBuffers[1].get()});

	        cmdList.SetRootSignature(m_StandardRootSignature->GetRootSignature());

            cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(
                m_StandardRootSignature->GetCBVParamIndex(0),
                perFrameConstants);

            vec3 scaleVec = vec3(g_AssimpScale.GetValue());
            mat4 globalXform = glm::scale(glm::identity<mat4>(), scaleVec);
            globalXform *= g_AssimpTransform.GetValue();
            //globalXform = glm::rotate(globalXform, glm::half_pi<float>(), vec3(1.f, 0.f, 0.f));
            if(m_RootEntity.m_Visible)
                RenderEntity(cmdList, globalXform, m_RootEntity);
        }

        if(m_AmbientPipelineState && m_LightingPipelineState)
        {
            PIX_EVENT_SCOPE(cmdList, L"Lighting");
            m_DepthTexture->TransitionToStates(cmdList,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ);
            for(size_t i = 0; i < (size_t)GBuffer::Count; ++i)
                m_GBuffers[i]->TransitionToStates(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            m_ColorRenderTarget->TransitionToStates(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

            cmdList.GetCmdList()->ClearRenderTargetView(
                m_ColorRenderTarget->GetD3D12RTV(), glm::value_ptr(g_BackgroundColor.GetValue()), 0, nullptr);

            cmdList.SetRenderTargets(m_DepthTexture.get(), m_ColorRenderTarget.get());
            cmdList.SetRootSignature(m_StandardRootSignature->GetRootSignature());
            
            cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(
                m_StandardRootSignature->GetCBVParamIndex(0), perFrameConstants);
            cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(
                m_StandardRootSignature->GetSRVParamIndex(0), m_DepthTexture->GetD3D12SRV());
            cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(
                m_StandardRootSignature->GetSRVParamIndex(1), m_GBuffers[(size_t)GBuffer::Albedo]->GetD3D12SRV());
            cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(
                m_StandardRootSignature->GetSRVParamIndex(2), m_GBuffers[(size_t)GBuffer::Normal]->GetD3D12SRV());
            cmdList.GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            {
                PIX_EVENT_SCOPE(cmdList, L"Ambient");
                cmdList.SetPipelineState(m_AmbientPipelineState.Get());
                cmdList.GetCmdList()->DrawInstanced(3, 1, 0, 0);
            }

            if(!m_Lights.empty())
            {
                cmdList.SetPipelineState(m_LightingPipelineState.Get());

                for(size_t lightIndex = 0; lightIndex < m_Lights.size(); ++lightIndex)
                {
                    const Scene::Light& l = m_Lights[lightIndex];
                    if(l.m_Enabled)
                    {
                        PIX_EVENT_SCOPE(cmdList, std::format(L"Light {} Type={}", lightIndex, l.m_Type));

                        vec3 dirToLight_View = glm::normalize(TransformNormal(m_Camera->GetView(), l.m_DirectionToLight_Position));

                        LightConstants lc;
                        lc.m_Color = l.m_Color;
                        lc.m_Type = l.m_Type;
                        lc.m_DirectionToLight_Position = dirToLight_View;
                        void* mappedPtr = nullptr;
                        D3D12_GPU_DESCRIPTOR_HANDLE lcDesc;
                        m_TemporaryConstantBufferManager->CreateBuffer(sizeof(lc), mappedPtr, lcDesc);
                        memcpy(mappedPtr, &lc, sizeof(lc));

                        cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(
                            m_StandardRootSignature->GetCBVParamIndex(1), lcDesc);
                        cmdList.GetCmdList()->DrawInstanced(3, 1, 0, 0);
                    }
                }
            }
        }

        if(m_PostprocessingPipelineState)
        {
            PIX_EVENT_SCOPE(cmdList, L"Postprocessing");
            m_ColorRenderTarget->TransitionToStates(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            cmdList.SetRenderTargets(nullptr, frameRes.m_BackBuffer.get());
            cmdList.SetPipelineState(m_PostprocessingPipelineState.Get());
            cmdList.SetRootSignature(m_StandardRootSignature->GetRootSignature());
            cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(
                m_StandardRootSignature->GetSRVParamIndex(0), m_ColorRenderTarget->GetD3D12SRV());
            cmdList.GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            cmdList.GetCmdList()->DrawInstanced(3, 1, 0, 0);
        }

        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList.GetCmdList());

        frameRes.m_BackBuffer->TransitionToStates(cmdList, D3D12_RESOURCE_STATE_PRESENT);
    }
    cmdList.Execute(m_CmdQueue.Get());

	frameRes.m_SubmittedFenceValue = m_NextFenceValue++;
	CHECK_HR(m_CmdQueue->Signal(m_Fence.Get(), frameRes.m_SubmittedFenceValue));

	CHECK_HR(m_SwapChain->Present(g_SyncInterval.GetValue(), 0));

    ERR_CATCH_FUNC
}

void Renderer::EnableDebugLayer()
{
    LogInfo(L"Enabling D3D12 Debug Layer");
    ComPtr<ID3D12Debug> debug;
    if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
    {
        debug->EnableDebugLayer();
    }
    else
    {
        LogWarning(L"D3D12GetDebugInterface failed.");
        return;
    }

    if(g_D3D12DebugLayer_GPUBasedValidation_Enabled.GetValue() ||
        !g_D3D12DebugLayer_SynchronizedCommandQueueValidation_Enabled.GetValue())
    {
        ComPtr<ID3D12Debug1> debug1;
        if(SUCCEEDED(debug->QueryInterface(IID_PPV_ARGS(&debug1))))
        {
            if(g_D3D12DebugLayer_GPUBasedValidation_Enabled.GetValue())
                debug1->SetEnableGPUBasedValidation(TRUE);
            if(!g_D3D12DebugLayer_SynchronizedCommandQueueValidation_Enabled.GetValue())
                debug1->SetEnableSynchronizedCommandQueueValidation(FALSE);
        }
    else
        LogWarning(L"QueryInterface for ID3D12Debug1 failed.");
    }
    
    if(!g_D3D12DebugLayer_GPUBasedValidation_StateTracking_Enabled.GetValue())
    {
        ComPtr<ID3D12Debug2> debug2;
        if(SUCCEEDED(debug->QueryInterface(IID_PPV_ARGS(&debug2))))
        {
            if(!g_D3D12DebugLayer_GPUBasedValidation_StateTracking_Enabled.GetValue())
                debug2->SetGPUBasedValidationFlags(D3D12_GPU_BASED_VALIDATION_FLAGS_DISABLE_STATE_TRACKING);
        }
    else
        LogWarning(L"QueryInterface for ID3D12Debug2 failed.");
    }
}

void Renderer::CreateDevice()
{
    ID3D12Device* devicePtr;
    CHECK_HR(D3D12CreateDevice(m_Adapter, MY_D3D_FEATURE_LEVEL, IID_PPV_ARGS(&devicePtr)));
    m_Device.reset(devicePtr);
    SetD3D12ObjectName(m_Device.get(), L"Device");

    CHECK_HR(m_Device->QueryInterface(IID_PPV_ARGS(&m_Device1)));
    SetD3D12ObjectName(m_Device1, L"Device1");

    if(g_D3D12DebugLayer_Enabled.GetValue())
        EnableDebugDevice();
}

void Renderer::EnableDebugDevice()
{
    ComPtr<ID3D12DebugDevice1> debugDevice1;
    if(SUCCEEDED(m_Device->QueryInterface(IID_PPV_ARGS(&debugDevice1))))
    {
        uint32_t featureFlags = 0;
        if(g_D3D12DebugLayer_AllowBehaviorChangingDebugAids.GetValue())
            featureFlags |= D3D12_DEBUG_FEATURE_ALLOW_BEHAVIOR_CHANGING_DEBUG_AIDS;
        if(g_D3D12DebugLayer_ConservativeResourceStateTracking.GetValue())
            featureFlags |= D3D12_DEBUG_FEATURE_CONSERVATIVE_RESOURCE_STATE_TRACKING;
        if(g_D3D12DebugLayer_DisableVirtualizedBundlesValidation.GetValue())
            featureFlags |= D3D12_DEBUG_FEATURE_DISABLE_VIRTUALIZED_BUNDLES_VALIDATION;
        HRESULT hr = debugDevice1->SetDebugParameter(D3D12_DEBUG_DEVICE_PARAMETER_FEATURE_FLAGS,
            &featureFlags, sizeof featureFlags);
        if(FAILED(hr))
            LogWarning(L"ID3D12DebugDevice1::SetDebugParameter(D3D12_DEBUG_DEVICE_PARAMETER_FEATURE_FLAGS) failed.");

        if(g_D3D12DebugLayer_GPUBasedValidation_Enabled.GetValue())
        {
            D3D12_DEBUG_DEVICE_GPU_BASED_VALIDATION_SETTINGS GPUBasedValidationSettings = {
                .MaxMessagesPerCommandList = g_D3D12DebugLayer_GPUBasedValidation_MaxMessagesPerCommandList.GetValue(),
                .DefaultShaderPatchMode = (D3D12_GPU_BASED_VALIDATION_SHADER_PATCH_MODE)
                    g_D3D12DebugLayer_GPUBasedValidation_DefaultShaderPatchMode.GetValue(),
                .PipelineStateCreateFlags = (D3D12_GPU_BASED_VALIDATION_PIPELINE_STATE_CREATE_FLAGS)
                    g_D3D12DebugLayer_GPUBasedValidation_PipelineStateCreateFlags.GetValue() };
            hr = debugDevice1->SetDebugParameter(D3D12_DEBUG_DEVICE_PARAMETER_GPU_BASED_VALIDATION_SETTINGS,
                &GPUBasedValidationSettings, sizeof GPUBasedValidationSettings);
            if(FAILED(hr))
                LogWarning(L"ID3D12DebugDevice1::SetDebugParameter(D3D12_DEBUG_DEVICE_PARAMETER_GPU_BASED_VALIDATION_SETTINGS) failed.");
        }

        if(g_D3D12DebugLayer_SlowdownPerformanceFactor.GetValue() != 0.f)
        {
            D3D12_DEBUG_DEVICE_GPU_SLOWDOWN_PERFORMANCE_FACTOR slowdownPerformanceFactor = {
                .SlowdownFactor = g_D3D12DebugLayer_SlowdownPerformanceFactor.GetValue() };
            hr = debugDevice1->SetDebugParameter(D3D12_DEBUG_DEVICE_PARAMETER_GPU_SLOWDOWN_PERFORMANCE_FACTOR,
                &slowdownPerformanceFactor, sizeof slowdownPerformanceFactor);
            if(FAILED(hr))
                LogWarning(L"ID3D12DebugDevice1::SetDebugParameter(D3D12_DEBUG_DEVICE_PARAMETER_GPU_SLOWDOWN_PERFORMANCE_FACTOR) failed.");
        }
    }
    else
        LogWarning(L"ID3D12Device::QueryInterface for ID3D12DebugDevice1 failed.");
}

void Renderer::CreateMemoryAllocator()
{
    assert(m_Device && m_Adapter && !m_MemoryAllocator);

    D3D12MA::ALLOCATOR_DESC desc = {};
    desc.pDevice = m_Device.get();
    desc.pAdapter = m_Adapter;
    desc.Flags = D3D12MA::ALLOCATOR_FLAG_DEFAULT_POOLS_NOT_ZEROED |
        D3D12MA::ALLOCATOR_FLAG_MSAA_TEXTURES_ALWAYS_COMMITTED |
        D3D12MA::ALLOCATOR_FLAG_SINGLETHREADED;
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

    CHECK_HR(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_UploadCmdAllocator)));
    SetD3D12ObjectName(m_UploadCmdAllocator, L"Upload command allocator");

    CHECK_HR(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_UploadCmdAllocator.Get(), NULL, IID_PPV_ARGS(&m_UploadCmdList)));
	CHECK_HR(m_UploadCmdList->Close());
    SetD3D12ObjectName(m_UploadCmdList, L"Upload command list");
}

void Renderer::CreateSwapChain()
{
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {
	    .BufferDesc = {
            .Width = GetFinalResolutionU().x,
	        .Height = GetFinalResolutionU().y,
            .Format = RENDER_TARGET_FORMAT},
	    .SampleDesc = {.Count = 1},
	    .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
	    .BufferCount = g_FrameCount.GetValue(),
	    .OutputWindow = m_Wnd,
        .Windowed = TRUE,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL};//DXGI_SWAP_EFFECT_FLIP_DISCARD
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
        FrameResources& frameRes = m_FrameResources[i];

        CHECK_HR(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frameRes.m_CmdAllocator)));
        SetD3D12ObjectName(frameRes.m_CmdAllocator, std::format(L"Command allocator {}", i));

        CHECK_HR(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			frameRes.m_CmdAllocator.Get(), NULL, IID_PPV_ARGS(&frameRes.m_CmdList)));
		CHECK_HR(frameRes.m_CmdList->Close());
        SetD3D12ObjectName(frameRes.m_CmdList, std::format(L"Command list {}", i));

        ID3D12Resource* backBuffer = nullptr;
		CHECK_HR(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));
        frameRes.m_BackBuffer = std::make_unique<RenderingResource>();
        frameRes.m_BackBuffer->InitExternallyOwned(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
	}
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
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
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

	m_StandardRootSignature = std::make_unique<StandardRootSignature>();

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

    for(size_t i = 0; i < (size_t)GBuffer::Count; ++i)
    {
        D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            GBUFFER_FORMATS[i],
            GetFinalResolutionU().x, GetFinalResolutionU().y,
            1, // arraySize
            1, // mipLevels
            1, 0, // sampleCount, sampleQuality
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        m_GBuffers[i] = std::make_unique<RenderingResource>();
        m_GBuffers[i]->Init(resDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, std::format(L"GBuffer {}", GBUFFER_NAMES[i]));
    }

	CreateLightingPipelineStates();
	CreatePostprocessingPipelineState();

    ERR_CATCH_FUNC;
}

ID3D12PipelineState* Renderer::GetOrCreateGBufferPipelineState(uint32_t materialFlags)
{
    assert(m_ColorRenderTarget);

    const auto it = m_GBufferPipelineStates.find(materialFlags);
    if(it != m_GBufferPipelineStates.end())
        return it->second.Get();

    ComPtr<ID3D12PipelineState>& pso = m_GBufferPipelineStates[materialFlags];

    ERR_TRY
    ERR_TRY

    const uint32_t macroValues[] = {
        (materialFlags & Scene::Material::FLAG_ALPHA_MASK) ? 1u : 0u, // ALPHA_TEST
        (materialFlags & Scene::Material::FLAG_HAS_MATERIAL_COLOR) ? 1u : 0u, // HAS_MATERIAL_COLOR
        (materialFlags & Scene::Material::FLAG_HAS_ALBEDO_TEXTURE) ? 1u : 0u, // HAS_ALBEDO_TEXTURE
        (materialFlags & Scene::Material::FLAG_HAS_NORMAL_TEXTURE) ? 1u : 0u, // HAS_NORMAL_TEXTURE
    };

    const bool backfaceCullingEnabled = g_BackFaceCullingMode.GetValue() > 0 &&
        (materialFlags & Scene::Material::FLAG_TWOSIDED) == 0;

    Shader vs;
    vs.Init(ShaderType::Vertex, L"Shaders/GBuffer.hlsl", L"MainVS");

    const Shader* ps = m_GBufferMultiPixelShader->GetShader(macroValues);
    CHECK_BOOL(ps);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {
	    .pRootSignature = m_StandardRootSignature->GetRootSignature(),
	    .VS = {
            .pShaderBytecode = vs.GetCode().data(),
            .BytecodeLength = vs.GetCode().size()},
	    .PS = {
            .pShaderBytecode = ps->GetCode().data(),
            .BytecodeLength = ps->GetCode().size()},
        .SampleMask = UINT32_MAX,
	    .InputLayout = {
            .pInputElementDescs = Vertex::GetInputElements(),
            .NumElements = Vertex::GetInputElementCount()},
	    .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	    .NumRenderTargets = (UINT)GBuffer::Count,
	    .DSVFormat = DEPTH_STENCIL_FORMAT,
        .SampleDesc = {.Count = 1}};
    for(size_t i = 0; i < (size_t)GBuffer::Count; ++i)
	    desc.RTVFormats[i] = GBUFFER_FORMATS[i];
    FillRasterizerDesc(desc.RasterizerState,
        backfaceCullingEnabled ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_NONE,
        g_BackFaceCullingMode.GetValue() == 2 ? TRUE : FALSE);
	FillBlendDesc_NoBlending(desc.BlendState);
    FillDepthStencilDesc_DepthTest(desc.DepthStencilState, D3D12_DEPTH_WRITE_MASK_ALL, D3D12_COMPARISON_FUNC_GREATER);
	
    CHECK_HR(m_Device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso)));
    SetD3D12ObjectName(pso, std::format(L"G-buffer pipeline state Flags=0x{:X}", materialFlags));

    return pso.Get();

    ERR_CATCH_MSG(std::format(L"Cannot create G-buffer pipeline state with Flags=0x{:X}.", materialFlags));
    } CATCH_PRINT_ERROR(return nullptr;);
}

void Renderer::CreateLightingPipelineStates()
{
    m_LightingPipelineState.Reset();
    m_AmbientPipelineState.Reset();

    ERR_TRY
    ERR_TRY

    // Ambient root signature
    {
        Shader vs, ps;
        vs.Init(ShaderType::Vertex, L"Shaders/Ambient.hlsl", L"FullScreenQuadVS");
        ps.Init(ShaderType::Pixel,  L"Shaders/Ambient.hlsl", L"MainPS");

	    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {
            .pRootSignature = m_StandardRootSignature->GetRootSignature(),
	        .VS = {
                .pShaderBytecode = vs.GetCode().data(),
                .BytecodeLength = vs.GetCode().size()},
	        .PS = {
                .pShaderBytecode = ps.GetCode().data(),
                .BytecodeLength = ps.GetCode().size()},
            .SampleMask = UINT32_MAX,
	        .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	        .NumRenderTargets = 1,
            .RTVFormats = {m_ColorRenderTarget->GetDesc().Format},
	        .DSVFormat = DXGI_FORMAT_D32_FLOAT,
            .SampleDesc = {.Count = 1}};
        FillRasterizerDesc_NoCulling(desc.RasterizerState);
        FillBlendDesc_NoBlending(desc.BlendState);
        FillDepthStencilDesc_DepthTest(desc.DepthStencilState,
            D3D12_DEPTH_WRITE_MASK_ZERO, D3D12_COMPARISON_FUNC_LESS);

	    CHECK_HR(m_Device->CreateGraphicsPipelineState(&desc, __uuidof(ID3D12PipelineState), &m_AmbientPipelineState));
        SetD3D12ObjectName(m_AmbientPipelineState, L"Ambient pipeline state");
    }

    // Lighting root signature
    {
        Shader vs, ps;
        vs.Init(ShaderType::Vertex, L"Shaders/Lighting.hlsl", L"FullScreenQuadVS");
        ps.Init(ShaderType::Pixel,  L"Shaders/Lighting.hlsl", L"MainPS");

	    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {
	        .pRootSignature = m_StandardRootSignature->GetRootSignature(),
	        .VS = {
                .pShaderBytecode = vs.GetCode().data(),
                .BytecodeLength = vs.GetCode().size()},
	        .PS = {
                .pShaderBytecode = ps.GetCode().data(),
                .BytecodeLength = ps.GetCode().size()},
            .SampleMask = UINT32_MAX,
	        .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	        .NumRenderTargets = 1,
            .RTVFormats = {m_ColorRenderTarget->GetDesc().Format},
	        .DSVFormat = DXGI_FORMAT_D32_FLOAT,
            .SampleDesc = {.Count = 1}};
        FillRasterizerDesc_NoCulling(desc.RasterizerState);
        FillBlendDesc_BlendRT0(desc.BlendState,
            D3D12_BLEND_ONE, D3D12_BLEND_ONE, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ONE, D3D12_BLEND_OP_ADD);
        FillDepthStencilDesc_DepthTest(desc.DepthStencilState,
            D3D12_DEPTH_WRITE_MASK_ZERO, D3D12_COMPARISON_FUNC_LESS);

	    CHECK_HR(m_Device->CreateGraphicsPipelineState(&desc, __uuidof(ID3D12PipelineState), &m_LightingPipelineState));
        SetD3D12ObjectName(m_LightingPipelineState, L"Lighting pipeline state");
    }

    ERR_CATCH_MSG(L"Cannot create lighting pipeline states.");
    } CATCH_PRINT_ERROR(;);
}

void Renderer::CreatePostprocessingPipelineState()
{
    m_PostprocessingPipelineState.Reset();

    ERR_TRY
    ERR_TRY

    Shader vs, ps;
    vs.Init(ShaderType::Vertex, L"Shaders/Postprocessing.hlsl", L"FullScreenQuadVS");
    ps.Init(ShaderType::Pixel,  L"Shaders/Postprocessing.hlsl", L"MainPS");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {
	    .pRootSignature = m_StandardRootSignature->GetRootSignature(),
	    .VS = {
            .pShaderBytecode = vs.GetCode().data(),
            .BytecodeLength = vs.GetCode().size()},
	    .PS = {
            .pShaderBytecode = ps.GetCode().data(),
            .BytecodeLength = ps.GetCode().size()},
        .SampleMask = UINT32_MAX,
	    .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	    .NumRenderTargets = 1,
        .RTVFormats = {RENDER_TARGET_FORMAT},
	    .DSVFormat = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {.Count = 1}};
    FillRasterizerDesc_NoCulling(desc.RasterizerState);
    FillBlendDesc_NoBlending(desc.BlendState);
    FillDepthStencilDesc_NoTests(desc.DepthStencilState);

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
        L"Standard texture - Empty normal",
    };
    // We are little endian, so it is 0xAABBGGRR
    using PixelType = uint32_t;
    const PixelType COLORS[] = {
        0x00000000, 0xFF000000, 0xFF808080, 0xFFFFFFFF, 0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFF00FFFF, 0xFFFF00FF,
        0xFFFF7F80,
    };
    static_assert(_countof(NAMES) == (size_t)StandardTexture::Count);
    static_assert(_countof(COLORS) == (size_t)StandardTexture::Count);

    constexpr uint32_t SIZE = 8;
    constexpr uint32_t ROW_PITCH = std::max<uint32_t>(SIZE * sizeof(PixelType), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

    std::vector<char> buf(SIZE * ROW_PITCH * sizeof(PixelType));

    D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_UNKNOWN,
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

        resDesc.Format = textureIndex == (size_t)StandardTexture::EmptyNormal ?
            DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

        unique_ptr<Texture>& texPtr = m_StandardTextures[textureIndex];
        texPtr = std::make_unique<Texture>();
        texPtr->LoadFromMemory(resDesc, data, NAMES[textureIndex]);
    }
}

void Renderer::InitImGui()
{
    m_ImGuiDescriptor = m_SRVDescriptorManager->AllocatePersistent(1);

    CHECK_BOOL(ImGui_ImplDX12_Init(
        m_Device.get(),
        (int)g_FrameCount.GetValue(),
        RENDER_TARGET_FORMAT,
        m_SRVDescriptorManager->GetHeap(),
        m_SRVDescriptorManager->GetCPUHandle(m_ImGuiDescriptor),
        m_SRVDescriptorManager->GetGPUHandle(m_ImGuiDescriptor)));
}

void Renderer::ShutdownImGui()
{
    ImGui_ImplDX12_Shutdown();
    m_SRVDescriptorManager->FreePersistent(m_ImGuiDescriptor);
}

void Renderer::ClearModel()
{
    m_Lights.clear();
    m_Textures.clear();
    m_Materials.clear();
    m_Meshes.clear();
    m_RootEntity = Scene::Entity{};
}

void Renderer::ClearGBufferShaders()
{
    m_GBufferPipelineStates.clear();
    m_GBufferMultiPixelShader->Clear();
}

void Renderer::CreateLights()
{
    Scene::Light dl = {
        .m_Type = LIGHT_TYPE_DIRECTIONAL,
        .m_Color = g_LightColor.GetValue(),
        .m_DirectionToLight_Position = g_DirectionToLight.GetValue()
    };
    m_Lights.push_back(dl);

    Scene::Light pl0 = {
        .m_Enabled = false,
        .m_Type = LIGHT_TYPE_DIRECTIONAL,
        .m_Color = vec3(0.5f, 0.f, 0.f),
        .m_DirectionToLight_Position = vec3(-1.f, 0.f, 0.f)
    };
    m_Lights.push_back(pl0);

    Scene::Light pl1 = {
        .m_Enabled = false,
        .m_Type = LIGHT_TYPE_DIRECTIONAL,
        .m_Color = vec3(0.f, 0.5f, 0.f),
        .m_DirectionToLight_Position = vec3(0.f, 1.f, 0.f)
    };
    m_Lights.push_back(pl1);
}

void Renderer::LoadModel(bool refreshAll)
{
    ClearModel();
    CreateLights();

    /*
    RegEngine coordinate system is left-handed: X right, Y back, Z up.
    Assimp coordinate system is right-handed: X right, Y up, Z back.
    Thus, we have to:
    - Swap Y and Z components in position.s
    - Flip winding order.
    */

    const str_view filePath = g_AssimpModelPath.GetValue();
    LogMessageF(L"Loading model from \"{}\"...", filePath);

    ERR_TRY;
    ERR_TRY;

    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(filePath.c_str(), GetAssimpFlags());
        if(!scene)
            FAIL(ConvertCharsToUnicode(importer.GetErrorString(), CP_ACP));
        
        if(g_AssimpPrintSceneInfo.GetValue())
            PrintAssimpSceneInfo(scene);

        const bool globalXformIsInverted = glm::determinant(glm::mat3(g_AssimpTransform.GetValue())) < 0;

        for(uint32_t i = 0; i < scene->mNumMeshes; ++i)
            LoadModelMesh(scene, scene->mMeshes[i], globalXformIsInverted);
        
        const aiNode* node = scene->mRootNode;
        if(node)
            LoadModelNode(m_RootEntity, scene, node);

        const std::filesystem::path modelDir = std::filesystem::path(filePath.begin(), filePath.end()).parent_path();
        for(uint32_t i = 0; i < scene->mNumMaterials; ++i)
            LoadMaterial(modelDir, scene, i, scene->mMaterials[i], refreshAll);
    }

    ERR_CATCH_MSG(std::format(L"Cannot load model from \"{}\".", filePath));
    } CATCH_PRINT_ERROR(ClearModel(););
}

void Renderer::LoadModelNode(Scene::Entity& outEntity, const aiScene* scene, const aiNode* node)
{
    outEntity.m_Title = ConvertCharsToUnicode(str_view(node->mName.data, node->mName.length), CP_UTF8);

    // Matrix is Assimp is also right-to-left ordered (translation in last column, vectors are column, transform is mat * vec),
    // but it is stored as row-major instead of column-major like in GLM.
    outEntity.m_Transform = glm::transpose(glm::make_mat4(&node->mTransformation.a1));

    for(uint32_t i = 0; i < node->mNumMeshes; ++i)
        outEntity.m_Meshes.push_back((size_t)node->mMeshes[i]);
    
    for(uint32_t i = 0; i < node->mNumChildren; ++i)
    {
        unique_ptr<Scene::Entity> childEntity = std::make_unique<Scene::Entity>();
        LoadModelNode(*childEntity, scene, node->mChildren[i]);
        outEntity.m_Children.push_back(std::move(childEntity));
    }
}

void Renderer::LoadModelMesh(const aiScene* scene, const aiMesh* assimpMesh, bool globalXformIsInverted)
{
    const uint32_t vertexCount = assimpMesh->mNumVertices;
    const uint32_t faceCount = assimpMesh->mNumFaces;
    CHECK_BOOL(vertexCount > 0 && faceCount > 0);
    CHECK_BOOL(assimpMesh->mPrimitiveTypes == aiPrimitiveType_TRIANGLE);
    CHECK_BOOL(assimpMesh->HasTextureCoords(0) || !assimpMesh->HasTextureCoords(1));
    CHECK_BOOL(assimpMesh->mNumUVComponents[0] == 2 && assimpMesh->mNumUVComponents[1] == 0);
    CHECK_BOOL(assimpMesh->HasNormals() && assimpMesh->HasTangentsAndBitangents());
    const bool negateBitangent = g_AssimpNegateBitangent.GetValue();

    std::vector<Vertex> vertices(vertexCount);
    for(uint32_t i = 0; i < vertexCount; ++i)
    {
        const aiVector3D pos = assimpMesh->mVertices[i];
        const aiVector3D texCoord = assimpMesh->mTextureCoords[0][i];
        const aiVector3D normal = assimpMesh->mNormals[i];
        const aiVector3D tangent = assimpMesh->mTangents[i];
        const aiVector3D bitangent = assimpMesh->mBitangents[i];
        vertices[i].m_Position = packed_vec3(pos.x, pos.y, pos.z);
        // I thought I need to invert this when globalXformIsInverted, but apparently I don't.
        vertices[i].m_Normal = packed_vec3(normal.x, normal.y, normal.z);
        vertices[i].m_Tangent = packed_vec3(tangent.x, tangent.y, tangent.z);
        vertices[i].m_Bitangent = negateBitangent ?
            -packed_vec3(bitangent.x, bitangent.y, bitangent.z) :
            packed_vec3(bitangent.x, bitangent.y, bitangent.z);
        vertices[i].m_TexCoord = packed_vec2(texCoord.x, texCoord.y);
        vertices[i].m_Color = packed_vec4(1.f, 1.f, 1.f, 1.f);
    }
    
    std::vector<Mesh::IndexType> indices(faceCount * 3);
    uint32_t indexIndex = 0;
    for(uint32_t faceIndex = 0; faceIndex < faceCount; ++faceIndex)
    {
        assert(assimpMesh->mFaces[faceIndex].mNumIndices == 3);
        for(uint32_t j = 0; j < 3; ++j)
        {
            assert(assimpMesh->mFaces[faceIndex].mIndices[j] <= std::numeric_limits<Mesh::IndexType>::max());
            indices[indexIndex++] = (Mesh::IndexType)assimpMesh->mFaces[faceIndex].mIndices[j];
        }
    }

    if(globalXformIsInverted)
    {
        // Invert winding.
        for(size_t i = 0, count = indices.size(); i < count; i += 3)
            std::swap(indices[i], indices[i + 2]);
    }

    Scene::Mesh mesh;
    mesh.m_Title = ConvertCharsToUnicode(str_view(assimpMesh->mName.data, assimpMesh->mName.length), CP_UTF8);
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

void Renderer::LoadMaterial(const std::filesystem::path& modelDir, const aiScene* scene, uint32_t materialIndex,
    const aiMaterial* material, bool refreshAll)
{
    ERR_TRY;

    Scene::Material sceneMat;

    auto mapModesToAddressMode = [](D3D12_TEXTURE_ADDRESS_MODE &outAddressMode, const aiTextureMapMode mapModes[3]) -> bool
    {
        if(mapModes[0] == aiTextureMapMode_Wrap && mapModes[1] == aiTextureMapMode_Wrap)
        {
            outAddressMode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            return true;
        }
        if(mapModes[0] == aiTextureMapMode_Clamp && mapModes[1] == aiTextureMapMode_Clamp)
        {
            outAddressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            return true;
        }
        return false;
    };

    string albedoPath;
    string normalPath;

    // Canonical way - GetTexture.
    if(material->GetTextureCount(aiTextureType_DIFFUSE) >= 1)
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
        {
            albedoPath = s.C_Str();
            if(!mapModesToAddressMode(sceneMat.m_AlbedoTextureAddressMode, mapmodes))
                LogWarning(L"Unsupported map modes for diffuse texture.");
        }
    }
    if(material->GetTextureCount(aiTextureType_NORMALS) >= 1)
    {
        aiString s;
        aiTextureMapping mapping;
        uint32_t uvindex;
        float blend;
        aiTextureOp op;
        aiTextureMapMode mapmodes[3];
        const aiReturn ret = material->GetTexture(aiTextureType_NORMALS, 0,
            &s, &mapping, &uvindex, &blend, &op, mapmodes);
        if(ret == aiReturn_SUCCESS)
        {
            normalPath = s.C_Str();
            if(!mapModesToAddressMode(sceneMat.m_NormalTextureAddressMode, mapmodes))
                LogWarning(L"Unsupported map modes for normal texture.");
        }
    }
    // Method found in a model from Turbosquid made in Maya.
    if(albedoPath.empty())
    {
        GetStringMaterialProperty(albedoPath, material, "$raw.Maya|baseColor|file");
    }

    wstring albedoPathW;
    std::filesystem::path albedoPathP;
    if(!g_TexturePath.GetValue().empty())
    {
        albedoPathW = ConvertCharsToUnicode(g_TexturePath.GetValue(), CP_UTF8);
        // "TexturePath" setting - relative to working directory not model path!
        albedoPathP = std::filesystem::absolute(StrToPath(albedoPathW));
    }
    else if(!albedoPath.empty())
    {
        albedoPathW = ConvertCharsToUnicode(albedoPath, CP_ACP);
        albedoPathP = StrToPath(albedoPathW);
    }
    
    if(!albedoPathP.empty() && !albedoPathP.is_absolute())
        albedoPathP = modelDir / albedoPathP;
    if(!albedoPathP.empty())
    {
        sceneMat.m_AlbedoTextureIndex = TryLoadTexture(albedoPathW, albedoPathP, true, !refreshAll);
        sceneMat.m_Flags |= Scene::Material::FLAG_HAS_ALBEDO_TEXTURE;
    }

    wstring normalPathW;
    std::filesystem::path normalPathP;
    if(!g_NormalTexturePath.GetValue().empty())
    {
        normalPathW = ConvertCharsToUnicode(g_NormalTexturePath.GetValue(), CP_UTF8);
        // "NormalTexturePath" setting - relative to working directory not model path!
        normalPathP = std::filesystem::absolute(StrToPath(normalPathW));
    }
    else if(!normalPath.empty())
    {
        normalPathW = ConvertCharsToUnicode(normalPath, CP_ACP);
        normalPathP = StrToPath(normalPathW);
    }
    if(!normalPathP.empty() && !normalPathP.is_absolute())
        normalPathP = modelDir / normalPathP;
    if(!normalPathP.empty())
    {
        sceneMat.m_NormalTextureIndex = TryLoadTexture(normalPathW, normalPathP, false, !refreshAll);
        sceneMat.m_Flags |= Scene::Material::FLAG_HAS_NORMAL_TEXTURE;
    }

    // Twosided. Property found in Sponza scene, as 1-byte buffer = 0 or 1.
    {
        char buf;
        if(GetFixedBufferMaterialProperty(std::span<char>(&buf, 1), material, "$mat.twosided") &&
            buf != 0)
        {
            sceneMat.m_Flags |= Scene::Material::FLAG_TWOSIDED;
        }
    }

    // Alpha mask. Property found in Sponza scene, as strinrg = "MASK".
    {
        string s;
        if(GetStringMaterialProperty(s, material, "$mat.gltf.alphaMode"))
        {
            if(s == "MASK")
                sceneMat.m_Flags |= Scene::Material::FLAG_ALPHA_MASK;
            else if(s != "OPAQUE")
                LogWarningF(L"Unrecognized material property \"$mat.gltf.alphaMode\" value: {}", str_view(s));
        }
    }

    // Alpha cutoff.
    sceneMat.m_AlphaCutoff = 0.5f;
    if((sceneMat.m_Flags & Scene::Material::FLAG_ALPHA_MASK) != 0)
    {
        float v;
        if(GetFloatMaterialProperty(v, material, "$mat.gltf.alphaCutoff"))
            sceneMat.m_AlphaCutoff = v;
    }

    m_Materials.push_back(std::move(sceneMat));

    ERR_CATCH_MSG(std::format(L"Cannot load material {}.", materialIndex));
}

size_t Renderer::TryLoadTexture(const wstr_view& title, const std::filesystem::path& path, bool sRGB, bool allowCache)
{
    if(path.empty())
        return SIZE_MAX;

    wstring processedPath = std::filesystem::weakly_canonical(path);
    ToUpperCase(processedPath);

    // Find existing texture.
    for(size_t i = 0, count = m_Textures.size(); i < count; ++i)
    {
        if(m_Textures[i].m_ProcessedPath == processedPath)
        {
            return i;
        }
    }

    // Not found - load new texture.
    try
    {
        Scene::Texture tex;
        tex.m_Title = title;
        tex.m_ProcessedPath = std::move(processedPath);
        tex.m_Texture = std::make_unique<Texture>();
        uint32_t flags = Texture::FLAG_GENERATE_MIPMAPS | Texture::FLAG_CACHE_SAVE;
        if(sRGB)
            flags |= Texture::FLAG_SRGB;
        if(allowCache)
            flags |= Texture::FLAG_CACHE_LOAD;
        tex.m_Texture->LoadFromFile(flags, path.native());
        m_Textures.push_back(std::move(tex));
        return m_Textures.size() - 1;
    }
    CATCH_PRINT_ERROR(return SIZE_MAX;)
}

void Renderer::CreateProceduralModel()
{
    ClearModel();
    CreateLights();

    wstring normalTexturePath = ConvertCharsToUnicode(g_NormalTexturePath.GetValue(), CP_UTF8);
    Scene::Material mat;
    mat.m_NormalTextureIndex = TryLoadTexture(normalTexturePath, StrToPath(normalTexturePath), false, true);
    m_Materials.push_back(mat);

    Vertex vertices[] = {
        {
            .m_Position={-1.f, -1.f, 0.f}, .m_Normal={0.f, 0.f, 1.f}, .m_Tangent={1.f, 0.f, 0.f}, .m_Bitangent={0.f, -1.f, 0.f},
            .m_TexCoord={0.f, 0.f}, .m_Color={1.f, 1.f, 1.f, 1.f}
        },
        {
            .m_Position={ 1.f, -1.f, 0.f}, .m_Normal={0.f, 0.f, 1.f}, .m_Tangent={1.f, 0.f, 0.f}, .m_Bitangent={0.f, -1.f, 0.f},
            .m_TexCoord={1.f, 0.f}, .m_Color={1.f, 1.f, 1.f, 1.f}
        },
        {
            .m_Position={-1.f,  1.f, 0.f}, .m_Normal={0.f, 0.f, 1.f}, .m_Tangent={1.f, 0.f, 0.f}, .m_Bitangent={0.f, -1.f, 0.f},
            .m_TexCoord={0.f, 1.f}, .m_Color={1.f, 1.f, 1.f, 1.f}
        },
        {
            .m_Position={ 1.f,  1.f, 0.f}, .m_Normal={0.f, 0.f, 1.f}, .m_Tangent={1.f, 0.f, 0.f}, .m_Bitangent={0.f, -1.f, 0.f},
            .m_TexCoord={1.f, 1.f}, .m_Color={1.f, 1.f, 1.f, 1.f}
        },
    };
    Mesh::IndexType indices[] = {0, 1, 2, 2, 1, 3};

    Scene::Mesh mesh;
    mesh.m_MaterialIndex = 0;
    mesh.m_Mesh = std::make_unique<Mesh>();
    mesh.m_Mesh->Init(L"Procedural mesh", D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        vertices, indices);
    m_Meshes.push_back(std::move(mesh));

    m_RootEntity.m_Transform = glm::identity<mat4>();
    m_RootEntity.m_Meshes.push_back(0);
}

void Renderer::WaitForFenceOnCPU(UINT64 value)
{
	if(m_Fence->GetCompletedValue() < value)
	{
		CHECK_HR(m_Fence->SetEventOnCompletion(value, m_FenceEvent.get()));
		WaitForSingleObject(m_FenceEvent.get(), INFINITE);
	}
}

void Renderer::RenderEntity(CommandList& cmdList, const mat4& parentXform, const Scene::Entity& entity)
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

        cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(
            m_StandardRootSignature->GetCBVParamIndex(1),
            perObjectConstantsDescriptorHandle);

        for(size_t meshIndex : entity.m_Meshes)
            RenderEntityMesh(cmdList, entity, meshIndex);
    }

    for(const auto& childEntity : entity.m_Children)
    {
        if(childEntity->m_Visible)
            RenderEntity(cmdList, entityXform, *childEntity);
    }
}

void Renderer::RenderEntityMesh(CommandList& cmdList, const Scene::Entity& entity, size_t meshIndex)
{
    const Mesh* const mesh = m_Meshes[meshIndex].m_Mesh.get();
    const size_t materialIndex = m_Meshes[meshIndex].m_MaterialIndex;
    assert(materialIndex < m_Materials.size());
    const Scene::Material& mat = m_Materials[materialIndex];

    uint32_t materialFlags = mat.m_Flags;
    if(!g_BackfaceCullingEnabled.GetValue())
        materialFlags |= Scene::Material::FLAG_TWOSIDED;
    if(!g_AlbedoTexturesEnabled.GetValue())
        materialFlags &= ~Scene::Material::FLAG_HAS_ALBEDO_TEXTURE;
    if(!g_NormalMapsEnabled.GetValue())
        materialFlags &= ~Scene::Material::FLAG_HAS_NORMAL_TEXTURE;

    ID3D12PipelineState* const pso = GetOrCreateGBufferPipelineState(materialFlags);

    if(!pso)
        return;
    cmdList.SetPipelineState(pso);

    // Per material constant buffer
    {
        PerMaterialConstants perMaterialConstants = {};
        perMaterialConstants.m_Flags = 0;
        if((materialFlags & Scene::Material::FLAG_TWOSIDED) != 0)
            perMaterialConstants.m_Flags |= MATERIAL_FLAG_TWOSIDED;
        if((materialFlags & Scene::Material::FLAG_ALPHA_MASK) != 0)
        {
            perMaterialConstants.m_Flags |= MATERIAL_FLAG_ALPHA_MASK;
            perMaterialConstants.m_AlphaCutoff = mat.m_AlphaCutoff;
        }
        if((materialFlags & Scene::Material::FLAG_HAS_MATERIAL_COLOR) != 0)
        {
            perMaterialConstants.m_Flags |= MATERIAL_FLAG_HAS_MATERIAL_COLOR;
            perMaterialConstants.m_Color = mat.m_Color;
        }
        if((materialFlags & Scene::Material::FLAG_HAS_ALBEDO_TEXTURE) != 0)
            perMaterialConstants.m_Flags |= MATERIAL_FLAG_HAS_ALBEDO_TEXTURE;
        if((materialFlags & Scene::Material::FLAG_HAS_NORMAL_TEXTURE) != 0)
            perMaterialConstants.m_Flags |= MATERIAL_FLAG_HAS_NORMAL_TEXTURE;

        void* perMaterialConstantsPtr = nullptr;
        D3D12_GPU_DESCRIPTOR_HANDLE perMaterialConstantsDescriptorHandle;
        m_TemporaryConstantBufferManager->CreateBuffer(sizeof(perMaterialConstants), perMaterialConstantsPtr, perMaterialConstantsDescriptorHandle);
        memcpy(perMaterialConstantsPtr, &perMaterialConstants, sizeof(perMaterialConstants));

        cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(
            m_StandardRootSignature->GetCBVParamIndex(2),
            perMaterialConstantsDescriptorHandle);
    }

    if((materialFlags & Scene::Material::FLAG_HAS_ALBEDO_TEXTURE) != 0)
    {
        Texture* albedoTexture = (mat.m_AlbedoTextureIndex != SIZE_MAX && g_AlbedoTexturesEnabled.GetValue()) ?
            m_Textures[mat.m_AlbedoTextureIndex].m_Texture.get() : nullptr;
        D3D12_GPU_DESCRIPTOR_HANDLE albedoTextureDescriptorHandle;
        if(albedoTexture)
        {
            albedoTextureDescriptorHandle = m_SRVDescriptorManager->GetGPUHandle(
                m_Textures[mat.m_AlbedoTextureIndex].m_Texture->GetDescriptor());
        }
        else
        {
            albedoTextureDescriptorHandle = m_SRVDescriptorManager->GetGPUHandle(
                m_StandardTextures[(size_t)StandardTexture::Gray]->GetDescriptor());
        }
        cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(
            m_StandardRootSignature->GetSRVParamIndex(0), albedoTextureDescriptorHandle);
        cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(
            m_StandardRootSignature->GetSamplerParamIndex(0),
            m_StandardSamplers.GetD3D12(D3D12_FILTER_ANISOTROPIC, mat.m_AlbedoTextureAddressMode));
    }

    if((materialFlags & Scene::Material::FLAG_HAS_NORMAL_TEXTURE) != 0)
    {
        Texture* normalTexture = mat.m_NormalTextureIndex != SIZE_MAX ?
            m_Textures[mat.m_NormalTextureIndex].m_Texture.get() : nullptr;
        D3D12_GPU_DESCRIPTOR_HANDLE normalTextureDescriptorHandle;
        if(normalTexture)
        {
            normalTextureDescriptorHandle = m_SRVDescriptorManager->GetGPUHandle(
                m_Textures[mat.m_NormalTextureIndex].m_Texture->GetDescriptor());
        }
        else
        {
            normalTextureDescriptorHandle = m_SRVDescriptorManager->GetGPUHandle(
                m_StandardTextures[(size_t)StandardTexture::EmptyNormal]->GetDescriptor());
        }
        cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(
            m_StandardRootSignature->GetSRVParamIndex(1), normalTextureDescriptorHandle);
        cmdList.GetCmdList()->SetGraphicsRootDescriptorTable(
            m_StandardRootSignature->GetSamplerParamIndex(1),
            m_StandardSamplers.GetD3D12(D3D12_FILTER_ANISOTROPIC, mat.m_NormalTextureAddressMode));
    }

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

void Renderer::SaveD3D12MAJSONDump()
{
    LogMessage(L"Saving D3D12 Memory Allocator JSON dump...");
    wchar_t* json;
    m_MemoryAllocator->BuildStatsString(&json, TRUE);
    try
    {
        wstring dateTime = CurrentDateTime();
        // Convert to file-name-friendly "YYYY-MM-DD HH.MM.SS"
        dateTime[13] = L'.';
        dateTime[16] = L'.';
        wstring filePath = std::format(L"D3D12MA {} {}.json", dateTime, m_NextD3D12MAJSONDumpIndex++);
        SaveFile(filePath, std::span<const char>{(const char*)json, wcslen(json) * sizeof(wchar_t)});
    }
    CATCH_PRINT_ERROR(;)
    m_MemoryAllocator->FreeStatsString(json);
}

#endif // _RENDERER_IMPL

#ifndef _STANDARD_SAMPLERS_IMPL

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

#endif // #ifndef _STANDARD_SAMPLERS_IMPL

void AssimpPrint(const wstr_view& filePath)
{
    LogMessageF(L"Loading model from \"{}\"...", filePath);

    ERR_TRY;

    AssimpInit assimp;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        ConvertUnicodeToChars(filePath, CP_ACP).c_str(), GetAssimpFlags());
    if(!scene)
        FAIL(ConvertCharsToUnicode(importer.GetErrorString(), CP_ACP));
        
    PrintAssimpSceneInfo(scene);

    ERR_CATCH_MSG(std::format(L"Cannot load model from \"{}\".", filePath));
}

void D3D12DeviceDeleter::operator()(ID3D12Device* device)
{
    if(!device)
        return;

    if(g_D3D12DebugLayer_Enabled.GetValue() &&
        g_D3D12DebugLayer_ReportLiveDeviceObjects.GetValue())
    {
        ComPtr<ID3D12DebugDevice> debugDevice;
        if(SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&debugDevice))))
        {
            debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
        }
    }

    device->Release();
}
