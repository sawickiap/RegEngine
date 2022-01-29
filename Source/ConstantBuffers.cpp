#include "BaseUtils.hpp"
#include "ConstantBuffers.hpp"
#include "Descriptors.hpp"
#include "Renderer.hpp"
#include "Settings.hpp"

constexpr uint32_t ALIGNMENT = 256;

extern UintSetting g_FrameCount;

static UintSetting g_TemporaryConstantBuffereMaxSizePerFrame(SettingCategory::Startup,
    "ConstantBuffers.Temporary.MaxSizePerFrame", 0);

void TemporaryConstantBufferManager::Init()
{
    assert(g_Renderer);

    CHECK_BOOL(g_TemporaryConstantBuffereMaxSizePerFrame.GetValue() > 0 &&
        g_TemporaryConstantBuffereMaxSizePerFrame.GetValue() % 32 == 0);

    const uint32_t bufSize = g_TemporaryConstantBuffereMaxSizePerFrame.GetValue() * g_FrameCount.GetValue();

    m_RingBuffer.Init(bufSize, g_FrameCount.GetValue());

    // Create m_Buffer.
    {
        D3D12MA::ALLOCATION_DESC allocDesc = {};
        allocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
        const D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufSize);
        CHECK_HR(g_Renderer->GetMemoryAllocator()->CreateResource(&allocDesc, &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, // pOptimizedClearValue
            &m_Buffer,
            IID_NULL, nullptr)); // riidResource, ppvResource
    }

    // Persistently map m_Buffer.
    CHECK_HR(m_Buffer->GetResource()->Map(0, D3D12_RANGE_NONE, &m_BufferMappedPtr));
}

TemporaryConstantBufferManager::~TemporaryConstantBufferManager()
{
    if(m_BufferMappedPtr)
        m_Buffer->GetResource()->Unmap(0, D3D12_RANGE_ALL);
}

void TemporaryConstantBufferManager::NewFrame()
{
    m_RingBuffer.NewFrame();
}

void TemporaryConstantBufferManager::CreateBuffer(uint32_t size,
    void*& outMappedPtr, D3D12_GPU_VIRTUAL_ADDRESS& outGPUAddr)
{
    const uint32_t alignedSize = AlignUp(size, ALIGNMENT);

    uint32_t newBufOffset = 0;
    CHECK_BOOL(m_RingBuffer.Allocate(alignedSize, newBufOffset));

    outMappedPtr = (char*)m_BufferMappedPtr + newBufOffset;
    
    // GPU VA of the newly allocated data inside m_Buffer.
    outGPUAddr = m_Buffer->GetResource()->GetGPUVirtualAddress() + newBufOffset;
}

void TemporaryConstantBufferManager::CreateBuffer(uint32_t size,
    void*& outMappedPtr, D3D12_GPU_DESCRIPTOR_HANDLE& outCBVDescriptorHandle)
{
    const uint32_t alignedSize = AlignUp(size, ALIGNMENT);

    // GPU VA of the newly allocated data inside m_Buffer.
    D3D12_GPU_VIRTUAL_ADDRESS newBufGPUVA;
    CreateBuffer(alignedSize, outMappedPtr, newBufGPUVA);
    
    ShaderResourceDescriptorManager* const descMgr = g_Renderer->GetShaderResourceDescriptorManager();
    const ShaderResourceDescriptor descriptor = descMgr->AllocateTemporaryDescriptor(1);

    D3D12_CONSTANT_BUFFER_VIEW_DESC CBVDesc = {};
    CBVDesc.BufferLocation = newBufGPUVA;
    CBVDesc.SizeInBytes = alignedSize;
    g_Renderer->GetDevice()->CreateConstantBufferView(&CBVDesc, descMgr->GetDescriptorCPUHandle(descriptor));
    outCBVDescriptorHandle = descMgr->GetDescriptorGPUHandle(descriptor);
}
