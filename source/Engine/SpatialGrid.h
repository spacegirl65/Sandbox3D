// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "CellCoord.h"
#include "SpatialCell.h"
#include "Camera.h"
#include "Maths/Maths.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace Sandbox3D::Engine
{
    using Maths::Vec3D;
    using Maths::BoundingBoxD;
    using Maths::BoundingFrustumD;

    // Hierarchical sparse 3D cubic cell grid managing multi-scale spatial indexing and visibility culling
    class SpatialGrid final
    {
    public:
        explicit SpatialGrid(double baseCellSize = 500.0);
        ~SpatialGrid() = default;

        SpatialGrid(const SpatialGrid&) = delete;
        SpatialGrid& operator=(const SpatialGrid&) = delete;
        SpatialGrid(SpatialGrid&&) noexcept = default;
        SpatialGrid& operator=(SpatialGrid&&) noexcept = default;

        // Base cell dimension configuration
        void SetBaseCellSize(double baseCellSize) noexcept;
        [[nodiscard]] double GetBaseCellSize() const noexcept { return m_baseCellSize; }
        [[nodiscard]] size_t GetCellCount() const noexcept { return m_cells.size(); }

        // Cell lifecycle management
        std::shared_ptr<SpatialCell> GetOrCreateCell(const CellCoord& coord);
        [[nodiscard]] std::shared_ptr<SpatialCell> FindCell(const CellCoord& coord) const noexcept;
        bool RemoveCell(const CellCoord& coord);
        void Clear() noexcept;

        // Discrete coordinate queries
        [[nodiscard]] CellCoord GetCellCoord(const Vec3D& worldPosition, uint32_t level = 0) const noexcept;
        [[nodiscard]] BoundingBoxD GetCellBounds(const CellCoord& coord) const noexcept;
        [[nodiscard]] Vec3D GetCellCenter(const CellCoord& coord) const noexcept;

        // Spatial queries
        void QueryFrustum(
            const BoundingFrustumD& frustum,
            std::vector<SpatialCell*>& outVisibleCells
        ) const;

        void QuerySphere(
            const Vec3D& center,
            double radius,
            std::vector<SpatialCell*>& outCells,
            uint32_t level = 0
        ) const;

        void QueryBox(
            const BoundingBoxD& box,
            std::vector<SpatialCell*>& outCells,
            uint32_t level = 0
        ) const;

        // Updates visibility and distance metrics for all active cells relative to camera
        void UpdateVisibility(
            const Camera& camera,
            std::vector<SpatialCell*>& outVisibleCells
        );

        // Access all allocated cells
        [[nodiscard]] const std::unordered_map<CellCoord, std::shared_ptr<SpatialCell>>& GetCells() const noexcept
        {
            return m_cells;
        }

    private:
        double m_baseCellSize{ 500.0 };
        std::unordered_map<CellCoord, std::shared_ptr<SpatialCell>> m_cells;
    };
}

