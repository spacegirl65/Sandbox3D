// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Body.h"
#include "TerrainConfig.h"
#include "TerrainGenerator.h"

#include <memory>
#include <string_view>

namespace Sandbox3D::Engine
{
    // Represents a landscape scenery entity inheriting from Body for physics and scene integration
    class TerrainObject : public Body
    {
    public:
        explicit TerrainObject(const TerrainConfig& config = TerrainConfig{}, std::string_view name = "Terrain");
        TerrainObject(std::shared_ptr<Renderer::Mesh> mesh, const TerrainConfig& config = TerrainConfig{}, std::string_view name = "Terrain");
        ~TerrainObject() override = default;

        TerrainObject(const TerrainObject&) = delete;
        TerrainObject& operator=(const TerrainObject&) = delete;
        TerrainObject(TerrainObject&&) noexcept = default;
        TerrainObject& operator=(TerrainObject&&) noexcept = default;

        // Core polymorphic update loop implementation
        void Update(float deltaTime) override;

        // Rebuild terrain generator with updated configuration
        void Rebuild(const TerrainConfig& config);

        // Accessors
        [[nodiscard]] const TerrainConfig& GetConfig() const noexcept { return m_config; }
        [[nodiscard]] const TerrainGenerator& GetGenerator() const noexcept { return m_generator; }

        // Continuous elevation query at specified world coordinates
        [[nodiscard]] double GetHeightAt(double worldX, double worldZ) const noexcept;

    private:
        TerrainConfig    m_config;
        TerrainGenerator m_generator;
    };

    using TerrainBody = TerrainObject;
}

