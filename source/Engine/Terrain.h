// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Base.h"
#include "TerrainConfig.h"
#include "TerrainGenerator.h"
#include "Renderer/Mesh.h"
#include "Renderer/RenderItem.h"

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
        Terrain(std::shared_ptr<Renderer::Mesh> mesh, const TerrainConfig& config = TerrainConfig{}, std::string_view name = "Terrain");
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

        // Geometry mesh assignment
        void SetMesh(std::shared_ptr<Renderer::Mesh> mesh);
        [[nodiscard]] const std::shared_ptr<Renderer::Mesh>& GetMesh() const noexcept { return m_mesh; }

        // Rebuild terrain geometry with updated configuration
        void Rebuild(const TerrainConfig& config);

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

