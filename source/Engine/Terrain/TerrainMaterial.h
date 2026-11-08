// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "TerrainLayer.h"
#include "Renderer/Material.h"
#include "Renderer/Texture.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace Sandbox3D::Renderer
{
    class CascadedShadowMap;
}

namespace Sandbox3D::Engine::Terrain
{
    // Manages landscape surface biome layers, composite BC5/BC7 texture streaming, and GPU descriptor table bindings
    class TerrainMaterial : public Renderer::Material
    {
    public:
        TerrainMaterial();
        explicit TerrainMaterial(std::filesystem::path texturesDirectory);
        ~TerrainMaterial() override = default;

        TerrainMaterial(const TerrainMaterial&) = delete;
        TerrainMaterial& operator=(const TerrainMaterial&) = delete;
        TerrainMaterial(TerrainMaterial&&) noexcept = default;
        TerrainMaterial& operator=(TerrainMaterial&&) noexcept = default;

        // Adds or overrides a primary biome layer
        void AddPrimaryLayer(TerrainLayer layer);

        // Adds or overrides an alternative variety biome layer
        void AddAlternativeLayer(TerrainLayer layer);

        // Allocates the material's dedicated 48-slot SRV descriptor heap and populates fallbacks
        bool InitialiseDescriptorHeap(ID3D12Device* device);

        // Loads all configured layer textures into the material's dedicated descriptor heap
        bool Load(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* commandList,
            D3D12_CPU_DESCRIPTOR_HANDLE heapStart,
            uint32_t srvDescriptorSize,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& outStagingBuffers
        );

        // Full load convenience using direct command queue and internal heap
        bool Load(
            ID3D12Device* device,
            ID3D12CommandQueue* commandQueue
        );

        // LiDAR horizon ambient occlusion, crevice depth, and directional horizon angle maps
        void SetLidarOcclusionMaps(
            ID3D12Device* device,
            ID3D12CommandQueue* commandQueue,
            const void* aoPixelData,
            const void* horizon0PixelData,
            const void* horizon1PixelData,
            uint32_t width,
            uint32_t height
        );

        // GPU heightfield, continuous macro normal, and landscape palette textures
        void SetTerrainGpuMaps(
            ID3D12Device* device,
            ID3D12CommandQueue* commandQueue,
            const float* heightfieldData,
            const int16_t* macroNormalData,
            const uint16_t* paletteColorData,
            uint32_t width,
            uint32_t height
        );

        // Binds the cascaded shadow map SRV into descriptor slot 27 (register t28)
        void SetShadowMap(ID3D12Device* device, const Renderer::CascadedShadowMap& shadowMap);

        // Descriptor table interface overrides from Renderer::Material
        [[nodiscard]] bool HasDescriptorHeap() const noexcept override { return m_srvHeap != nullptr; }
        [[nodiscard]] ID3D12DescriptorHeap* GetDescriptorHeap() const noexcept override { return m_srvHeap.Get(); }
        [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetGpuDescriptorTable() const noexcept override
        {
            return m_srvHeap ? m_srvHeap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };
        }

        // Texture accessors
        [[nodiscard]] std::shared_ptr<Renderer::Texture> GetLidarOcclusionTexture() const noexcept { return m_lidarOcclusionTexture; }
        [[nodiscard]] std::shared_ptr<Renderer::Texture> GetLidarHorizonTexture0() const noexcept { return m_lidarHorizonTexture0; }
        [[nodiscard]] std::shared_ptr<Renderer::Texture> GetLidarHorizonTexture1() const noexcept { return m_lidarHorizonTexture1; }
        [[nodiscard]] std::shared_ptr<Renderer::Texture> GetHeightfieldTexture() const noexcept { return m_terrainHeightfieldTexture; }
        [[nodiscard]] std::shared_ptr<Renderer::Texture> GetMacroNormalTexture() const noexcept { return m_terrainMacroNormalTexture; }
        [[nodiscard]] std::shared_ptr<Renderer::Texture> GetPaletteTexture() const noexcept { return m_terrainPaletteTexture; }

        // Layer accessors
        [[nodiscard]] std::span<const TerrainLayer> GetPrimaryLayers() const noexcept { return m_primaryLayers; }
        [[nodiscard]] std::span<const TerrainLayer> GetAlternativeLayers() const noexcept { return m_altLayers; }
        [[nodiscard]] size_t GetTotalLayerCount() const noexcept { return m_primaryLayers.size() + m_altLayers.size(); }
        [[nodiscard]] size_t GetLoadedTextureCount() const noexcept { return m_loadedTextures.size(); }
        [[nodiscard]] bool IsLoaded() const noexcept { return m_isLoaded; }

        [[nodiscard]] const std::filesystem::path& GetTexturesDirectory() const noexcept { return m_texturesDir; }
        void SetTexturesDirectory(std::filesystem::path dir) noexcept { m_texturesDir = std::move(dir); }

        static constexpr uint32_t RequiredDescriptorCount = 24;
        static constexpr uint32_t TotalDescriptorCount    = 48;

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

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>    m_srvHeap;
        uint32_t                                        m_srvDescriptorSize{ 0 };

        std::shared_ptr<Renderer::Texture>              m_lidarOcclusionTexture;
        std::shared_ptr<Renderer::Texture>              m_lidarHorizonTexture0;
        std::shared_ptr<Renderer::Texture>              m_lidarHorizonTexture1;
        std::shared_ptr<Renderer::Texture>              m_terrainHeightfieldTexture;
        std::shared_ptr<Renderer::Texture>              m_terrainMacroNormalTexture;
        std::shared_ptr<Renderer::Texture>              m_terrainPaletteTexture;
    };
}
