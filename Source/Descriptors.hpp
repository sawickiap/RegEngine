#pragma once

#include "MultiFrameRingBuffer.hpp"

/*
Represents a single or a sequence of several shader-visible descriptors,
allocated from ShaderResourceDescriptorManager.
A lightweight object to be passed by value.
*/
struct Descriptor
{
    uint64_t m_Index = UINT64_MAX;

    bool IsNull() const { return m_Index == UINT64_MAX; }
};

/*
Represents a collection of descriptors.
There is only one all-encompassing ID3D12DescriptorHeap from which you can
allocate descriptors of two types:

- Persistent - You need to allocate and free them manually. Can do it in any
  order. There is a full allocation algorithm underneath.
- Temporary - Lifetime limited to recording and execution of the current frame.
  Freed automatically.

Space in m_DescriptorHeap is divided into two sections:

1. (persistentDescriptorMaxCount) for persistent descriptors, managed by
   D3D12MA::VirtualAllocator.
2. (temporaryDescriptorMaxCountPerFrame * g_FrameCount) for temporary
   descriptors, managed by MultiFrameRingBuffer.
*/
class DescriptorManager
{
public:
    void Init(
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        uint32_t persistentDescriptorMaxCount,
        uint32_t temporaryDescriptorMaxCountPerFrame);
    ~DescriptorManager();
    void NewFrame();

    ID3D12DescriptorHeap* GetHeap() { return m_DescriptorHeap.Get(); }

    Descriptor AllocatePersistent(uint32_t descriptorCount);
    Descriptor AllocateTemporary(uint32_t descriptorCount);
    void FreePersistent(Descriptor desc);

    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(Descriptor desc);
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(Descriptor desc);

private:
    D3D12_DESCRIPTOR_HEAP_TYPE m_Type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    uint32_t m_PersistentDescriptorMaxCount = UINT32_MAX;
    uint32_t m_TemporaryDescriptorMaxCountPerFrame = UINT32_MAX;
    ComPtr<ID3D12DescriptorHeap> m_DescriptorHeap;
    D3D12_GPU_DESCRIPTOR_HANDLE m_GPUHandleForHeapStart = { UINT64_MAX };
    D3D12_CPU_DESCRIPTOR_HANDLE m_CPUHandleForHeapStart = { UINT64_MAX };
    // Size of one descriptor, in bytes.
    // Copy of g_Renderer->GetCapabilities().m_DescriptorSize_CVB_SRV_UAV.
    uint32_t m_DescriptorSize = UINT32_MAX;
    // Not null if m_PersistentDescriptorMaxCount > 0.
    // Unit used in this allocator is entire descriptors NOT single bytes.
    ComPtr<D3D12MA::VirtualBlock> m_VirtualBlock;
    // Initialized if m_TemporaryDescriptorMaxCountPerFrame > 0.
    MultiFrameRingBuffer<uint64_t> m_TemporaryDescriptorRingBuffer;
};
