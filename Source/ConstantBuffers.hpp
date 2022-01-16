#pragma once

/*
Represents a facility for allocation and filling temporary constant buffers
valid only for recording and execution of the current frame.
*/
class TemporaryConstantBufferManager
{
public:
    void Init();
    ~TemporaryConstantBufferManager();
    void NewFrame();

    /*
    - Allocates a new piece of data inside the buffer.
      - Returns mapped pointer to it. The memory is uncached and write-combined!
    - Allocates and sets a new temporary CBV descriptor for it using
      ShaderResourceDescriptorManager::AllocateTemporaryDescriptor.
      - Returns GPU handle to that descriptor ready to be set as a root parameter.
    */
    void CreateBuffer(uint32_t size,
        void*& outMappedPtr, D3D12_GPU_DESCRIPTOR_HANDLE& outCBVDescriptorHandle);

private:
    ComPtr<D3D12MA::Allocation> m_Buffer;
    void* m_BufferMappedPtr = nullptr;
    uint32_t m_FrameIndex = 0;
    uint32_t m_AllocatedSizeInCurrentFrame = 0;
};
