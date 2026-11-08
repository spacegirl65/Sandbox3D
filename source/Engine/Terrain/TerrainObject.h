// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "../Body.h"
#include "TerrainConfig.h"
#include "TerrainGenerator.h"
#include "TerrainClipmap.h"
#include "TerrainCollider.h"

#include <d3d12.h>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace Sandbox3D::Engine::Terrain
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

        // Sets elevation grid, dimensions, and initialises attached TerrainCollider
        void SetElevationGrid(
            uint32_t resX,
            uint32_t resZ,
            double width,
            double depth,
            std::vector<float> elevations,
            double centerElevation = 333.794,
            double scaleXZ = 1.0,
            double scaleY = 1.0,
            double minElevation = -100.0,
            double maxElevation = 1000.0
        );

        // Allocates and configures CDLOD quadtree clipmap patch generator
        bool InitialiseClipmap(
            ID3D12Device* device,
            uint32_t patchQuads = 64,
            uint32_t maxLodLevels = 5
        );

        // Updates continuous distance-dependent quadtree subdivision relative to camera
        void UpdateClipmap(
            const Maths::Vec3D& cameraPosition,
            const Maths::BoundingFrustumD& frustum
        ) noexcept;

        // Generates active terrain render items for all selected quadtree patches
        void GenerateClipmapRenderItems(std::vector<Renderer::RenderItem>& outItems) const;

        // Clipmap query interface
        [[nodiscard]] bool IsUsingClipmap() const noexcept { return m_useClipmap && m_clipmap && m_clipmap->IsInitialised(); }
        void SetUseClipmap(bool enabled) noexcept { m_useClipmap = enabled; }
        [[nodiscard]] std::shared_ptr<TerrainClipmap> GetClipmap() const noexcept { return m_clipmap; }

        // Topography and dimension accessors
        [[nodiscard]] const TerrainConfig& GetConfig() const noexcept { return m_config; }
        [[nodiscard]] const TerrainGenerator& GetGenerator() const noexcept { return m_generator; }
        [[nodiscard]] std::span<const float> GetElevations() const noexcept { return m_elevations; }
        [[nodiscard]] uint32_t GetResX() const noexcept { return m_resX; }
        [[nodiscard]] uint32_t GetResZ() const noexcept { return m_resZ; }
        [[nodiscard]] double GetWidth() const noexcept { return m_width; }
        [[nodiscard]] double GetDepth() const noexcept { return m_depth; }
        [[nodiscard]] double GetCenterElevation() const noexcept { return m_centerElevation; }
        [[nodiscard]] double GetScaleXZ() const noexcept { return m_scaleXZ; }
        [[nodiscard]] double GetScaleY() const noexcept { return m_scaleY; }

        // Continuous elevation and surface normal query at specified world coordinates
        [[nodiscard]] double GetHeightAt(double worldX, double worldZ) const noexcept;
        [[nodiscard]] Maths::Vec3D GetNormalAt(double worldX, double worldZ) const noexcept;

        // Custom terrain spatial collider access
        [[nodiscard]] std::shared_ptr<TerrainCollider> GetTerrainCollider() const noexcept;

    private:
        TerrainConfig                    m_config;
        TerrainGenerator                 m_generator;
        std::shared_ptr<TerrainClipmap>  m_clipmap{};
        bool                             m_useClipmap{ true };
        std::vector<float>               m_elevations{};
        uint32_t                         m_resX{ 0 };
        uint32_t                         m_resZ{ 0 };
        double                           m_width{ 0.0 };
        double                           m_depth{ 0.0 };
        double                           m_centerElevation{ 333.794 };
        double                           m_scaleXZ{ 1.0 };
        double                           m_scaleY{ 1.0 };
    };
}
