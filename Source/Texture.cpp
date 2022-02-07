#include "BaseUtils.hpp"
#include "Texture.hpp"
#include "CommandList.hpp"
#include "Renderer.hpp"
#include "Streams.hpp"
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

    const std::filesystem::path sourceFilePath = StrToPath(filePath);
    const size_t hash = CalculateHash(flags, sourceFilePath);
    const std::filesystem::path cacheFilePath = StrToPath(std::format(L"Cache/Textures/{:016X}", hash));
    const bool sourceFileExists = FileExists(sourceFilePath);

    bool cacheIsValid = (flags & FLAG_CACHE_LOAD) != 0 && FileExists(cacheFilePath);
    if(cacheIsValid)
    {
        std::filesystem::file_time_type cacheWriteTime;
        cacheIsValid = GetFileLastWriteTime(cacheWriteTime, cacheFilePath);
        if(cacheIsValid)
        {
            if(sourceFileExists)
            {
                std::filesystem::file_time_type sourceWriteTime;
                cacheIsValid = GetFileLastWriteTime(sourceWriteTime, sourceFilePath) &&
                    cacheWriteTime > sourceWriteTime;
            }
            // else: Can load only from cache not from source file. Cache is always valid.
        }
    }
    if(cacheIsValid)
    {
        try
        {
            LoadFromCacheFile(flags, cacheFilePath);
        } CATCH_PRINT_ERROR(;)
    }

    if(IsEmpty())
        LoadFromSourceFile(flags, filePath, cacheFilePath);

    assert(!IsEmpty());
    SetD3D12ObjectName(m_Resource, filePath);
    CreateDescriptor();

    ERR_CATCH_MSG(std::format(L"Cannot load texture from \"{}\".", filePath));
}

void Texture::LoadFromMemory(
    const D3D12_RESOURCE_DESC& resDesc,
    const D3D12_SUBRESOURCE_DATA& data,
    const wstr_view& name)
{
    assert(IsEmpty());

    ERR_TRY;
    
    m_Desc = resDesc;
    CreateTexture();
    assert(m_Resource.Get());
    if(!name.empty())
        SetD3D12ObjectName(m_Resource.Get(), name);
    UploadMipLevel(0, uvec2((uint32_t)resDesc.Width, resDesc.Height), data,
        true); // lastLevel
    CreateDescriptor();

    ERR_CATCH_FUNC;
}

size_t Texture::CalculateHash(uint32_t flags, const std::filesystem::path& filePath)
{
    flags &= FLAG_SRGB | FLAG_GENERATE_MIPMAPS;
    size_t hash = std::hash<uint32_t>()(flags);
    
    wstring processedPath = std::filesystem::canonical(std::filesystem::absolute(filePath)).native();
    ToUpperCase(processedPath);
    hash = CombineHash(hash, std::hash<wstring>()(processedPath));

    return hash;
}

void Texture::LoadFromSourceFile(uint32_t flags, const wstr_view& filePath,
    const std::filesystem::path& cacheFilePath)
{
    DirectX::ScratchImage image;

    if(filePath.ends_with(L".tga", false))
    {
        constexpr DirectX::TGA_FLAGS TGAFlags = DirectX::TGA_FLAGS_NONE;//DirectX::TGA_FLAGS_IGNORE_SRGB | DirectX::TGA_FLAGS_FORCE_SRGB;
        CHECK_HR(DirectX::LoadFromTGAFile(filePath.c_str(), TGAFlags, nullptr, image));
    }
    else
    {
        constexpr DirectX::WIC_FLAGS WICFlags = DirectX::WIC_FLAGS_NONE;//DirectX::WIC_FLAGS_IGNORE_SRGB | DirectX::WIC_FLAGS_FORCE_SRGB;
        CHECK_HR(DirectX::LoadFromWICFile(filePath.c_str(), WICFlags, nullptr, image));
    }

    Load(flags, image);

    if((flags & FLAG_CACHE_SAVE) != 0)
    {
        ERR_TRY;
        SaveCacheFile(cacheFilePath, image);
       } CATCH_PRINT_ERROR(;)
    }
}

static const char* const CACHE_FILE_HEADER = "RegEngine Cache Texture 100";

void Texture::SaveCacheFile(const std::filesystem::path& cacheFilePath, const DirectX::ScratchImage& image) const
{
    const wstring pathStr = cacheFilePath.native();
    LogInfoF(L"Saving texture cache to file \"{}\"...", pathStr);

    ERR_TRY;

    DirectX::Blob DDSBlob;
    CHECK_HR(DirectX::SaveToDDSMemory(
        image.GetImages(),
        image.GetImageCount(),
        image.GetMetadata(),
        DirectX::DDS_FLAGS_NONE,
        DDSBlob));

    {
        FileWriteStream stream;
        stream.Init(pathStr, FILE_STREAM_FLAG_SEQUENTIAL);
        const str_view headerStr{CACHE_FILE_HEADER};
        stream.WriteString(headerStr);
        stream.WriteValue((uint32_t)DDSBlob.GetBufferSize());
        stream.WriteData(DDSBlob.GetBufferPointer(), DDSBlob.GetBufferSize());
        stream.WriteString(headerStr);
    }
       
    ERR_CATCH_MSG(std::format(L"Cannot save texture cache file \"{}\".", pathStr));
}

