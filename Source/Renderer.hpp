#pragma once

#include "Descriptors.hpp"
#include <unordered_map>

class AssimpInit;

class CommandList;
class RenderingResource;
class Texture;
class Mesh;
class TemporaryConstantBufferManager;
class Shader;
class MultiShader;
class ShaderCompiler;
class OrbitingCamera;
class FlyingCamera;

struct aiScene;
struct aiNode;
struct aiMesh;
struct aiMaterial;

enum class GBuffer
{
    Albedo,
    Normal,
    Count
};

enum class StandardTexture
{
    Transparent, Black, Gray, White, Red, Green, Blue, Yellow, Fuchsia, EmptyNormal, Count
};

struct RendererCapabilities
{
};

struct D3D12DeviceDeleter
{
    void operator()(ID3D12Device* device);
};
using D3D12DevicePtr = unique_ptr<ID3D12Device, D3D12DeviceDeleter>;

/*
The only accepted values are:
    D3D12_FILTER_MIN_MAG_MIP_POINT
    D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT
    D3D12_FILTER_MIN_MAG_MIP_LINEAR
    D3D12_FILTER_ANISOTROPIC
X
    D3D12_TEXTURE_ADDRESS_MODE_WRAP
    D3D12_TEXTURE_ADDRESS_MODE_CLAMP
*/
class StandardSamplers
{
public:
    void Init();
    ~StandardSamplers();
    
    Descriptor Get(D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE address) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetD3D12(D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE address) const;

private:
    static constexpr size_t COUNT = 4 * 2;
    Descriptor m_Descriptors;
};

namespace Scene
{

struct Texture
{
    // Just for display to the user.
    wstring m_Title;
    wstring m_ProcessedPath;
    // Can be null - use some standard texture then.
    unique_ptr<::Texture> m_Texture;
};

struct Material
{
    enum FLAG
    {
        FLAG_TWOSIDED = 0x1,
        FLAG_ALPHA_MASK = 0x2,
        FLAG_HAS_MATERIAL_COLOR = 0x4,
        FLAG_HAS_ALBEDO_TEXTURE = 0x8,
        FLAG_HAS_NORMAL_TEXTURE = 0x10,
    };

    uint32_t m_Flags = 0;
    packed_vec3 m_Color = packed_vec3(1.f, 1.f, 1.f);
    size_t m_AlbedoTextureIndex = SIZE_MAX;
    size_t m_NormalTextureIndex = SIZE_MAX;
    // Use only WRAP or CLAMP.
    D3D12_TEXTURE_ADDRESS_MODE m_AlbedoTextureAddressMode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    D3D12_TEXTURE_ADDRESS_MODE m_NormalTextureAddressMode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    // Valid only when m_Flags & FLAG_ALPHA_MASK.
    // Pixels with albedo.a < m_AlphaCutoff are discarded, according to GLTF specification.
    float m_AlphaCutoff = 0.5f;
};

struct Light
{
    bool m_Enabled = true;
    // Use LIGHT_TYPE_* from ShaderConstants.h
    uint32_t m_Type = 0;
    vec3 m_Color; // Linear space
    // LIGHT_TYPE_DIRECTIONAL: direction to light (world space)
    // LIGHT_TYPE_POINT: position (world space)
    vec3 m_DirectionToLight_Position;
};

struct Mesh
{
    wstring m_Title;
    unique_ptr<::Mesh> m_Mesh;
    size_t m_MaterialIndex = SIZE_MAX;
};

struct Entity
{
    wstring m_Title;
    mat4 m_Transform = glm::identity<mat4>();
    bool m_Visible = true;
    std::vector<unique_ptr<Entity>> m_Children;
    std::vector<size_t> m_Meshes; // Indices into Renderer::m_Meshes.
};

} // namespace Scene

