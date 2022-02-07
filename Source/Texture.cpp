#include "BaseUtils.hpp"
#include "Texture.hpp"
#include "CommandList.hpp"
#include "Renderer.hpp"
//#include <WICTextureLoader.h>
#include <DirectXTex.h>

Texture::~Texture()
{
    g_Renderer->GetSRVDescriptorManager()->FreePersistent(m_Descriptor);
}

void Texture::LoadFromFile(uint32_t flags, const wstr_view& filePath)
{
    assert(IsEmpty());

    if(filePath.empty())
        return;

    ERR_TRY;

    LogMessageF(L"Loading texture from \"{}\"...", filePath);

    DirectX::ScratchImage scratchImage;

    if(filePath.ends_with(L".tga", false))
    {
        constexpr DirectX::TGA_FLAGS TGAFlags = DirectX::TGA_FLAGS_NONE;//DirectX::TGA_FLAGS_IGNORE_SRGB | DirectX::TGA_FLAGS_FORCE_SRGB;
        CHECK_HR(DirectX::LoadFromTGAFile(filePath.c_str(), TGAFlags, nullptr, scratchImage));
    }
    else
    {
        constexpr DirectX::WIC_FLAGS WICFlags = DirectX::WIC_FLAGS_NONE;//DirectX::WIC_FLAGS_IGNORE_SRGB | DirectX::WIC_FLAGS_FORCE_SRGB;
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

    if((flags & FLAG_SRGB) != 0)
    {
        const DXGI_FORMAT SRGBFormat = DirectX::MakeSRGB(img->format);
        CHECK_BOOL(SRGBFormat != DXGI_FORMAT_UNKNOWN);
        scratchImage.OverrideFormat(SRGBFormat);
    }

    LogInfoF(L"  Width={}, Height={}, Format={}",
        metadata.width, metadata.height, (uint32_t)metadata.format);

    if((flags & FLAG_GENERATE_MIPMAPS) != 0)
    {
        LogInfo(L"  Generating mipmaps...");

        DirectX::ScratchImage mipScratchImage;
        CHECK_HR(DirectX::GenerateMipMaps(*img, DirectX::TEX_FILTER_DEFAULT, 0, mipScratchImage));
        const DirectX::TexMetadata& mipMetadata = mipScratchImage.GetMetadata();
        m_Desc = CD3DX12_RESOURCE_DESC::Tex2D(
            mipMetadata.format,
            (uint64_t)mipMetadata.width,
            (uint32_t)mipMetadata.height,
            (uint16_t)mipMetadata.arraySize,
            (uint16_t)mipMetadata.mipLevels,
            1, // sampleCount
            0, // sampleQuality
            D3D12_RESOURCE_FLAG_NONE);
            CreateTexture(filePath);
        CHECK_BOOL(mipScratchImage.GetImageCount() == mipMetadata.mipLevels);
        for(uint32_t mip = 0; mip < (uint32_t)mipMetadata.mipLevels; ++mip)
        {
            const DirectX::Image* const mipImg = mipScratchImage.GetImage(mip, 0, 0);
            D3D12_SUBRESOURCE_DATA subresourceData = {
                .pData = mipImg->pixels,
                .RowPitch = (LONG_PTR)mipImg->rowPitch,
                .SlicePitch = (LONG_PTR)mipImg->slicePitch
            };
            const bool lastLevel = mip == mipMetadata.mipLevels - 1;
            UploadMipLevel(mip, uvec2((uint32_t)mipImg->width, (uint32_t)mipImg->height), subresourceData, lastLevel);
        }
        CreateDescriptor();
    }
    else
    {
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
        LoadFromMemory(0, resDesc, subresourceData, filePath);
    }

    ERR_CATCH_MSG(std::format(L"Cannot load texture from \"{}\".", filePath));
}

void Texture::LoadFromMemory(
    uint32_t flags,
    const D3D12_RESOURCE_DESC& resDesc,
    const D3D12_SUBRESOURCE_DATA& data,
    const wstr_view& name)
{
    assert(IsEmpty());

    ERR_TRY;
    
    m_Desc = resDesc;
    CreateTexture(name);
    UploadMipLevel(0, uvec2((uint32_t)resDesc.Width, resDesc.Height), data,
        true); // lastLevel
    CreateDescriptor();

    ERR_CATCH_FUNC;
}

void Texture::CreateTexture(const wstr_view& name)
{
    assert(m_Desc.Format != DXGI_FORMAT_UNKNOWN);
    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
    CHECK_HR(g_Renderer->GetMemoryAllocator()->CreateResource(
        &allocDesc,
        &m_Desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, // pOptimizedClearValue
        &m_Allocation,
        IID_PPV_ARGS(&m_Resource))); // riidResource, ppvResource
    if(!name.empty())
        SetD3D12ObjectName(m_Resource.Get(), name);
}

void Texture::UploadMipLevel(uint32_t mipLevel, const uvec2& size, const D3D12_SUBRESOURCE_DATA& data,
    bool lastLevel)
{
    assert(m_Desc.Format != DXGI_FORMAT_UNKNOWN);
    const uint8_t bitsPerPixel = DXGIFormatToBitsPerPixel(m_Desc.Format);
    CHECK_BOOL(bitsPerPixel > 0 && bitsPerPixel % 8 == 0);
    const uint32_t bytesPerPixel = bitsPerPixel / 8;

    // Create source buffer.
    const size_t srcBufRowPitch = std::max<size_t>(size.x * bytesPerPixel, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    const size_t srcBufSize = srcBufRowPitch * size.y;
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
        for(uint32_t y = 0; y < size.y; ++y)
        {
            memcpy(srcBufMappedPtr, textureDataPtr, size.x * bytesPerPixel);
            textureDataPtr = (char*)textureDataPtr + data.RowPitch;
            srcBufMappedPtr += srcBufRowPitch;
        }
        srcBuf->GetResource()->Unmap(0, D3D12_RANGE_ALL); // pWrittenRange
    }

    // Copy the data.
    {
        CommandList cmdList;
        g_Renderer->BeginUploadCommandList(cmdList);

        CD3DX12_TEXTURE_COPY_LOCATION dst{m_Resource.Get(), mipLevel};
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT srcFootprint = {0, // Offset
            {m_Desc.Format, size.x, size.y, 1, (UINT)srcBufRowPitch}};
        CD3DX12_TEXTURE_COPY_LOCATION src{srcBuf->GetResource(), srcFootprint};
        CD3DX12_BOX srcBox{
            0, 0, 0, // Left, Top, Front
            (LONG)size.x, (LONG)size.y, 1}; // Right, Bottom, Back

        cmdList.GetCmdList()->CopyTextureRegion(&dst,
            0, 0, 0, // DstX, DstY, DstZ
            &src, &srcBox);

        if(lastLevel)
        {
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            cmdList.GetCmdList()->ResourceBarrier(1, &barrier);
        }

        g_Renderer->CompleteUploadCommandList(cmdList);
    }
}

void Texture::CreateDescriptor()
{
    assert(m_Resource);
    assert(m_Desc.Format != DXGI_FORMAT_UNKNOWN);
    // If not true, we need a conversion from D3D12_RESOURCE_DIMENSION to D3D12_SRV_DIMENSION.
    CHECK_BOOL(m_Desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);

    DescriptorManager* SRVDescManager = g_Renderer->GetSRVDescriptorManager();
    m_Descriptor = SRVDescManager->AllocatePersistent(1);

    /*
    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {
        .Format = m_Desc.Format,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
            D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
            D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1,
            D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2,
            D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3)};
    SRVDesc.Texture2D = {
        .MostDetailedMip = 0,
        .MipLevels = UINT32_MAX,
        .PlaneSlice = 0,
        .ResourceMinLODClamp = 0.f};
    */
    g_Renderer->GetDevice()->CreateShaderResourceView(
        m_Resource.Get(), nullptr, SRVDescManager->GetCPUHandle(m_Descriptor));
}
