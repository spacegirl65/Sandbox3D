// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Texture.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace Sandbox3D::Renderer
{
    namespace
    {
        // DirectDraw Surface binary layout definitions
        #pragma pack(push, 1)
        struct DdsPixelFormat
        {
            uint32_t size;
            uint32_t flags;
            uint32_t fourCC;
            uint32_t rgbBitCount;
            uint32_t rBitMask;
            uint32_t gBitMask;
            uint32_t bBitMask;
            uint32_t aBitMask;
        };

        struct DdsHeader
        {
            uint32_t       size;
            uint32_t       flags;
            uint32_t       height;
            uint32_t       width;
            uint32_t       pitchOrLinearSize;
            uint32_t       depth;
            uint32_t       mipMapCount;
            uint32_t       reserved1[11];
            DdsPixelFormat ddspf;
            uint32_t       caps;
            uint32_t       caps2;
            uint32_t       caps3;
            uint32_t       caps4;
            uint32_t       reserved2;
        };

        struct DdsHeaderDxt10
        {
            DXGI_FORMAT dxgiFormat;
            uint32_t    resourceDimension;
            uint32_t    miscFlag;
            uint32_t    arraySize;
            uint32_t    miscFlags2;
        };
        #pragma pack(pop)

        constexpr uint32_t DdsMagic = 0x20534444; // "DDS "
        constexpr uint32_t DdpfFourCC = 0x00000004;

        constexpr uint32_t MakeFourCC(char c0, char c1, char c2, char c3) noexcept
        {
            return static_cast<uint32_t>(static_cast<uint8_t>(c0)) |
                  (static_cast<uint32_t>(static_cast<uint8_t>(c1)) << 8) |
                  (static_cast<uint32_t>(static_cast<uint8_t>(c2)) << 16) |
                  (static_cast<uint32_t>(static_cast<uint8_t>(c3)) << 24);
        }

        // Determines bytes per block or texel for row pitch calculation
        void GetSurfaceInfo(
            uint32_t width,
            uint32_t height,
            DXGI_FORMAT format,
            uint32_t& outNumBytes,
            uint32_t& outRowBytes,
            uint32_t& outNumRows
        )
        {
            bool isBlockCompressed = false;
            uint32_t bytesPerBlock = 16;

            switch (format)
            {
            case DXGI_FORMAT_BC1_TYPELESS:
            case DXGI_FORMAT_BC1_UNORM:
            case DXGI_FORMAT_BC1_UNORM_SRGB:
            case DXGI_FORMAT_BC4_TYPELESS:
            case DXGI_FORMAT_BC4_UNORM:
            case DXGI_FORMAT_BC4_SNORM:
                isBlockCompressed = true;
                bytesPerBlock = 8;
                break;

            case DXGI_FORMAT_BC2_TYPELESS:
            case DXGI_FORMAT_BC2_UNORM:
            case DXGI_FORMAT_BC2_UNORM_SRGB:
            case DXGI_FORMAT_BC3_TYPELESS:
            case DXGI_FORMAT_BC3_UNORM:
            case DXGI_FORMAT_BC3_UNORM_SRGB:
            case DXGI_FORMAT_BC5_TYPELESS:
            case DXGI_FORMAT_BC5_UNORM:
            case DXGI_FORMAT_BC5_SNORM:
            case DXGI_FORMAT_BC6H_TYPELESS:
            case DXGI_FORMAT_BC6H_UF16:
            case DXGI_FORMAT_BC6H_SF16:
            case DXGI_FORMAT_BC7_TYPELESS:
            case DXGI_FORMAT_BC7_UNORM:
            case DXGI_FORMAT_BC7_UNORM_SRGB:
                isBlockCompressed = true;
                bytesPerBlock = 16;
                break;

            default:
                isBlockCompressed = false;
                break;
            }

            if (isBlockCompressed)
            {
                const uint32_t numBlocksWide = std::max(1u, (width + 3u) / 4u);
                const uint32_t numBlocksHigh = std::max(1u, (height + 3u) / 4u);
                outRowBytes = numBlocksWide * bytesPerBlock;
                outNumRows  = numBlocksHigh;
                outNumBytes = outRowBytes * numBlocksHigh;
            }
            else
            {
                // Uncompressed 32-bit RGBA fallback
                outRowBytes = width * 4u;
                outNumRows  = height;
                outNumBytes = outRowBytes * height;
            }
        }
    }

    std::shared_ptr<Texture> Texture::LoadFromDds(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList,
        const std::filesystem::path& filePath,
        ComPtr<ID3D12Resource>& outStagingBuffer,
        std::string_view debugName
    )
    {
        if (!device || !commandList)
        {
            throw std::invalid_argument("Device and command list pointers must be valid when loading a texture.");
        }

        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            throw std::runtime_error(std::format("Failed to open DDS texture file at path: {}", filePath.string()));
        }

        const std::streamsize fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        if (fileSize < static_cast<std::streamsize>(sizeof(uint32_t) + sizeof(DdsHeader)))
        {
            throw std::runtime_error(std::format("DDS file is truncated or corrupted: {}", filePath.string()));
        }

        std::vector<uint8_t> fileData(static_cast<size_t>(fileSize));
        if (!file.read(reinterpret_cast<char*>(fileData.data()), fileSize))
        {
            throw std::runtime_error(std::format("Failed to read DDS file bytes: {}", filePath.string()));
        }

        const uint8_t* bytePtr = fileData.data();
        const uint32_t magic = *reinterpret_cast<const uint32_t*>(bytePtr);
        bytePtr += sizeof(uint32_t);

        if (magic != DdsMagic)
        {
            throw std::runtime_error(std::format("Invalid DDS magic token in file: {}", filePath.string()));
        }

        const auto* header = reinterpret_cast<const DdsHeader*>(bytePtr);
        bytePtr += sizeof(DdsHeader);

        if (header->size != sizeof(DdsHeader) || header->ddspf.size != sizeof(DdsPixelFormat))
        {
            throw std::runtime_error(std::format("Corrupted DDS header structures in file: {}", filePath.string()));
        }

        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        if ((header->ddspf.flags & DdpfFourCC) != 0)
        {
            if (header->ddspf.fourCC == MakeFourCC('D', 'X', '1', '0'))
            {
                const auto* dxt10Header = reinterpret_cast<const DdsHeaderDxt10*>(bytePtr);
                bytePtr += sizeof(DdsHeaderDxt10);
                format = dxt10Header->dxgiFormat;
            }
            else if (header->ddspf.fourCC == MakeFourCC('B', 'C', '5', 'U') ||
                     header->ddspf.fourCC == MakeFourCC('A', 'T', 'I', '2'))
            {
                format = DXGI_FORMAT_BC5_UNORM;
            }
            else if (header->ddspf.fourCC == MakeFourCC('B', 'C', '4', 'U') ||
                     header->ddspf.fourCC == MakeFourCC('A', 'T', 'I', '1'))
            {
                format = DXGI_FORMAT_BC4_UNORM;
            }
            else if (header->ddspf.fourCC == MakeFourCC('D', 'X', 'T', '1'))
            {
                format = DXGI_FORMAT_BC1_UNORM;
            }
            else if (header->ddspf.fourCC == MakeFourCC('D', 'X', 'T', '5'))
            {
                format = DXGI_FORMAT_BC3_UNORM;
            }
        }

        if (format == DXGI_FORMAT_UNKNOWN)
        {
            throw std::runtime_error(std::format("Unsupported or unrecognised pixel format in DDS file: {}", filePath.string()));
        }

        auto texture = std::make_shared<Texture>();
        texture->m_width     = header->width;
        texture->m_height    = header->height;
        texture->m_mipLevels = std::max(1u, header->mipMapCount);
        texture->m_format    = format;
        texture->m_debugName = debugName.empty() ? filePath.stem().string() : std::string(debugName);

        // Prepare DirectX 12 default resource descriptor
        D3D12_RESOURCE_DESC texDesc{};
        texDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Alignment          = 0;
        texDesc.Width              = texture->m_width;
        texDesc.Height             = texture->m_height;
        texDesc.DepthOrArraySize   = 1;
        texDesc.MipLevels          = texture->m_mipLevels;
        texDesc.Format             = texture->m_format;
        texDesc.SampleDesc.Count   = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES defaultHeapProps{};
        defaultHeapProps.Type                 = D3D12_HEAP_TYPE_DEFAULT;
        defaultHeapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        defaultHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        defaultHeapProps.CreationNodeMask     = 1;
        defaultHeapProps.VisibleNodeMask      = 1;

        HR_CHECK(device->CreateCommittedResource(
            &defaultHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&texture->m_resource)
        ));

        if (!texture->m_debugName.empty())
        {
            const std::wstring wideDebugName(texture->m_debugName.begin(), texture->m_debugName.end());
            texture->m_resource->SetName(wideDebugName.c_str());
        }

        // Compute subresource footprints for staging upload buffer
        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(texture->m_mipLevels);
        std::vector<UINT>                               numRows(texture->m_mipLevels);
        std::vector<UINT64>                             rowSizesInBytes(texture->m_mipLevels);
        UINT64 totalUploadBufferSize = 0;

        device->GetCopyableFootprints(
            &texDesc,
            0,
            texture->m_mipLevels,
            0,
            layouts.data(),
            numRows.data(),
            rowSizesInBytes.data(),
            &totalUploadBufferSize
        );

        D3D12_HEAP_PROPERTIES uploadHeapProps{};
        uploadHeapProps.Type                 = D3D12_HEAP_TYPE_UPLOAD;
        uploadHeapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        uploadHeapProps.CreationNodeMask     = 1;
        uploadHeapProps.VisibleNodeMask      = 1;

        D3D12_RESOURCE_DESC bufferDesc{};
        bufferDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Alignment          = 0;
        bufferDesc.Width              = totalUploadBufferSize;
        bufferDesc.Height             = 1;
        bufferDesc.DepthOrArraySize   = 1;
        bufferDesc.MipLevels          = 1;
        bufferDesc.Format             = DXGI_FORMAT_UNKNOWN;
        bufferDesc.SampleDesc.Count   = 1;
        bufferDesc.SampleDesc.Quality = 0;
        bufferDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

        HR_CHECK(device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&outStagingBuffer)
        ));

        uint8_t* mappedData = nullptr;
        const D3D12_RANGE readRange{ 0, 0 };
        HR_CHECK(outStagingBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedData)));

        uint32_t currentWidth  = texture->m_width;
        uint32_t currentHeight = texture->m_height;

        for (uint32_t mip = 0; mip < texture->m_mipLevels; ++mip)
        {
            uint32_t numBytes = 0;
            uint32_t rowBytes = 0;
            uint32_t rows     = 0;
            GetSurfaceInfo(currentWidth, currentHeight, texture->m_format, numBytes, rowBytes, rows);

            if (bytePtr + numBytes > fileData.data() + fileData.size())
            {
                outStagingBuffer->Unmap(0, nullptr);
                throw std::runtime_error(std::format("Subresource data out of bounds in DDS file: {}", filePath.string()));
            }

            uint8_t* destSubresource = mappedData + layouts[mip].Offset;
            const uint32_t destRowPitch = layouts[mip].Footprint.RowPitch;

            for (uint32_t r = 0; r < rows; ++r)
            {
                std::memcpy(destSubresource + (r * destRowPitch), bytePtr + (r * rowBytes), rowBytes);
            }

            bytePtr += numBytes;

            // Schedule copy command from staging buffer into GPU default heap
            D3D12_TEXTURE_COPY_LOCATION dstLoc{};
            dstLoc.pResource        = texture->m_resource.Get();
            dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLoc.SubresourceIndex = mip;

            D3D12_TEXTURE_COPY_LOCATION srcLoc{};
            srcLoc.pResource       = outStagingBuffer.Get();
            srcLoc.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            srcLoc.PlacedFootprint = layouts[mip];

            commandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

            currentWidth  = std::max(1u, currentWidth / 2u);
            currentHeight = std::max(1u, currentHeight / 2u);
        }

        outStagingBuffer->Unmap(0, nullptr);

        // Transition resource state to pixel shader resource
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource   = texture->m_resource.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        commandList->ResourceBarrier(1, &barrier);

        return texture;
    }

    void Texture::CreateShaderResourceView(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const
    {
        if (!device || !m_resource)
        {
            return;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format                    = m_format;
        srvDesc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping   = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels       = m_mipLevels;
        srvDesc.Texture2D.PlaneSlice      = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

        device->CreateShaderResourceView(m_resource.Get(), &srvDesc, cpuHandle);
    }
}