void Texture::LoadFromCacheFile(uint32_t flags, const std::filesystem::path& path)
{
    const wstring pathStr = path.native();
    LogInfoF(L"Loading texture cache from file \"{}\"...", pathStr);
    ERR_TRY;

    FileReadStream stream;
    stream.Init(pathStr, 0);
    
    const size_t headerLen = strlen(CACHE_FILE_HEADER);
    char header[32];
    stream.ReadData(header, headerLen);
    if(memcmp(header, CACHE_FILE_HEADER, headerLen) != 0)
        FAIL(L"Invalid begin header.");

    uint32_t contentLen;
    stream.ReadValue(contentLen);
    CHECK_BOOL(contentLen > 0);

    // Validate header at the end.
    {
        const size_t fileSize = stream.GetFileSize();
        size_t pos = stream.GetFilePointer();
        CHECK_BOOL(pos + contentLen + headerLen == fileSize);
        stream.SetFilePointer((ptrdiff_t)contentLen, FileStreamBase::MoveMethod::Current);
        stream.ReadData(header, headerLen);
        if(memcmp(header, CACHE_FILE_HEADER, headerLen) != 0)
            FAIL(L"Invalid end header.");
        stream.SetFilePointer((ptrdiff_t)pos, FileStreamBase::MoveMethod::Begin);
    }

    std::vector<char> content(contentLen);
    stream.ReadData(content.data(), contentLen);

    DirectX::ScratchImage image;
    CHECK_HR(DirectX::LoadFromDDSMemory(content.data(), contentLen, DirectX::DDS_FLAGS_NONE, NULL, image));

    Load(flags, image);

    ERR_CATCH_MSG(std::format(L"Cannot load texture cache from file \"{}\"...", pathStr));
}

void Texture::Load(uint32_t flags, DirectX::ScratchImage& image)
{
    const DirectX::TexMetadata* metadata = &image.GetMetadata();
    CHECK_BOOL(metadata->depth == 1);
    CHECK_BOOL(metadata->arraySize == 1);
    CHECK_BOOL(metadata->miscFlags == 0); // The only option is TEX_MISC_TEXTURECUBE
    CHECK_BOOL(metadata->miscFlags2 == 0); // The only option is TEX_MISC2_ALPHA_MODE_MASK
    CHECK_BOOL(metadata->dimension == DirectX::TEX_DIMENSION_TEXTURE2D);

    CHECK_BOOL(image.GetImageCount() > 0);
    const DirectX::Image* const img0 = image.GetImage(0, 0, 0);
    CHECK_BOOL(img0->width == metadata->width);
    CHECK_BOOL(img0->height == metadata->height);
    CHECK_BOOL(img0->format == metadata->format);

    if((flags & FLAG_SRGB) != 0 && !DirectX::IsSRGB(img0->format))
    {
        const DXGI_FORMAT SRGBFormat = DirectX::MakeSRGB(img0->format);
        CHECK_BOOL(SRGBFormat != DXGI_FORMAT_UNKNOWN);
        image.OverrideFormat(SRGBFormat);
    }

    LogInfoF(L"Width={}, Height={}, Format={}, MipLevels={}",
        metadata->width, metadata->height, (uint32_t)metadata->format, metadata->mipLevels);

    if(metadata->mipLevels == 1 && (flags & FLAG_GENERATE_MIPMAPS) != 0)
    {
        LogInfo(L"Generating mipmaps...");

        DirectX::ScratchImage mipImage;
        CHECK_HR(DirectX::GenerateMipMaps(*img0, DirectX::TEX_FILTER_DEFAULT, 0, mipImage));
        const DirectX::TexMetadata& mipMetadata = mipImage.GetMetadata();
        image = std::move(mipImage);
        metadata = &image.GetMetadata(); // Have to refresh.
    }

    m_Desc = CD3DX12_RESOURCE_DESC::Tex2D(
        metadata->format,
        (uint64_t)metadata->width,
        (uint32_t)metadata->height,
        (uint16_t)metadata->arraySize,
        (uint16_t)metadata->mipLevels,
        1, // sampleCount
        0, // sampleQuality
        D3D12_RESOURCE_FLAG_NONE);
    CreateTexture();
    CHECK_BOOL(image.GetImageCount() == metadata->mipLevels);
    for(uint32_t mip = 0; mip < (uint32_t)metadata->mipLevels; ++mip)
    {
        const DirectX::Image* const currImg = image.GetImage(mip, 0, 0);
        D3D12_SUBRESOURCE_DATA subresourceData = {
            .pData = currImg->pixels,
            .RowPitch = (LONG_PTR)currImg->rowPitch,
            .SlicePitch = (LONG_PTR)currImg->slicePitch
        };
        const bool lastLevel = mip == metadata->mipLevels - 1;
        UploadMipLevel(mip, uvec2((uint32_t)currImg->width, (uint32_t)currImg->height), subresourceData, lastLevel);
    }
}

void Texture::CreateTexture()
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
