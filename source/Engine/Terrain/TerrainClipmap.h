// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Maths/Maths.h"
#include "Maths/BoundingFrustum.h"
#include "Renderer/Mesh.h"
#include "Renderer/RenderItem.h"

#include <d3d12.h>
#include <memory>
#include <span>
#include <vector>

namespace Sandbox3D::Engine::Terrain
{
    // Represents an active quadtree terrain patch instance submitted for rendering
    struct TerrainPatchInstance
    {
        Maths::Mat4x4D worldMatrix{ Maths::Mat4x4D::Identity() };
        Maths::Vec4    colorTint{ 0.0f, 0.0f, 1.0f, -1.0f };
        Maths::Vec4    morphParams{ 1e9f, 1e9f, 0.0f, 0.0f };
    };

    // Camera-relative continuous distance-dependent level-of-detail (CDLOD) quadtree patch generator
    class TerrainClipmap final
    {
    public:
        TerrainClipmap() = default;
        ~TerrainClipmap() = default;

        TerrainClipmap(const TerrainClipmap&) = delete;
        TerrainClipmap& operator=(const TerrainClipmap&) = delete;
        TerrainClipmap(TerrainClipmap&&) noexcept = default;
        TerrainClipmap& operator=(TerrainClipmap&&) noexcept = default;

        // Allocates reusable GPU patch mesh template and initialises CDLOD quadtree terrain parameters
        bool Initialise(
            ID3D12Device* device,
            uint32_t patchQuads,
            uint32_t maxLodLevels,
            double terrainMinX,
            double terrainMaxX,
            double terrainMinZ,
            double terrainMaxZ,
            double centerElevation = 0.0,
            std::span<const float> elevations = {},
            uint32_t resX = 0,
            uint32_t resZ = 0
        );

        // Evaluates distance-dependent quadtree subdivision from camera position with frustum culling
        void Update(
            const Maths::Vec3D& cameraPosition,
            const Maths::BoundingFrustumD& frustum
        ) noexcept;

        // Overload evaluating distance-dependent subdivision without external frustum culling
        void Update(const Maths::Vec3D& cameraPosition) noexcept;

        // Generates active render items for all selected terrain patches
        void GenerateRenderItems(
            std::shared_ptr<Renderer::Material> terrainMaterial,
            std::vector<Renderer::RenderItem>& outItems
        ) const;

        // Query interface
        [[nodiscard]] bool IsInitialised() const noexcept { return m_isInitialised; }
        [[nodiscard]] uint32_t GetPatchQuads() const noexcept { return m_patchQuads; }
        [[nodiscard]] uint32_t GetMaxLodLevels() const noexcept { return m_maxLodLevels; }
        [[nodiscard]] size_t GetActivePatchCount() const noexcept { return m_activePatches.size(); }
        [[nodiscard]] std::shared_ptr<Renderer::Mesh> GetPatchMesh() const noexcept { return m_patchMesh; }

    private:
        // Generates an N x N quad patch template grid spanning [0, N] x [0, N] with alternating diagonal triangulation
        [[nodiscard]] static std::shared_ptr<Renderer::Mesh> CreatePatchMesh(
            ID3D12Device* device,
            uint32_t patchQuads
        );

        struct ElevationBounds
        {
            float minElev{ -1000.0f };
            float maxElev{  1000.0f };
        };

        // Queries hierarchical conservative elevation bounds for a quadtree node
        [[nodiscard]] ElevationBounds GetElevationBounds(
            uint32_t lodLevel,
            double minX,
            double minZ,
            double sizeX,
            double sizeZ
        ) const noexcept;

        // Samples the CPU heightfield elevation at world coordinates for exact distance calculation
        [[nodiscard]] double SampleElevation(double worldX, double worldZ) const noexcept;

        // Evaluates continuous distance subdivision recursively across planar quadtree nodes with early frustum culling
        void SelectPatchesRecursive(
            double minX,
            double minZ,
            double sizeX,
            double sizeZ,
            uint32_t lodLevel,
            const Maths::Vec3D& cameraPosition,
            const Maths::BoundingFrustumD* frustum
        ) noexcept;

    private:
        std::shared_ptr<Renderer::Mesh>           m_patchMesh{};
        std::vector<TerrainPatchInstance>         m_activePatches{};
        std::vector<double>                       m_lodRanges{};
        std::vector<std::vector<ElevationBounds>> m_lodElevationBounds{};
        uint32_t                                  m_patchQuads{ 64 };
        uint32_t                                  m_maxLodLevels{ 5 };
        ElevationBounds                           m_fallbackBounds{ -1000.0f, 1000.0f };
        uint32_t                                  m_rootTilesX{ 1 };
        uint32_t                                  m_rootTilesZ{ 1 };
        double                                    m_terrainMinX{ 0.0 };
        double                                    m_terrainMaxX{ 0.0 };
        double                                    m_terrainMinZ{ 0.0 };
        double                                    m_terrainMaxZ{ 0.0 };
        double                                    m_centerElevation{ 0.0 };
        std::vector<float>                        m_elevations{};
        uint32_t                                  m_resX{ 0 };
        uint32_t                                  m_resZ{ 0 };
        bool                                      m_isInitialised{ false };
    };
}
