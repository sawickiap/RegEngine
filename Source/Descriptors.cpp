#include "BaseUtils.hpp"
#include "Descriptors.hpp"
#include "Renderer.hpp"
#include "Settings.hpp"

extern UintSetting g_FrameCount;

static UintSetting g_PersistentDescriptorMaxCount(SettingCategory::Startup,
    "ShaderResourceDescriptors.Persistent.MaxCount", 0);
static UintSetting g_TemporaryDescriptorMaxCountPerFrame(SettingCategory::Startup,
    "ShaderResourceDescriptors.Temporary.MaxCountPerFrame", 0);

void ShaderResourceDescriptorManager::Init()
{
    assert(g_Renderer);

    CHECK_BOOL(g_PersistentDescriptorMaxCount.GetValue() > 0);
    CHECK_BOOL(g_TemporaryDescriptorMaxCountPerFrame.GetValue() > 0);
    m_TemporaryDescriptorOffset = g_PersistentDescriptorMaxCount.GetValue();

    // Create m_DescriptorHeap.
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = m_TemporaryDescriptorOffset +
            g_TemporaryDescriptorMaxCountPerFrame.GetValue() * g_FrameCount.GetValue();
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        CHECK_HR(g_Renderer->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_DescriptorHeap)));
        SetD3D12ObjectName(m_DescriptorHeap, L"Shader resource descriptor heap");

        m_GPUHandleForHeapStart = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        m_CPUHandleForHeapStart = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        m_DescriptorSize = g_Renderer->GetCapabilities().m_DescriptorSize_CVB_SRV_UAV;
    }

    // Create m_VirtualBlock.
    {
        D3D12MA::VIRTUAL_BLOCK_DESC desc = {};
        desc.Size = g_PersistentDescriptorMaxCount.GetValue();
        CHECK_HR(D3D12MA::CreateVirtualBlock(&desc, &m_VirtualBlock));
    }
}

void ShaderResourceDescriptorManager::NewFrame()
{
    ++m_FrameIndex;
    m_TemporaryDescriptorCountInCurrentFrame = 0;
}

ShaderResourceDescriptor ShaderResourceDescriptorManager::AllocatePersistentDescriptor(uint32_t descriptorCount)
{
    D3D12MA::VIRTUAL_ALLOCATION_DESC virtualAllocDesc = {};
    virtualAllocDesc.Size = descriptorCount;
    ShaderResourceDescriptor descriptor;
    CHECK_HR(m_VirtualBlock->Allocate(&virtualAllocDesc, &descriptor.m_Index));
    return descriptor;
}

ShaderResourceDescriptor ShaderResourceDescriptorManager::AllocateTemporaryDescriptor(uint32_t descriptorCount)
{
    CHECK_BOOL(m_TemporaryDescriptorCountInCurrentFrame + descriptorCount <= g_TemporaryDescriptorMaxCountPerFrame.GetValue());
    const uint64_t currFrameDescriptorOffset = m_TemporaryDescriptorOffset +
        m_FrameIndex % g_FrameCount.GetValue() * g_TemporaryDescriptorMaxCountPerFrame.GetValue();
    ShaderResourceDescriptor descriptor = {
        currFrameDescriptorOffset + m_TemporaryDescriptorCountInCurrentFrame };
    m_TemporaryDescriptorCountInCurrentFrame += descriptorCount;
    return descriptor;
}

void ShaderResourceDescriptorManager::FreePersistentDescriptor(ShaderResourceDescriptor desc)
{
    m_VirtualBlock->FreeAllocation(desc.m_Index);
}

D3D12_GPU_DESCRIPTOR_HANDLE ShaderResourceDescriptorManager::GetDescriptorGPUHandle(ShaderResourceDescriptor desc)
{
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_GPUHandleForHeapStart,
        (int32_t)desc.m_Index, // offsetInDescriptors
        m_DescriptorSize); // descriptorIncrementSize
}

D3D12_CPU_DESCRIPTOR_HANDLE ShaderResourceDescriptorManager::GetDescriptorCPUHandle(ShaderResourceDescriptor desc)
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_CPUHandleForHeapStart,
        (int32_t)desc.m_Index, // offsetInDescriptors
        m_DescriptorSize); // descriptorIncrementSize
}