/*
It offers:
- CBV b0..b7, each separate DESCRIPTOR_TABLE, visible to ALL shader stages.
- SRV t0..t7, each separate DESCRIPTOR_TABLE, visible to PIXEL shader stage.
- Sampler s0..s3, each separate DESCRIPTOR_TABLE, visible to PIXEL shader stages.

Flags:
ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
DENY_HULL_SHADER_ROOT_ACCESS
DENY_DOMAIN_SHADER_ROOT_ACCESS
DENY_GEOMETRY_SHADER_ROOT_ACCESS
*/
class StandardRootSignature
{
public:
    StandardRootSignature();
    ID3D12RootSignature* GetRootSignature() const { return m_RootSignature.Get(); }
    static uint32_t GetCBVParamIndex(uint32_t CBVIndex) { return CBVIndex; }
    static uint32_t GetSRVParamIndex(uint32_t SRVIndex) { return SRVIndex + CBV_COUNT; }
    static uint32_t GetSamplerParamIndex(uint32_t samplerIndex) { return samplerIndex + CBV_COUNT + SRV_COUNT; }

private:
    static constexpr uint32_t CBV_COUNT = 8;
    static constexpr uint32_t SRV_COUNT = 8;
    static constexpr uint32_t SAMPLER_COUNT = 4;

    ComPtr<ID3D12RootSignature> m_RootSignature;
};

/*
Represents the main object responsible for rendering graphics.
It creates and keeps ID3D12Device and other key objects.
*/
class Renderer
{
public:
    bool m_AmbientEnabled = false;
    std::vector<Scene::Light> m_Lights;
    Scene::Entity m_RootEntity;
    std::vector<Scene::Mesh> m_Meshes;
    std::vector<Scene::Material> m_Materials;
    std::vector<Scene::Texture> m_Textures;

	Renderer(IDXGIFactory4* dxgiFactory, IDXGIAdapter1* adapter, HWND wnd);
	void Init();
	~Renderer();
    void Reload(bool refreshAll);

    ID3D12Device* GetDevice() { return m_Device.get(); };
    ID3D12Device1* GetDevice1() { return m_Device1.Get(); };
    const RendererCapabilities& GetCapabilities() { return m_Capabilities; }
    D3D12MA::Allocator* GetMemoryAllocator() { return m_MemoryAllocator.Get(); };
    DescriptorManager* GetSRVDescriptorManager() { return m_SRVDescriptorManager.get(); }
    DescriptorManager* GetSamplerDescriptorManager() { return m_SamplerDescriptorManager.get(); }
    DescriptorManager* GetRTVDescriptorManager() { return m_RTVDescriptorManager.get(); }
    DescriptorManager* GetDSVDescriptorManager() { return m_DSVDescriptorManager.get(); }
    TemporaryConstantBufferManager* GetTemporaryConstantBufferManager() { return m_TemporaryConstantBufferManager.get(); }
    StandardSamplers* GetStandardSamplers() { return &m_StandardSamplers; }
    ShaderCompiler* GetShaderCompiler() { return m_ShaderCompiler.get(); }
    FlyingCamera* GetCamera() { return m_Camera.get(); }

    uvec2 GetFinalResolutionU();
    vec2 GetFinalResolutionF();

    void BeginUploadCommandList(CommandList& dstCmdList);
    // Closes, submits, and waits for the upload command list on the CPU to finish.
    void CompleteUploadCommandList(CommandList& cmdList);

    void ImGui_D3D12MAStatistics();
	void Render();

private:
	struct FrameResources
	{
        ComPtr<ID3D12CommandAllocator> m_CmdAllocator;
		ComPtr<ID3D12GraphicsCommandList> m_CmdList;
		unique_ptr<RenderingResource> m_BackBuffer;
		UINT64 m_SubmittedFenceValue = 0;
	};

	IDXGIFactory4* const m_DXGIFactory;
	IDXGIAdapter1* const m_Adapter;
	const HWND m_Wnd;

