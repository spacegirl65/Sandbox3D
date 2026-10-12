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
            .name          = "Meadow",
            .assetId       = "oilpt20",
            .albedoFile    = "uncut_grass_oilpt20_4k_albedo.dds",
            .normalFile    = "uncut_grass_oilpt20_4k_normal.dds",
            .roughnessFile = "uncut_grass_oilpt20_4k_roughness.dds",
            .aoFile        = "uncut_grass_oilpt20_4k_ao.dds",
            .dispFile      = "uncut_grass_oilpt20_4k_displacement.dds",
            .uvScale       = 0.50f
        });

        // Biome 1: Mid-slope fell turf
        m_primaryLayers.push_back(TerrainLayer{
            .name          = "MidSlope",
            .assetId       = "umjlabus",
            .albedoFile    = "wild_grass_umjlabus_4k_albedo.dds",
            .normalFile    = "wild_grass_umjlabus_4k_normal.dds",
            .roughnessFile = "wild_grass_umjlabus_4k_roughness.dds",
            .aoFile        = "wild_grass_umjlabus_4k_ao.dds",
            .dispFile      = "wild_grass_umjlabus_4k_displacement.dds",
            .uvScale       = 0.50f
        });

        // Biome 2: High plateau moorland
        m_primaryLayers.push_back(TerrainLayer{
            .name          = "Plateau",
            .assetId       = "vbslfeqfw",
            .albedoFile    = "wild_grass_vbslfeqfw_4k_albedo.dds",
            .normalFile    = "wild_grass_vbslfeqfw_4k_normal.dds",
            .roughnessFile = "wild_grass_vbslfeqfw_4k_roughness.dds",
            .aoFile        = "wild_grass_vbslfeqfw_4k_ao.dds",
            .dispFile      = "wild_grass_vbslfeqfw_4k_displacement.dds",
            .uvScale       = 0.50f
        });

        // Biome 3: Dry thatch knolls and weathered ridges
        m_primaryLayers.push_back(TerrainLayer{
            .name          = "DryThatch",
            .assetId       = "olqkj0",
            .albedoFile    = "grass_dried_olqkj0_4k_albedo.dds",
            .normalFile    = "grass_dried_olqkj0_4k_normal.dds",
            .roughnessFile = "grass_dried_olqkj0_4k_roughness.dds",
            .aoFile        = "grass_dried_olqkj0_4k_ao.dds",
            .dispFile      = "grass_dried_olqkj0_4k_displacement.dds",
            .uvScale       = 0.50f
        });

        // Biome 4: Crevice, scar, and sheer cliff rock
        m_primaryLayers.push_back(TerrainLayer{
            .name          = "RockCliff",
            .assetId       = "vl3ibcxlw",
            .albedoFile    = "rock_cliff_vl3ibcxlw_4k_albedo.dds",
            .normalFile    = "rock_cliff_vl3ibcxlw_4k_normal.dds",
            .roughnessFile = "rock_cliff_vl3ibcxlw_4k_roughness.dds",
            .aoFile        = "rock_cliff_vl3ibcxlw_4k_ao.dds",
            .dispFile      = "rock_cliff_vl3ibcxlw_4k_displacement.dds",
            .uvScale       = 0.50f
        });

        // Alternative Biome 0: Clover and grazed pasture
        m_altLayers.push_back(TerrainLayer{
            .name          = "AltMeadow",
            .assetId       = "pftph0",
            .albedoFile    = "uncut_grass_pftph0_4k_albedo.dds",
            .normalFile    = "uncut_grass_pftph0_4k_normal.dds",
            .roughnessFile = "uncut_grass_pftph0_4k_roughness.dds",
            .aoFile        = "uncut_grass_pftph0_4k_ao.dds",
            .dispFile      = "uncut_grass_pftph0_4k_displacement.dds",
            .uvScale       = 0.50f
        });

        // Alternative Biome 1: Fine moor-edge bent-grass
        m_altLayers.push_back(TerrainLayer{
            .name          = "AltMidSlope",
            .assetId       = "umrmdgps",
            .albedoFile    = "wild_grass_umrmdgps_4k_albedo.dds",
            .normalFile    = "wild_grass_umrmdgps_4k_normal.dds",
            .roughnessFile = "wild_grass_umrmdgps_4k_roughness.dds",
            .aoFile        = "wild_grass_umrmdgps_4k_ao.dds",
            .dispFile      = "wild_grass_umrmdgps_4k_displacement.dds",
            .uvScale       = 0.50f
        });

        // Alternative Biome 2: Weathered crag alpine turf
        m_altLayers.push_back(TerrainLayer{
            .name          = "AltPlateau",
            .assetId       = "vbikagyn",
            .albedoFile    = "wild_grass_vbikagyn_4k_albedo.dds",
            .normalFile    = "wild_grass_vbikagyn_4k_normal.dds",
            .roughnessFile = "wild_grass_vbikagyn_4k_roughness.dds",
            .aoFile        = "wild_grass_vbikagyn_4k_ao.dds",
            .dispFile      = "wild_grass_vbikagyn_4k_displacement.dds",
            .uvScale       = 0.50f
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
        uint32_t fallbackPixel = 0xFFFFFFFF; // White by default
        switch (type)
        {
        case MapType::Albedo:
            fallbackPixel = 0xFF546029; // Rich vegetation green
            break;
        case MapType::Normal:
            fallbackPixel = 0xFFFF8080; // (0.5, 0.5, 1.0, 1.0) flat surface normal in tangent space
            break;
        case MapType::Roughness:
            fallbackPixel = 0xFF808080; // Roughness = 0.5
            break;
        case MapType::AO:
            fallbackPixel = 0xFFFFFFFF; // AO = 1.0 (unoccluded)
            break;
        case MapType::Displacement:
            fallbackPixel = 0xFF808080; // Height = 0.5
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

        // 1. Primary PBR maps: 5 layers x 4 maps (slots 0..19, registers t1..t20)
        for (auto& layer : m_primaryLayers)
        {
            slots.push_back({ layer.albedoFile,    MapType::Albedo,    &layer.albedo });
            slots.push_back({ layer.normalFile,    MapType::Normal,    &layer.normal });
            slots.push_back({ layer.roughnessFile, MapType::Roughness, &layer.roughness });
            slots.push_back({ layer.aoFile,        MapType::AO,        &layer.ao });
        }

        // 2. Primary POM Displacement maps: 5 maps (slots 20..24, registers t21..t25)
        for (auto& layer : m_primaryLayers)
        {
            slots.push_back({ layer.dispFile, MapType::Displacement, &layer.displacement });
        }

        // 3. Alternative PBR maps: 3 layers x 4 maps (slots 25..36, registers t26..t37)
        for (auto& layer : m_altLayers)
        {
            slots.push_back({ layer.albedoFile,    MapType::Albedo,    &layer.albedo });
            slots.push_back({ layer.normalFile,    MapType::Normal,    &layer.normal });
            slots.push_back({ layer.roughnessFile, MapType::Roughness, &layer.roughness });
            slots.push_back({ layer.aoFile,        MapType::AO,        &layer.ao });
        }

        // 4. Alternative POM Displacement maps: 3 maps (slots 37..39, registers t38..t40)
        for (auto& layer : m_altLayers)
        {
            slots.push_back({ layer.dispFile, MapType::Displacement, &layer.displacement });
        }

        // Bind all required slots into the descriptor heap
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
        std::wcout << L"[TerrainMaterial] Successfully staged " << m_loadedTextures.size()
                   << L" biome texture maps across descriptor slots 0.." << (RequiredDescriptorCount - 1) << L".\n";

        return m_isLoaded;
    }
}

