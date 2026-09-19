// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Base.h"
#include "TerrainConfig.h"
#include "TerrainGenerator.h"
#include "TerrainMesh.h"
#include "Renderer/RenderItem.h"

#include <d3d12.h>
#include <memory>
#include <span>
#include <vector>

namespace Sandbox3D::Engine
{
    // High-level scenery orchestrator managing procedural terrain generation and render items
    class Terrain final : public Base
    {
    public:
        explicit Terrain(const TerrainConfig& config = TerrainConfig{}, std::string_view name = "Terrain");
        ~Terrain() override = default;

        Terrain(const Terrain&) = delete;
        Terrain& operator=(const Terrain&) = delete;
        Terrain(Terrain&&) noexcept = default;
        Terrain& operator=(Terrain&&) noexcept = default;

        // Core polymorphic update loop implementation
        void Update(float deltaTime) override;

        // Renderable query interface overrides
        [[nodiscard]] bool IsRenderable() const noexcept override { return true; }
        [[nodiscard]] std::span<const Renderer::RenderItem> GetRenderItems() const noexcept override { return m_renderItems; }

        // Initialise terrain geometry on GPU device
        void Initialise(ID3D12Device* device);

        // Rebuild terrain geometry with updated configuration
        void Rebuild(ID3D12Device* device, const TerrainConfig& config);

        // Accessors
        [[nodiscard]] const TerrainConfig& GetConfig() const noexcept { return m_config; }
        [[nodiscard]] const TerrainGenerator& GetGenerator() const noexcept { return m_generator; }

        // Continuous elevation query at specified world coordinates
        [[nodiscard]] double GetHeightAt(double worldX, double worldZ) const noexcept;

    private:
        TerrainConfig                     m_config;
        TerrainGenerator                  m_generator;
        std::vector<Renderer::RenderItem> m_renderItems;
        std::shared_ptr<Renderer::Mesh>   m_mesh;
    };
}