	D3D12DevicePtr m_Device;
	ComPtr<ID3D12Device1> m_Device1;
	RendererCapabilities m_Capabilities;
    ComPtr<D3D12MA::Allocator> m_MemoryAllocator;
	ComPtr<ID3D12CommandQueue> m_CmdQueue;
    ComPtr<ID3D12CommandAllocator> m_UploadCmdAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_UploadCmdList;
    UINT64 m_UploadCmdListSubmittedFenceValue = 0;
	ComPtr<ID3D12Fence> m_Fence;
	UINT64 m_NextFenceValue = 1;
	ComPtr<IDXGISwapChain3> m_SwapChain;
    unique_ptr<DescriptorManager> m_SRVDescriptorManager;
    unique_ptr<DescriptorManager> m_SamplerDescriptorManager;
    unique_ptr<DescriptorManager> m_RTVDescriptorManager;
    unique_ptr<DescriptorManager> m_DSVDescriptorManager;
    unique_ptr<TemporaryConstantBufferManager> m_TemporaryConstantBufferManager;
    StandardSamplers m_StandardSamplers;
    unique_ptr<ShaderCompiler> m_ShaderCompiler;
	std::array<FrameResources, MAX_FRAME_COUNT> m_FrameResources;
	unique_ptr<RenderingResource> m_DepthTexture;
	UINT m_FrameIndex = UINT32_MAX;
	unique_ptr<HANDLE, CloseHandleDeleter> m_FenceEvent;
    unique_ptr<Texture> m_StandardTextures[(size_t)StandardTexture::Count];
    unique_ptr<RenderingResource> m_GBuffers[(size_t)GBuffer::Count];
    unique_ptr<RenderingResource> m_ColorRenderTarget;
    unique_ptr<StandardRootSignature> m_StandardRootSignature;
    unique_ptr<MultiShader> m_GBufferMultiPixelShader;
    uint32_t m_NextD3D12MAJSONDumpIndex = 0;

    // Key is combination of Scene::Material::FLAG_*.
    // Value null means the PSO couldn't be created due to error, which has been printed to the log.
    using GBufferPipelineStateMapType = std::unordered_map<uint32_t, ComPtr<ID3D12PipelineState>>;
    GBufferPipelineStateMapType m_GBufferPipelineStates;
    
    unique_ptr<AssimpInit> m_AssimpInit;
    unique_ptr<FlyingCamera> m_Camera;
	ComPtr<ID3D12PipelineState> m_AmbientPipelineState;
	ComPtr<ID3D12PipelineState> m_LightingPipelineState;
	ComPtr<ID3D12PipelineState> m_PostprocessingPipelineState;
    Descriptor m_ImGuiDescriptor;

    void EnableDebugLayer();
	void CreateDevice();
	void CreateMemoryAllocator();
	void LoadCapabilities();
	void CreateCommandQueues();
	void CreateSwapChain();
	void CreateFrameResources();
	void CreateResources();
    ID3D12PipelineState* GetOrCreateGBufferPipelineState(uint32_t materialFlags);
    void CreateLightingPipelineStates();
    void CreatePostprocessingPipelineState();
    void CreateStandardTextures();
    void InitImGui();
    void ShutdownImGui();
    void ClearModel();
    void ClearGBufferShaders();
    void CreateLights();
    void LoadModel(bool refreshAll);
    void LoadModelNode(Scene::Entity& outEntity, const aiScene* scene, const aiNode* node);
    // Always pushes one new object to m_Meshes.
    void LoadModelMesh(const aiScene* scene, const aiMesh* assimpMesh, bool globalXformIsInverted);
    void LoadMaterial(const std::filesystem::path& modelDir, const aiScene* scene, uint32_t materialIndex,
        const aiMaterial* material, bool refreshAll);
    // Returns index of the existing or newly loaded texture in m_Textures, SIZE_MAX if failed.
    size_t TryLoadTexture(const wstr_view& title, const std::filesystem::path& path, bool sRGB, bool allowCache);
    void CreateProceduralModel();

    void WaitForFenceOnCPU(UINT64 value);

    void RenderEntity(CommandList& cmdList, const mat4& parentXform, const Scene::Entity& entity);
    void RenderEntityMesh(CommandList& cmdList, const Scene::Entity& entity, size_t meshIndex);
    void SaveD3D12MAJSONDump();
};

extern Renderer* g_Renderer;

void AssimpPrint(const wstr_view& filePath);
