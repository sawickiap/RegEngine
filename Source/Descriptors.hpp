#pragma once

#include "MultiFrameRingBuffer.hpp"

/*
Represents a single or a sequence of several shader-visible CBV/SRV/UAV
descriptors, allocated from ShaderResourceDescriptorManager.
A lightweight object to be passed by value.
*/
struct ShaderResourceDescriptor
{
    uint64_t m_Index = UINT64_MAX;
};

/*
Represents a collection of shader-visible CBV/SRV/UAV descriptors.
There is only one all-encompassing ID3D12DescriptorHeap from which you can
allocate descriptors of two types:

- Persistent - You need to allocate and free them manually. Can do it in any
  order. There is a full allocation algorithm underneath.
- Temporary - Lifetime limited to recording and execution of the current frame.
  Freed automatically.

Space in m_DescriptorHeap is divided into two sections:

1. (g_PersistentDescriptorMaxCount) for persistent descriptors, managed by
   D3D12MA::VirtualAllocator.
2. (g_TemporaryDescriptorMaxCountPerFrame * g_FrameCount) for temporary
   descriptors, managed by MultiFrameRingBuffer.
*/
class ShaderResourceDescriptorManager
{
public:
    void Init();
    void NewFrame();

    ID3D12DescriptorHeap* GetDescriptorHeap() { return m_DescriptorHeap.Get(); }

    ShaderResourceDescriptor AllocatePersistentDescriptor(uint32_t descriptorCount);
    ShaderResourceDescriptor AllocateTemporaryDescriptor(uint32_t descriptorCount);
    void FreePersistentDescriptor(ShaderResourceDescriptor desc);

    D3D12_GPU_DESCRIPTOR_HANDLE GetDescriptorGPUHandle(ShaderResourceDescriptor desc);
    D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorCPUHandle(ShaderResourceDescriptor desc);

private:
    ComPtr<ID3D12DescriptorHeap> m_DescriptorHeap;
    D3D12_GPU_DESCRIPTOR_HANDLE m_GPUHandleForHeapStart = { UINT64_MAX };
    D3D12_CPU_DESCRIPTOR_HANDLE m_CPUHandleForHeapStart = { UINT64_MAX };
    // Size of one descriptor, in bytes.
    // Copy of g_Renderer->GetCapabilities().m_DescriptorSize_CVB_SRV_UAV.
    uint32_t m_DescriptorSize = UINT32_MAX;
    // Unit used in this allocator is entire descriptors NOT single bytes.
    ComPtr<D3D12MA::VirtualBlock> m_VirtualBlock;
    // These offsets and sizes are also in descriptors.
    uint32_t m_TemporaryDescriptorOffset = UINT32_MAX;
    MultiFrameRingBuffer<uint64_t> m_TemporaryDescriptorRingBuffer;
};
