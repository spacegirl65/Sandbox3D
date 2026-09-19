// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Body.h"
#include "TerrainConfig.h"
#include "TerrainGenerator.h"

#include <d3d12.h>
#include <string_view>

namespace Sandbox3D::Engine
{
    // Represents a landscape scenery entity inheriting from Body for physics and scene integration
    class TerrainObject : public Body
    {
    public:
        explicit TerrainObject(const Terrain::TerrainConfig& config = Terrain::TerrainConfig{}, std::string_view name = "Terrain");
        TerrainObject(ID3D12Device* device, const Terrain::TerrainConfig& config = Terrain::TerrainConfig{}, std::string_view name = "Terrain");
        ~TerrainObject() override = default;

        TerrainObject(const TerrainObject&) = delete;
        TerrainObject& operator=(const TerrainObject&) = delete;
        TerrainObject(TerrainObject&&) noexcept = default;
        TerrainObject& operator=(TerrainObject&&) noexcept = default;

        // Core polymorphic update loop implementation
        void Update(float deltaTime) override;

        // Initialise terrain geometry on GPU device
        void Initialise(ID3D12Device* device);

        // Rebuild terrain geometry with updated configuration
        void Rebuild(ID3D12Device* device, const Terrain::TerrainConfig& config);

        // Accessors
        [[nodiscard]] const Terrain::TerrainConfig& GetConfig() const noexcept { return m_config; }
        [[nodiscard]] const Terrain::TerrainGenerator& GetGenerator() const noexcept { return m_generator; }

        // Continuous elevation query at specified world coordinates
        [[nodiscard]] double GetHeightAt(double worldX, double worldZ) const noexcept;

    private:
        Terrain::TerrainConfig    m_config;
        Terrain::TerrainGenerator m_generator;
    };

    using TerrainBody = TerrainObject;
}

namespace Sandbox3D::Terrain
{
    using TerrainObject = Engine::TerrainObject;
    using TerrainBody   = Engine::TerrainBody;
}

