// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "TerrainMaterial.h"

#include <iostream>
#include <vector>

namespace Sandbox3D::Engine::Terrain
{
    TerrainMaterial::TerrainMaterial()
        : m_texturesDir("resources/environment/terrain/textures")
    {
    }

    TerrainMaterial::TerrainMaterial(std::filesystem::path texturesDirectory)
        : m_texturesDir(std::move(texturesDirectory))
    {
    }

    void TerrainMaterial::AddPrimaryLayer(TerrainLayer layer)
    {
        m_primaryLayers.push_back(std::move(layer));
    }

    void TerrainMaterial::AddAlternativeLayer(TerrainLayer layer)
    {
        m_altLayers.push_back(std::move(layer));
    }

    std::shared_ptr<Renderer::Texture> TerrainMaterial::LoadOrCreateFallback(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList,
        const std::string& filename,
        MapType type,
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& outStagingBuffers
    )
    {
        if (!filename.empty())
        {
            const auto fullPath = m_texturesDir / filename;
            if (std::filesystem::exists(fullPath))
            {
                try
                {
                    Microsoft::WRL::ComPtr<ID3D12Resource> staging;
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

        Microsoft::WRL::ComPtr<ID3D12Resource> staging;
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
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& outStagingBuffers
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
}

