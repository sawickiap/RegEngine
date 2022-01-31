#pragma once

class Font;
class AssimpInit;

class CommandList;
class RenderingResource;
class Texture;
class Mesh;
struct ShaderResourceDescriptor;
class ShaderResourceDescriptorManager;
class TemporaryConstantBufferManager;
class ShaderCompiler;

struct aiScene;
struct aiNode;
struct aiMesh;
struct aiMaterial;

enum class StandardTexture
{
    Transparent, Black, Gray, White, Red, Green, Blue, Yellow, Fuchsia, Count
};

struct RendererCapabilities
{
	UINT m_DescriptorSize_CVB_SRV_UAV = UINT32_MAX;
	UINT m_DescriptorSize_Sampler = UINT32_MAX;
	UINT m_DescriptorSize_RTV = UINT32_MAX;
	UINT m_DescriptorSize_DSV = UINT32_MAX;
};

struct Entity
{
    mat4 m_Transform = glm::identity<mat4>();
    std::vector<unique_ptr<Entity>> m_Children;
    std::vector<size_t> m_Meshes; // Indices into Renderer::m_Meshes.
};

class Renderer
{
public:
	Renderer(IDXGIFactory4* dxgiFactory, IDXGIAdapter1* adapter, HWND wnd);
	void Init();
	~Renderer();
    void Reload();

    ID3D12Device* GetDevice() { return m_Device.Get(); };
    const RendererCapabilities& GetCapabilities() { return m_Capabilities; }
    D3D12MA::Allocator* GetMemoryAllocator() { return m_MemoryAllocator.Get(); };
    ID3D12CommandAllocator* GetCmdAllocator() { return m_CmdAllocator.Get(); }
    ShaderResourceDescriptorManager* GetShaderResourceDescriptorManager() { return m_ShaderResourceDescriptorManager.get(); }
    TemporaryConstantBufferManager* GetTemporaryConstantBufferManager() { return m_TemporaryConstantBufferManager.get(); }
    ShaderCompiler* GetShaderCompiler() { return m_ShaderCompiler.get(); }
    void BeginUploadCommandList(CommandList& dstCmdList);
    // Closes, submits, and waits for the upload command list on the CPU to finish.
    void CompleteUploadCommandList(CommandList& cmdList);

	void Render();

private:
	struct FrameResources
	{
		ComPtr<ID3D12GraphicsCommandList> m_CmdList;
		unique_ptr<RenderingResource> m_BackBuffer;
		UINT64 m_SubmittedFenceValue = 0;
	};

	IDXGIFactory4* const m_DXGIFactory;
	IDXGIAdapter1* const m_Adapter;
	const HWND m_Wnd;

	ComPtr<ID3D12Device> m_Device;
	RendererCapabilities m_Capabilities;
    ComPtr<D3D12MA::Allocator> m_MemoryAllocator;
	ComPtr<ID3D12CommandQueue> m_CmdQueue;
    ComPtr<ID3D12CommandAllocator> m_CmdAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_UploadCmdList;
    UINT64 m_UploadCmdListSubmittedFenceValue = 0;
	ComPtr<ID3D12Fence> m_Fence;
	UINT64 m_NextFenceValue = 1;
	ComPtr<IDXGISwapChain3> m_SwapChain;
	ComPtr<ID3D12DescriptorHeap> m_SwapChainRTVDescriptors;
	unique_ptr<RenderingResource> m_DepthTexture;
	ComPtr<ID3D12DescriptorHeap> m_DSVDescriptor;
    unique_ptr<ShaderResourceDescriptorManager> m_ShaderResourceDescriptorManager;
    unique_ptr<TemporaryConstantBufferManager> m_TemporaryConstantBufferManager;
    unique_ptr<ShaderCompiler> m_ShaderCompiler;
	std::array<FrameResources, MAX_FRAME_COUNT> m_FrameResources;
	UINT m_FrameIndex = UINT32_MAX;
	unique_ptr<HANDLE, CloseHandleDeleter> m_FenceEvent;
    unique_ptr<Texture> m_StandardTextures[(size_t)StandardTexture::Count];
	ComPtr<ID3D12RootSignature> m_RootSignature;
	ComPtr<ID3D12PipelineState> m_PipelineState;
    unique_ptr<Font> m_Font;
    unique_ptr<AssimpInit> m_AssimpInit;

    std::vector<unique_ptr<Mesh>> m_Meshes;
    // Indices of this array match m_Meshes.
    // Values are indices into m_Textures.
    std::vector<size_t> m_MeshMaterialIndices;
    Entity m_RootEntity;
    // Indices to this array are material indices from the Assimp scene.
    // Elements can be null - use some standard texture then.
    std::vector<unique_ptr<Texture>> m_Textures;

    mat4 m_ViewProj;

	void CreateDevice();
	void CreateMemoryAllocator();
	void LoadCapabilities();
	void CreateCommandQueues();
	void CreateSwapChain();
	void CreateFrameResources();
	void CreateResources();
    void CreatePipelineState();
    void CreateStandardTextures();
    void ClearModel();
    void LoadModel();
    void LoadModelNode(Entity& outEntity, const aiScene* scene, const aiNode* node);
    // Always pushes one new object to m_Meshes.
    void LoadModelMesh(const aiScene* scene, const aiMesh* assimpMesh);
    void LoadMaterial(const std::filesystem::path& modelDir, const aiScene* scene, uint32_t materialIndex, const aiMaterial* material);

    void WaitForFenceOnCPU(UINT64 value);

    void RenderEntity(CommandList& cmdList, const mat4& parentXform, const Entity& entity);
    void RenderEntityMesh(CommandList& cmdList, const Entity& entity, size_t meshIndex);
};

extern Renderer* g_Renderer;
