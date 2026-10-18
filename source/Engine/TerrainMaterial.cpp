// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "TerrainMaterial.h"

#include <iostream>
#include <vector>

namespace Sandbox3D::Engine
{
    TerrainMaterial::TerrainMaterial()
    {
        SetupDefaultLayers();
    }

    TerrainMaterial::TerrainMaterial(std::filesystem::path texturesDirectory)
        : m_texturesDir(std::move(texturesDirectory))
    {
        SetupDefaultLayers();
    }

    void TerrainMaterial::SetupDefaultLayers()
    {
        m_primaryLayers.clear();
        m_altLayers.clear();

        // Biome 0: Low lying alluvial pasture and meadow
        m_primaryLayers.push_back(TerrainLayer{
            .name             = "Meadow",
            .assetId          = "1",
            .albedoHeightFile = "uncut_grass_1_albedo_height.dds",
            .normalFile       = "uncut_grass_1_normal.dds",
            .ormFile          = "uncut_grass_1_orm.dds",
            .uvScale          = 0.50f
        });

        // Biome 1: Mid-slope fell turf
        m_primaryLayers.push_back(TerrainLayer{
            .name             = "MidSlope",
            .assetId          = "1",
            .albedoHeightFile = "wild_grass_1_albedo_height.dds",
            .normalFile       = "wild_grass_1_normal.dds",
            .ormFile          = "wild_grass_1_orm.dds",
            .uvScale          = 0.50f
        });

        // Biome 2: High plateau moorland
        m_primaryLayers.push_back(TerrainLayer{
            .name             = "Plateau",
            .assetId          = "2",
            .albedoHeightFile = "wild_grass_2_albedo_height.dds",
            .normalFile       = "wild_grass_2_normal.dds",
            .ormFile          = "wild_grass_2_orm.dds",
            .uvScale          = 0.50f
        });

        // Biome 3: Dry thatch knolls and weathered ridges
        m_primaryLayers.push_back(TerrainLayer{
            .name             = "DryThatch",
            .assetId          = "1",
            .albedoHeightFile = "grass_dried_1_albedo_height.dds",
            .normalFile       = "grass_dried_1_normal.dds",
            .ormFile          = "grass_dried_1_orm.dds",
            .uvScale          = 0.50f
        });

        // Biome 4: Crevice, scar, and sheer cliff rock
        m_primaryLayers.push_back(TerrainLayer{
            .name             = "RockCliff",
            .assetId          = "1",
            .albedoHeightFile = "rock_cliff_1_albedo_height.dds",
            .normalFile       = "rock_cliff_1_normal.dds",
            .ormFile          = "rock_cliff_1_orm.dds",
            .uvScale          = 0.50f
        });

        // Alternative Biome 0: Clover and grazed pasture
        m_altLayers.push_back(TerrainLayer{
            .name             = "AltMeadow",
            .assetId          = "2",
            .albedoHeightFile = "uncut_grass_2_albedo_height.dds",
            .normalFile       = "uncut_grass_2_normal.dds",
            .ormFile          = "uncut_grass_2_orm.dds",
            .uvScale          = 0.50f
        });

        // Alternative Biome 1: Fine moor-edge bent-grass
        m_altLayers.push_back(TerrainLayer{
            .name             = "AltMidSlope",
            .assetId          = "3",
            .albedoHeightFile = "wild_grass_3_albedo_height.dds",
            .normalFile       = "wild_grass_3_normal.dds",
            .ormFile          = "wild_grass_3_orm.dds",
            .uvScale          = 0.50f
        });

        // Alternative Biome 2: Weathered crag alpine turf
        m_altLayers.push_back(TerrainLayer{
            .name             = "AltPlateau",
            .assetId          = "4",
            .albedoHeightFile = "wild_grass_4_albedo_height.dds",
            .normalFile       = "wild_grass_4_normal.dds",
            .ormFile          = "wild_grass_4_orm.dds",
            .uvScale          = 0.50f
        });
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
            fallbackPixel = 0x80546029; // RGB = Green, A = 0x80 (mid-height)
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

        // 1. Primary Composite Maps: 5 layers x 3 maps = 15 descriptors (slots 0..14, registers t1..t15)
        for (auto& layer : m_primaryLayers)
        {
            slots.push_back({ layer.albedoHeightFile, MapType::AlbedoHeight, &layer.albedoHeight });
            slots.push_back({ layer.normalFile,       MapType::Normal,       &layer.normal });
            slots.push_back({ layer.ormFile,          MapType::ORM,          &layer.orm });
        }

        // 2. Alternative Composite Maps: 3 layers x 3 maps = 9 descriptors (slots 15..23, registers t16..t24)
        for (auto& layer : m_altLayers)
        {
            slots.push_back({ layer.albedoHeightFile, MapType::AlbedoHeight, &layer.albedoHeight });
            slots.push_back({ layer.normalFile,       MapType::Normal,       &layer.normal });
            slots.push_back({ layer.ormFile,          MapType::ORM,          &layer.orm });
        }

        // Bind all composite slots into the descriptor heap
        for (size_t i = 0; i < slots.size() && i < RequiredDescriptorCount; ++i)
        {
            auto& slot = slots[i];
            auto texture = LoadOrCreateFallback(device, commandList, slot.filename, slot.type, outStagingBuffers);

            D3D12_CPU_DESCRIPTOR_HANDLE destHandle = heapStart;
            destHandle.ptr += i * srvDescriptorSize;
            texture->CreateShaderResourceView(device, destHandle);

            if (slot.targetPtr)
            {
                *slot.targetPtr = texture;
            }
            m_loadedTextures.push_back(texture);
        }

        m_isLoaded = (m_loadedTextures.size() == RequiredDescriptorCount);
        std::wcout << L"[TerrainMaterial] Staged " << m_loadedTextures.size()
                   << L" composite BC5/BC7 texture maps across descriptor slots 0.." << (RequiredDescriptorCount - 1) << L".\n";

        return m_isLoaded;
    }
}

