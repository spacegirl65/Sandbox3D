// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "TerrainMaterial.h"
#include "Renderer/CascadedShadowMap.h"
#include "Renderer/DxCheck.h"
#include "Renderer/Renderer.h"

#include <iostream>
#include <vector>

namespace Sandbox3D::Engine::Terrain
{
    using Microsoft::WRL::ComPtr;

    TerrainMaterial::TerrainMaterial()
        : Renderer::Material("TerrainMaterial")
        , m_texturesDir("resources/environment/terrain/textures")
    {
        SetShaderName("Terrain");
        SetLightChannels(LightChannel::Terrain);
        SetAlbedo(0.35f, 0.55f, 0.25f, 1.0f);
        SetRoughness(0.85f);
        SetMetallic(0.0f);
        SetSpecular(0.2f);
    }

    TerrainMaterial::TerrainMaterial(std::filesystem::path texturesDirectory)
        : TerrainMaterial()
    {
        m_texturesDir = std::move(texturesDirectory);
    }

    void TerrainMaterial::AddPrimaryLayer(TerrainLayer layer)
    {
        m_primaryLayers.push_back(std::move(layer));
    }

    void TerrainMaterial::AddAlternativeLayer(TerrainLayer layer)
    {
        m_altLayers.push_back(std::move(layer));
    }

