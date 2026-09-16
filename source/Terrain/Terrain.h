// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "TerrainConfig.h"
#include "TerrainGenerator.h"
#include "TerrainMesh.h"
#include "Renderer/RenderItem.h"

#include <d3d12.h>
#include <memory>
#include <vector>

namespace Sandbox3D::Terrain
{
    // High-level scenery orchestrator managing procedural terrain generation and render items
    class Terrain final
    {
    public:
        explicit Terrain(const TerrainConfig& config = TerrainConfig{});
        ~Terrain() = default;

        Terrain(const Terrain&) = delete;
        Terrain& operator=(const Terrain&) = delete;
        Terrain(Terrain&&) noexcept = default;
        Terrain& operator=(Terrain&&) noexcept = default;

        // Initialise terrain geometry on GPU device
        void Initialise(ID3D12Device* device);

        // Rebuild terrain geometry with updated configuration
        void Rebuild(ID3D12Device* device, const TerrainConfig& config);

        // Accessors
        [[nodiscard]] const TerrainConfig& GetConfig() const noexcept { return m_config; }
        [[nodiscard]] const TerrainGenerator& GetGenerator() const noexcept { return m_generator; }
        [[nodiscard]] const std::vector<Renderer::RenderItem>& GetRenderItems() const noexcept { return m_renderItems; }
        [[nodiscard]] std::vector<Renderer::RenderItem>& GetRenderItems() noexcept { return m_renderItems; }

        // Continuous elevation query at specified world coordinates
        [[nodiscard]] double GetHeightAt(double worldX, double worldZ) const noexcept;

    private:
        TerrainConfig                     m_config;
        TerrainGenerator                  m_generator;
        std::vector<Renderer::RenderItem> m_renderItems;
        std::shared_ptr<Renderer::Mesh>   m_mesh;
    };
}

