#include "BaseUtils.hpp"
#include "Texture.hpp"
#include "CommandList.hpp"
#include "Renderer.hpp"
#include <WICTextureLoader.h>

void Texture::LoadFromFile(const wstr_view& filePath)
{
    assert(IsEmpty());

    if(filePath.empty())
        return;

    ERR_TRY;

    LogMessageF(L"Loading texture from \"%.*s\"...", STR_TO_FORMAT(filePath));

    unique_ptr<uint8_t[]> decodedData;
    D3D12_SUBRESOURCE_DATA subresource = {};
    CHECK_HR(DirectX::LoadWICTextureFromFileEx(
        g_Renderer->GetDevice(),
        filePath.c_str(),
        0, // maxSize
        D3D12_RESOURCE_FLAG_NONE,
        DirectX::WIC_LOADER_FORCE_SRGB,
        &m_Resource,
        decodedData,
        subresource));
    CHECK_BOOL(m_Resource && decodedData && subresource.pData && subresource.RowPitch);
    CHECK_BOOL(subresource.pData == decodedData.get());
    SetD3D12ObjectName(m_Resource, Format(L"Texture from file: %.*s", STR_TO_FORMAT(filePath)));

    m_Desc = m_Resource->GetDesc();
    const uint32_t bytesPerPixel = DXGIFormatToBytesPerPixel(m_Desc.Format);
    CHECK_BOOL(bytesPerPixel > 0);
    const uvec2 textureSize = uvec2((uint32_t)m_Desc.Width, (uint32_t)m_Desc.Height);

    // Create source buffer.
    const uint32_t srcBufRowPitch = std::max<uint32_t>(textureSize.x * bytesPerPixel, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    const uint32_t srcBufSize = srcBufRowPitch * textureSize.y;
    ComPtr<ID3D12Resource> srcBuf;
    {
        const CD3DX12_RESOURCE_DESC srcBufDesc = CD3DX12_RESOURCE_DESC::Buffer(srcBufSize);
        CD3DX12_HEAP_PROPERTIES heapProps{D3D12_HEAP_TYPE_UPLOAD};
        CHECK_HR(g_Renderer->GetDevice()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &srcBufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, // pOptimizedClearValue
            IID_PPV_ARGS(&srcBuf)));
        SetD3D12ObjectName(srcBuf, L"Texture source buffer");
    }

    // Map source buffer and memcpy data to it from decodedData.
    {
        const uint8_t* textureDataPtr = decodedData.get();
        CD3DX12_RANGE readEmptyRange{0, 0};
        void* srcBufMappedPtr = nullptr;
        CHECK_HR(srcBuf->Map(0, D3D12_RANGE_NONE, &srcBufMappedPtr));
        for(uint32_t y = 0; y < textureSize.y; ++y)
        {
            memcpy(srcBufMappedPtr, textureDataPtr, textureSize.x * bytesPerPixel);
            textureDataPtr += subresource.RowPitch;
            srcBufMappedPtr = (char*)srcBufMappedPtr + srcBufRowPitch;
        }
        srcBuf->Unmap(0, D3D12_RANGE_ALL);
    }
    decodedData.reset();

    // Copy the data.
    {
        CommandList cmdList;
        g_Renderer->BeginUploadCommandList(cmdList);

        CD3DX12_TEXTURE_COPY_LOCATION dst{m_Resource.Get()};
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT srcFootprint = {0, // Offset
            {m_Desc.Format, (uint32_t)m_Desc.Width, (uint32_t)m_Desc.Height, 1, srcBufRowPitch}};
        CD3DX12_TEXTURE_COPY_LOCATION src{srcBuf.Get(), srcFootprint};
        CD3DX12_BOX srcBox{
            0, 0, 0, // Left, Top, Front
            (LONG)m_Desc.Width, (LONG)m_Desc.Height, 1}; // Right, Bottom, Back

        cmdList.GetCmdList()->CopyTextureRegion(&dst,
            0, 0, 0, // DstX, DstY, DstZ
            &src, &srcBox);

        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmdList.GetCmdList()->ResourceBarrier(1, &barrier);

        g_Renderer->CompleteUploadCommandList(cmdList);
    }

    ERR_CATCH_MSG(Format(L"Cannot load texture from \"%.*s\".", STR_TO_FORMAT(filePath)));
}

void Texture::LoadFromMemory(
    const D3D12_RESOURCE_DESC& resDesc,
    const D3D12_SUBRESOURCE_DATA& data,
    const wstr_view& name)
{
    assert(IsEmpty());

    ERR_TRY;

    const uint8_t bytesPerPixel = DXGIFormatToBytesPerPixel(resDesc.Format);
    CHECK_BOOL(bytesPerPixel > 0);

    // Create source buffer.
    const size_t srcBufRowPitch = std::max<size_t>(resDesc.Width * bytesPerPixel, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    const size_t srcBufSize = srcBufRowPitch * resDesc.Height;
    ComPtr<ID3D12Resource> srcBuf;
    {
        const CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(srcBufSize);
        CD3DX12_HEAP_PROPERTIES heapProps{D3D12_HEAP_TYPE_UPLOAD};
        CHECK_HR(g_Renderer->GetDevice()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, // pOptimizedClearValue
            IID_PPV_ARGS(&srcBuf)));
        SetD3D12ObjectName(srcBuf, L"Font texture source buffer");
    }

    // Fill source buffer.
    {
        CD3DX12_RANGE readEmptyRange{0, 0};
        char* srcBufMappedPtr = nullptr;
        const char* textureDataPtr = (const char*)data.pData;
        CHECK_HR(srcBuf->Map(0, D3D12_RANGE_NONE, (void**)&srcBufMappedPtr));
        for(uint32_t y = 0; y < resDesc.Height; ++y)
        {
            memcpy(srcBufMappedPtr, textureDataPtr, resDesc.Width * bytesPerPixel);
            textureDataPtr = (char*)textureDataPtr + data.RowPitch;
            srcBufMappedPtr += srcBufRowPitch;
        }
        srcBuf->Unmap(0, D3D12_RANGE_ALL); // pWrittenRange
    }

    // Create destination texture.
    {
        CD3DX12_HEAP_PROPERTIES heapProps{D3D12_HEAP_TYPE_DEFAULT};
        CHECK_HR(g_Renderer->GetDevice()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr, // pOptimizedClearValue
            IID_PPV_ARGS(&m_Resource)));
        if(!name.empty())
            SetD3D12ObjectName(m_Resource, name);
    }

    // Copy the data.
    {
        CommandList cmdList;
        g_Renderer->BeginUploadCommandList(cmdList);

        CD3DX12_TEXTURE_COPY_LOCATION dst{m_Resource.Get()};
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT srcFootprint = {0, // Offset
            {resDesc.Format, (UINT)resDesc.Width, (UINT)resDesc.Height, 1, (UINT)srcBufRowPitch}};
        CD3DX12_TEXTURE_COPY_LOCATION src{srcBuf.Get(), srcFootprint};
        CD3DX12_BOX srcBox{
            0, 0, 0, // Left, Top, Front
            (LONG)resDesc.Width, (LONG)resDesc.Height, 1}; // Right, Bottom, Back

        cmdList.GetCmdList()->CopyTextureRegion(&dst,
            0, 0, 0, // DstX, DstY, DstZ
            &src, &srcBox);

        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmdList.GetCmdList()->ResourceBarrier(1, &barrier);

        g_Renderer->CompleteUploadCommandList(cmdList);
    }

    m_Desc = resDesc;

    ERR_CATCH_FUNC;
}
