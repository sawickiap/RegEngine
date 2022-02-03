#include "BaseUtils.hpp"
#include "Descriptors.hpp"
#include "Renderer.hpp"
#include "Settings.hpp"

extern UintSetting g_FrameCount;

static const wchar_t* DESCRIPTOR_HEAP_TYPE_NAMES[] = {
    L"CBV_SRV_UAV", L"SAMPLER", L"RTV", L"DSV",
};
static_assert(_countof(DESCRIPTOR_HEAP_TYPE_NAMES) == (size_t)D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);

void DescriptorManager::Init(
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    uint32_t persistentDescriptorMaxCount,
    uint32_t temporaryDescriptorMaxCountPerFrame)
{
    assert(g_Renderer);
    assert(persistentDescriptorMaxCount > 0 || temporaryDescriptorMaxCountPerFrame > 0);

    m_Type = type;
    m_PersistentDescriptorMaxCount = persistentDescriptorMaxCount;
    m_TemporaryDescriptorMaxCountPerFrame = temporaryDescriptorMaxCountPerFrame;
    
    if(temporaryDescriptorMaxCountPerFrame)
    {
        m_TemporaryDescriptorRingBuffer.Init(
            temporaryDescriptorMaxCountPerFrame * g_FrameCount.GetValue(),
            g_FrameCount.GetValue());
    }

    // Create m_DescriptorHeap.
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = type;
        desc.NumDescriptors = persistentDescriptorMaxCount +
            temporaryDescriptorMaxCountPerFrame * g_FrameCount.GetValue();
        if(type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        CHECK_HR(g_Renderer->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_DescriptorHeap)));
        SetD3D12ObjectName(m_DescriptorHeap, std::format(L"{} descriptor heap",
            DESCRIPTOR_HEAP_TYPE_NAMES[(uint32_t)type]));

        m_GPUHandleForHeapStart = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        m_CPUHandleForHeapStart = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        m_DescriptorSize = g_Renderer->GetDevice()->GetDescriptorHandleIncrementSize(type);
    }

    // Create m_VirtualBlock.
    if(persistentDescriptorMaxCount)
    {
        D3D12MA::VIRTUAL_BLOCK_DESC desc = {};
        desc.Size = persistentDescriptorMaxCount;
        CHECK_HR(D3D12MA::CreateVirtualBlock(&desc, &m_VirtualBlock));
    }
}

DescriptorManager::~DescriptorManager()
{
    if(m_VirtualBlock)
    {
        D3D12MA::StatInfo stats = {};
        m_VirtualBlock->CalculateStats(&stats);
        assert(stats.AllocationCount == 0 && "Unfreed persistent descriptors. Inspect m_Type to check their type.");
    }
}

void DescriptorManager::NewFrame()
{
    if(m_TemporaryDescriptorMaxCountPerFrame > 0)
        m_TemporaryDescriptorRingBuffer.NewFrame();
}

Descriptor DescriptorManager::AllocatePersistent(uint32_t descriptorCount)
{
    assert(m_PersistentDescriptorMaxCount);
    D3D12MA::VIRTUAL_ALLOCATION_DESC virtualAllocDesc = {};
    virtualAllocDesc.Size = descriptorCount;
    Descriptor descriptor;
    CHECK_HR(m_VirtualBlock->Allocate(&virtualAllocDesc, &descriptor.m_Index));
    return descriptor;
}

Descriptor DescriptorManager::AllocateTemporary(uint32_t descriptorCount)
{
    assert(m_TemporaryDescriptorMaxCountPerFrame);
    Descriptor descriptor;
    CHECK_BOOL(m_TemporaryDescriptorRingBuffer.Allocate(descriptorCount, descriptor.m_Index));
    descriptor.m_Index += m_PersistentDescriptorMaxCount;
    return descriptor;
}

void DescriptorManager::FreePersistent(Descriptor desc)
{
    if(!desc.IsNull())
    {
        assert(m_PersistentDescriptorMaxCount);
        m_VirtualBlock->FreeAllocation(desc.m_Index);
    }
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorManager::GetGPUHandle(Descriptor desc, uint32_t descIndex)
{
    assert(!desc.IsNull());
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_GPUHandleForHeapStart,
        (int32_t)(desc.m_Index + descIndex), // offsetInDescriptors
        m_DescriptorSize); // descriptorIncrementSize
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorManager::GetCPUHandle(Descriptor desc, uint32_t descIndex)
{
    assert(!desc.IsNull());
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_CPUHandleForHeapStart,
        (int32_t)(desc.m_Index + descIndex), // offsetInDescriptors
        m_DescriptorSize); // descriptorIncrementSize
}
