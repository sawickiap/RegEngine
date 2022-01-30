#include "BaseUtils.hpp"
#include "Texture.hpp"
#include "CommandList.hpp"
#include "Renderer.hpp"
//#include <WICTextureLoader.h>
#include <DirectXTex.h>

void Texture::LoadFromFile(const wstr_view& filePath)
{
    assert(IsEmpty());

    if(filePath.empty())
        return;

    ERR_TRY;

    LogMessageF(L"Loading texture from \"{}\"...", filePath);

    DirectX::ScratchImage scratchImage;

    if(filePath.ends_with(L".tga", false))
    {
        constexpr DirectX::TGA_FLAGS TGAFlags = DirectX::TGA_FLAGS_DEFAULT_SRGB;
        CHECK_HR(DirectX::LoadFromTGAFile(filePath.c_str(), TGAFlags, nullptr, scratchImage));
    }
    else
    {
        constexpr DirectX::WIC_FLAGS WICFlags = DirectX::WIC_FLAGS_DEFAULT_SRGB;
        CHECK_HR(DirectX::LoadFromWICFile(filePath.c_str(), WICFlags, nullptr, scratchImage));
    }
    
    const DirectX::TexMetadata& metadata = scratchImage.GetMetadata();
    CHECK_BOOL(metadata.depth == 1);
    CHECK_BOOL(metadata.arraySize == 1);
    CHECK_BOOL(metadata.mipLevels == 1);
    CHECK_BOOL(metadata.miscFlags == 0); // The only option is TEX_MISC_TEXTURECUBE
    CHECK_BOOL(metadata.miscFlags2 == 0); // The only option is TEX_MISC2_ALPHA_MODE_MASK
    CHECK_BOOL(metadata.dimension == DirectX::TEX_DIMENSION_TEXTURE2D);

    CHECK_BOOL(scratchImage.GetImageCount() == 1);
    const DirectX::Image* const img = scratchImage.GetImage(0, 0, 0);
    CHECK_BOOL(img->width == metadata.width);
    CHECK_BOOL(img->height == metadata.height);
    CHECK_BOOL(img->format == metadata.format);

    LogInfoF(L"  Width={}, Height={}, Format={}",
        metadata.width, metadata.height, (uint32_t)metadata.format);

    D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        metadata.format,
        (uint64_t)metadata.width,
        (uint32_t)metadata.height,
        (uint16_t)metadata.arraySize,
        (uint16_t)metadata.mipLevels,
        1, // sampleCount
        0, // sampleQuality
        D3D12_RESOURCE_FLAG_NONE);

    D3D12_SUBRESOURCE_DATA subresourceData = {
        .pData = img->pixels,
        .RowPitch = (LONG_PTR)img->rowPitch,
        .SlicePitch = (LONG_PTR)img->slicePitch
    };

    LoadFromMemory(resDesc, subresourceData, filePath);

    ERR_CATCH_MSG(std::format(L"Cannot load texture from \"{}\".", filePath));
}

void Texture::LoadFromMemory(
    const D3D12_RESOURCE_DESC& resDesc,
    const D3D12_SUBRESOURCE_DATA& data,
    const wstr_view& name)
{
    assert(IsEmpty());

    ERR_TRY;

    const uint8_t bitsPerPixel = DXGIFormatToBitsPerPixel(resDesc.Format);
    CHECK_BOOL(bitsPerPixel > 0 && bitsPerPixel % 8 == 0);
    const uint32_t bytesPerPixel = bitsPerPixel / 8;

    // Create source buffer.
    const size_t srcBufRowPitch = std::max<size_t>(resDesc.Width * bytesPerPixel, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    const size_t srcBufSize = srcBufRowPitch * resDesc.Height;
    ComPtr<D3D12MA::Allocation> srcBuf;
    {
        const CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(srcBufSize);
        D3D12MA::ALLOCATION_DESC srcBufAllocDesc = {};
        srcBufAllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
        CHECK_HR(g_Renderer->GetMemoryAllocator()->CreateResource(
            &srcBufAllocDesc,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, // pOptimizedClearValue
            &srcBuf,
            IID_NULL, NULL)); // riidResource, ppvResource
        SetD3D12ObjectName(srcBuf->GetResource(), L"Font texture source buffer");
    }

    // Fill source buffer.
    {
        CD3DX12_RANGE readEmptyRange{0, 0};
        char* srcBufMappedPtr = nullptr;
        const char* textureDataPtr = (const char*)data.pData;
        CHECK_HR(srcBuf->GetResource()->Map(0, D3D12_RANGE_NONE, (void**)&srcBufMappedPtr));
        for(uint32_t y = 0; y < resDesc.Height; ++y)
        {
            memcpy(srcBufMappedPtr, textureDataPtr, resDesc.Width * bytesPerPixel);
            textureDataPtr = (char*)textureDataPtr + data.RowPitch;
            srcBufMappedPtr += srcBufRowPitch;
        }
        srcBuf->GetResource()->Unmap(0, D3D12_RANGE_ALL); // pWrittenRange
    }

    // Create destination texture.
    {
        D3D12MA::ALLOCATION_DESC allocDesc = {};
        allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
        CHECK_HR(g_Renderer->GetMemoryAllocator()->CreateResource(
            &allocDesc,
            &resDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr, // pOptimizedClearValue
            &m_Allocation,
            IID_PPV_ARGS(&m_Resource))); // riidResource, ppvResource
        if(!name.empty())
            SetD3D12ObjectName(m_Resource.Get(), name);
    }

    // Copy the data.
    {
        CommandList cmdList;
        g_Renderer->BeginUploadCommandList(cmdList);

        CD3DX12_TEXTURE_COPY_LOCATION dst{m_Resource.Get()};
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT srcFootprint = {0, // Offset
            {resDesc.Format, (UINT)resDesc.Width, (UINT)resDesc.Height, 1, (UINT)srcBufRowPitch}};
        CD3DX12_TEXTURE_COPY_LOCATION src{srcBuf->GetResource(), srcFootprint};
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