    bool TerrainMaterial::InitialiseDescriptorHeap(ID3D12Device* device)
    {
        if (!device)
        {
            return false;
        }

        if (m_srvHeap)
        {
            return true;
        }

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.NumDescriptors = TotalDescriptorCount;
        heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        heapDesc.NodeMask       = 0;

        HR_CHECK(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvHeap)));
        m_srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        return true;
    }

    std::shared_ptr<Renderer::Texture> TerrainMaterial::LoadOrCreateFallback(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList,
        const std::string& filename,
        MapType type,
        std::vector<ComPtr<ID3D12Resource>>& outStagingBuffers
    )
    {
        if (!filename.empty())
        {
            const auto fullPath = m_texturesDir / filename;
            if (std::filesystem::exists(fullPath))
            {
                try
                {
                    ComPtr<ID3D12Resource> staging;
                    auto texture = Renderer::Texture::LoadFromDds(device, commandList, fullPath, staging, filename);
                    if (texture)
                    {
                        outStagingBuffers.push_back(staging);
                        std::wcout << L"[TerrainMaterial] Loaded texture: " << fullPath.filename().wstring() << L"\n";
                        return texture;
                    }
                }
                catch (const std::exception& e)
                {
                    std::wcout << L"[TerrainMaterial] Warning: Failed to load " << fullPath.filename().wstring()
                               << L" (" << e.what() << L"), using fallback.\n";
                }
            }
            else
            {
                std::wcout << L"[TerrainMaterial] Notice: Texture not found: " << fullPath.filename().wstring()
                           << L", generating fallback.\n";
            }
        }

        // Generate 1x1 fallback pixel appropriate for map type
        uint32_t fallbackPixel = 0xFFFFFFFF;
        switch (type)
        {
        case MapType::AlbedoHeight:
            fallbackPixel = 0xFFFFFFFF; // Plain white (RGB = 1.0, A = 1.0)
            break;
        case MapType::Normal:
            fallbackPixel = 0xFFFF8080; // (0.5, 0.5, 1.0, 1.0) flat surface normal in tangent space
            break;
        case MapType::ORM:
            fallbackPixel = 0xFF0080FF; // R = 255 (AO=1.0), G = 128 (Rough=0.5), B = 0 (Metallic=0.0), A = 255
            break;
        }

        ComPtr<ID3D12Resource> staging;
        auto fallbackTexture = Renderer::Texture::Create2D(
            device,
            commandList,
            &fallbackPixel,
            1,
            1,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            staging,
            "TerrainFallbackTexture"
        );
        outStagingBuffers.push_back(staging);
        return fallbackTexture;
    }

    bool TerrainMaterial::Load(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList,
        D3D12_CPU_DESCRIPTOR_HANDLE heapStart,
        uint32_t srvDescriptorSize,
        std::vector<ComPtr<ID3D12Resource>>& outStagingBuffers
    )
    {
        if (!device || !commandList)
        {
            return false;
        }

        m_loadedTextures.clear();
        m_loadedTextures.reserve(RequiredDescriptorCount);

        struct MapSlotDesc
        {
            std::string filename;
            MapType     type;
            std::shared_ptr<Renderer::Texture>* targetPtr;
        };

        std::vector<MapSlotDesc> slots;
        slots.reserve(RequiredDescriptorCount);

        // Primary composite maps: 5 layers x 3 maps = 15 descriptors (slots 0..14, registers t1..t15)
        for (auto& layer : m_primaryLayers)
        {
            slots.push_back({ layer.albedoHeightFile, MapType::AlbedoHeight, &layer.albedoHeight });
            slots.push_back({ layer.normalFile,       MapType::Normal,       &layer.normal });
            slots.push_back({ layer.ormFile,          MapType::ORM,          &layer.orm });
        }

        // Alternative composite maps: 3 layers x 3 maps = 9 descriptors (slots 15..23, registers t16..t24)
        for (auto& layer : m_altLayers)
        {
            slots.push_back({ layer.albedoHeightFile, MapType::AlbedoHeight, &layer.albedoHeight });
            slots.push_back({ layer.normalFile,       MapType::Normal,       &layer.normal });
            slots.push_back({ layer.ormFile,          MapType::ORM,          &layer.orm });
        }

        // Bind composite slots into the descriptor heap, filling any unassigned slots with fallbacks
        for (size_t i = 0; i < RequiredDescriptorCount; ++i)
        {
            std::string filename;
            MapType type = MapType::AlbedoHeight;
            std::shared_ptr<Renderer::Texture>* targetPtr = nullptr;

            if (i < slots.size())
            {
                filename  = slots[i].filename;
                type      = slots[i].type;
                targetPtr = slots[i].targetPtr;
            }
            else
            {
                switch (i % 3)
                {
                case 0: type = MapType::AlbedoHeight; break;
                case 1: type = MapType::Normal; break;
                case 2: type = MapType::ORM; break;
                }
            }

            auto texture = LoadOrCreateFallback(device, commandList, filename, type, outStagingBuffers);

            D3D12_CPU_DESCRIPTOR_HANDLE destHandle = heapStart;
            destHandle.ptr += i * srvDescriptorSize;
            texture->CreateShaderResourceView(device, destHandle);

            if (targetPtr)
            {
                *targetPtr = texture;
            }
            m_loadedTextures.push_back(texture);
        }

        m_isLoaded = (m_loadedTextures.size() == RequiredDescriptorCount);
        std::wcout << L"[TerrainMaterial] Staged " << m_loadedTextures.size()
                   << L" composite BC5/BC7 texture maps across descriptor slots 0 to " << (RequiredDescriptorCount - 1) << L".\n";

        return m_isLoaded;
    }

    bool TerrainMaterial::Load(ID3D12Device* device, ID3D12CommandQueue* commandQueue)
    {
        if (!device || !commandQueue)
        {
            return false;
        }

        if (!InitialiseDescriptorHeap(device))
        {
            return false;
        }

        ComPtr<ID3D12CommandAllocator> uploadAlloc;
        HR_CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadAlloc)));

        ComPtr<ID3D12GraphicsCommandList> uploadCmdList;
        HR_CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, uploadAlloc.Get(), nullptr, IID_PPV_ARGS(&uploadCmdList)));

        std::vector<ComPtr<ID3D12Resource>> stagingBuffers;
        const D3D12_CPU_DESCRIPTOR_HANDLE heapStart = m_srvHeap->GetCPUDescriptorHandleForHeapStart();

        const bool result = Load(device, uploadCmdList.Get(), heapStart, m_srvDescriptorSize, stagingBuffers);

        // Populate default 1x1 fallback textures for slots 24..30 if not already assigned
        if (!m_lidarOcclusionTexture)
        {
            constexpr uint32_t defaultLidarPixel = 0xFF0000FF; // AO=1.0, crevice=0.0
            ComPtr<ID3D12Resource> fallbackStaging;
            m_lidarOcclusionTexture = Renderer::Texture::Create2D(
                device, uploadCmdList.Get(), &defaultLidarPixel, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, fallbackStaging, "DefaultLidarOcclusionFallback"
            );
            stagingBuffers.push_back(fallbackStaging);
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heapStart;
            handle.ptr += 24 * m_srvDescriptorSize;
            m_lidarOcclusionTexture->CreateShaderResourceView(device, handle);
        }

        if (!m_lidarHorizonTexture0)
        {
            constexpr uint32_t defaultHorizonPixel = 0x00000000;
            ComPtr<ID3D12Resource> fallbackStaging;
            m_lidarHorizonTexture0 = Renderer::Texture::Create2D(
                device, uploadCmdList.Get(), &defaultHorizonPixel, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, fallbackStaging, "DefaultLidarHorizon0Fallback"
            );
            stagingBuffers.push_back(fallbackStaging);
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heapStart;
            handle.ptr += 25 * m_srvDescriptorSize;
            m_lidarHorizonTexture0->CreateShaderResourceView(device, handle);
        }

        if (!m_lidarHorizonTexture1)
        {
            constexpr uint32_t defaultHorizonPixel = 0x00000000;
            ComPtr<ID3D12Resource> fallbackStaging;
            m_lidarHorizonTexture1 = Renderer::Texture::Create2D(
                device, uploadCmdList.Get(), &defaultHorizonPixel, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, fallbackStaging, "DefaultLidarHorizon1Fallback"
            );
            stagingBuffers.push_back(fallbackStaging);
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heapStart;
            handle.ptr += 26 * m_srvDescriptorSize;
            m_lidarHorizonTexture1->CreateShaderResourceView(device, handle);
        }

        if (!m_terrainHeightfieldTexture)
        {
            constexpr float defaultHeightPixel = 0.0f;
            ComPtr<ID3D12Resource> fallbackStaging;
            m_terrainHeightfieldTexture = Renderer::Texture::Create2D(
                device, uploadCmdList.Get(), &defaultHeightPixel, 1, 1, DXGI_FORMAT_R32_FLOAT, fallbackStaging, "DefaultTerrainHeightfieldFallback"
            );
            stagingBuffers.push_back(fallbackStaging);
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heapStart;
            handle.ptr += 28 * m_srvDescriptorSize;
            m_terrainHeightfieldTexture->CreateShaderResourceView(device, handle);
        }

        if (!m_terrainMacroNormalTexture)
        {
            constexpr int16_t defaultNormalPixel[2] = { 0, 0 };
            ComPtr<ID3D12Resource> fallbackStaging;
            m_terrainMacroNormalTexture = Renderer::Texture::Create2D(
                device, uploadCmdList.Get(), defaultNormalPixel, 1, 1, DXGI_FORMAT_R16G16_SNORM, fallbackStaging, "DefaultTerrainMacroNormalFallback"
            );
            stagingBuffers.push_back(fallbackStaging);
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heapStart;
            handle.ptr += 29 * m_srvDescriptorSize;
            m_terrainMacroNormalTexture->CreateShaderResourceView(device, handle);
        }

        if (!m_terrainPaletteTexture)
        {
            constexpr uint16_t defaultPalettePixel[4] = { 65535u, 65535u, 65535u, 0u };
            ComPtr<ID3D12Resource> fallbackStaging;
            m_terrainPaletteTexture = Renderer::Texture::Create2D(
                device, uploadCmdList.Get(), defaultPalettePixel, 1, 1, DXGI_FORMAT_R16G16B16A16_UNORM, fallbackStaging, "DefaultTerrainPaletteFallback"
            );
            stagingBuffers.push_back(fallbackStaging);
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heapStart;
            handle.ptr += 30 * m_srvDescriptorSize;
            m_terrainPaletteTexture->CreateShaderResourceView(device, handle);
        }

        HR_CHECK(uploadCmdList->Close());
        ID3D12CommandList* lists[] = { uploadCmdList.Get() };
        commandQueue->ExecuteCommandLists(1, lists);

        ComPtr<ID3D12Fence> fence;
        HR_CHECK(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        HR_CHECK(commandQueue->Signal(fence.Get(), 1));
        HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (event)
        {
            HR_CHECK(fence->SetEventOnCompletion(1, event));
            WaitForSingleObject(event, INFINITE);
            CloseHandle(event);
        }

        return result;
    }

    void TerrainMaterial::SetLidarOcclusionMaps(
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        const void* aoPixelData,
        const void* horizon0PixelData,
        const void* horizon1PixelData,
        uint32_t width,
        uint32_t height
    )
    {
        if (!device || !commandQueue || !aoPixelData || width == 0 || height == 0)
        {
            return;
        }

        InitialiseDescriptorHeap(device);

        ComPtr<ID3D12CommandAllocator> uploadAlloc;
        HR_CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadAlloc)));

        ComPtr<ID3D12GraphicsCommandList> uploadCmdList;
        HR_CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, uploadAlloc.Get(), nullptr, IID_PPV_ARGS(&uploadCmdList)));

        std::vector<ComPtr<ID3D12Resource>> stagingBuffers;
        stagingBuffers.reserve(3);

        // Bind ambient occlusion and crevice depth map to descriptor slot 24 (register t25)
        ComPtr<ID3D12Resource> stagingAo;
        m_lidarOcclusionTexture = Renderer::Texture::Create2D(
            device, uploadCmdList.Get(), aoPixelData, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, stagingAo, "LidarHorizonOcclusion"
        );
        stagingBuffers.push_back(stagingAo);

        const D3D12_CPU_DESCRIPTOR_HANDLE heapStart = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_CPU_DESCRIPTOR_HANDLE aoHandle = heapStart;
        aoHandle.ptr += 24 * m_srvDescriptorSize;
        m_lidarOcclusionTexture->CreateShaderResourceView(device, aoHandle);

        if (horizon0PixelData)
        {
            ComPtr<ID3D12Resource> stagingH0;
            m_lidarHorizonTexture0 = Renderer::Texture::Create2D(
                device, uploadCmdList.Get(), horizon0PixelData, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, stagingH0, "LidarHorizonAngles0"
            );
            stagingBuffers.push_back(stagingH0);
            D3D12_CPU_DESCRIPTOR_HANDLE h0Handle = heapStart;
            h0Handle.ptr += 25 * m_srvDescriptorSize;
            m_lidarHorizonTexture0->CreateShaderResourceView(device, h0Handle);
        }

        if (horizon1PixelData)
        {
            ComPtr<ID3D12Resource> stagingH1;
            m_lidarHorizonTexture1 = Renderer::Texture::Create2D(
                device, uploadCmdList.Get(), horizon1PixelData, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, stagingH1, "LidarHorizonAngles1"
            );
            stagingBuffers.push_back(stagingH1);
            D3D12_CPU_DESCRIPTOR_HANDLE h1Handle = heapStart;
            h1Handle.ptr += 26 * m_srvDescriptorSize;
            m_lidarHorizonTexture1->CreateShaderResourceView(device, h1Handle);
        }

        HR_CHECK(uploadCmdList->Close());
        ID3D12CommandList* lists[] = { uploadCmdList.Get() };
        commandQueue->ExecuteCommandLists(1, lists);

        ComPtr<ID3D12Fence> fence;
        HR_CHECK(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        HR_CHECK(commandQueue->Signal(fence.Get(), 1));
        HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (event)
        {
            HR_CHECK(fence->SetEventOnCompletion(1, event));
            WaitForSingleObject(event, INFINITE);
            CloseHandle(event);
        }
    }

    void TerrainMaterial::SetTerrainGpuMaps(
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        const float* heightfieldData,
        const int16_t* macroNormalData,
        const uint16_t* paletteColorData,
        uint32_t width,
        uint32_t height
    )
    {
        if (!device || !commandQueue || width == 0 || height == 0)
        {
            return;
        }

        InitialiseDescriptorHeap(device);

        ComPtr<ID3D12CommandAllocator> uploadAlloc;
        HR_CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadAlloc)));

        ComPtr<ID3D12GraphicsCommandList> uploadCmdList;
        HR_CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, uploadAlloc.Get(), nullptr, IID_PPV_ARGS(&uploadCmdList)));

        std::vector<ComPtr<ID3D12Resource>> stagingBuffers;
        stagingBuffers.reserve(3);

        const D3D12_CPU_DESCRIPTOR_HANDLE heapStart = m_srvHeap->GetCPUDescriptorHandleForHeapStart();

        if (heightfieldData)
        {
            ComPtr<ID3D12Resource> stagingHeight;
            m_terrainHeightfieldTexture = Renderer::Texture::Create2D(
                device, uploadCmdList.Get(), heightfieldData, width, height, DXGI_FORMAT_R32_FLOAT, stagingHeight, "TerrainHeightfield"
            );
            stagingBuffers.push_back(stagingHeight);
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heapStart;
            handle.ptr += 28 * m_srvDescriptorSize;
            m_terrainHeightfieldTexture->CreateShaderResourceView(device, handle);
        }

        if (macroNormalData)
        {
            ComPtr<ID3D12Resource> stagingNormal;
            m_terrainMacroNormalTexture = Renderer::Texture::Create2D(
                device, uploadCmdList.Get(), macroNormalData, width, height, DXGI_FORMAT_R16G16_SNORM, stagingNormal, "TerrainMacroNormal"
            );
            stagingBuffers.push_back(stagingNormal);
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heapStart;
            handle.ptr += 29 * m_srvDescriptorSize;
            m_terrainMacroNormalTexture->CreateShaderResourceView(device, handle);
        }

        if (paletteColorData)
        {
            ComPtr<ID3D12Resource> stagingPalette;
            m_terrainPaletteTexture = Renderer::Texture::Create2D(
                device, uploadCmdList.Get(), paletteColorData, width, height, DXGI_FORMAT_R16G16B16A16_UNORM, stagingPalette, "TerrainPaletteColor"
            );
            stagingBuffers.push_back(stagingPalette);
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heapStart;
            handle.ptr += 30 * m_srvDescriptorSize;
            m_terrainPaletteTexture->CreateShaderResourceView(device, handle);
        }

        HR_CHECK(uploadCmdList->Close());
        ID3D12CommandList* lists[] = { uploadCmdList.Get() };
        commandQueue->ExecuteCommandLists(1, lists);

        ComPtr<ID3D12Fence> fence;
        HR_CHECK(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        HR_CHECK(commandQueue->Signal(fence.Get(), 1));
        HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (event)
        {
            HR_CHECK(fence->SetEventOnCompletion(1, event));
            WaitForSingleObject(event, INFINITE);
            CloseHandle(event);
        }
    }

    void TerrainMaterial::SetShadowMap(ID3D12Device* device, const Renderer::CascadedShadowMap& shadowMap)
    {
        if (!device || !m_srvHeap)
        {
            return;
        }

        constexpr uint32_t shadowSrvSlot = 27;
        D3D12_CPU_DESCRIPTOR_HANDLE destHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        destHandle.ptr += shadowSrvSlot * m_srvDescriptorSize;
        shadowMap.CreateShaderResourceView(device, destHandle);
    }
}
