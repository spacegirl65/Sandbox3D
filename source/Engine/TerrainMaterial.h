// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "TerrainLayer.h"
#include "Renderer/Texture.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace Sandbox3D::Engine
{
    // Manages landscape surface biome layers, composite BC5/BC7 texture streaming, and GPU descriptor table bindings
    class TerrainMaterial final
    {
    public:
        TerrainMaterial();
        explicit TerrainMaterial(std::filesystem::path texturesDirectory);
        ~TerrainMaterial() = default;

        TerrainMaterial(const TerrainMaterial&) = delete;
        TerrainMaterial& operator=(const TerrainMaterial&) = delete;
        TerrainMaterial(TerrainMaterial&&) noexcept = default;
        TerrainMaterial& operator=(TerrainMaterial&&) noexcept = default;

        // Initialises default Garsdale / Yorkshire Dales biome layers (5 primary + 3 alternative)
        void SetupDefaultLayers();

        // Adds or overrides a primary biome layer
        void AddPrimaryLayer(TerrainLayer layer);

        // Adds or overrides an alternative variety biome layer
        void AddAlternativeLayer(TerrainLayer layer);

        // Loads all configured layer textures and writes their SRVs into the descriptor heap starting at heapStart
        bool Load(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* commandList,
            D3D12_CPU_DESCRIPTOR_HANDLE heapStart,
            uint32_t srvDescriptorSize,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& outStagingBuffers
        );

        // Layer accessors
        [[nodiscard]] std::span<const TerrainLayer> GetPrimaryLayers() const noexcept { return m_primaryLayers; }
        [[nodiscard]] std::span<const TerrainLayer> GetAlternativeLayers() const noexcept { return m_altLayers; }
        [[nodiscard]] size_t GetTotalLayerCount() const noexcept { return m_primaryLayers.size() + m_altLayers.size(); }
        [[nodiscard]] size_t GetLoadedTextureCount() const noexcept { return m_loadedTextures.size(); }
        [[nodiscard]] bool IsLoaded() const noexcept { return m_isLoaded; }

        [[nodiscard]] const std::filesystem::path& GetTexturesDirectory() const noexcept { return m_texturesDir; }
        void SetTexturesDirectory(std::filesystem::path dir) noexcept { m_texturesDir = std::move(dir); }

        // Expected descriptor slot count for composite shader bindings (8 biomes x 3 maps = 24 descriptors)
        static constexpr uint32_t RequiredDescriptorCount = 24;

    private:
        enum class MapType
        {
            AlbedoHeight,
            Normal,
            ORM
        };

        std::shared_ptr<Renderer::Texture> LoadOrCreateFallback(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* commandList,
            const std::string& filename,
            MapType type,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& outStagingBuffers
        );

        std::filesystem::path                           m_texturesDir{ "resources/environment/terrain/textures" };
        std::vector<TerrainLayer>                       m_primaryLayers;
        std::vector<TerrainLayer>                       m_altLayers;
        std::vector<std::shared_ptr<Renderer::Texture>> m_loadedTextures;
        bool                                            m_isLoaded{ false };
    };
}

