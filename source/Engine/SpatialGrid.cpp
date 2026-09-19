// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "SpatialGrid.h"

namespace Sandbox3D::Engine
{
    SpatialGrid::SpatialGrid(double baseCellSize)
        : m_baseCellSize(baseCellSize > 0.0 ? baseCellSize : 500.0)
    {
    }

    void SpatialGrid::SetBaseCellSize(double baseCellSize) noexcept
    {
        if (baseCellSize > 0.0)
        {
            m_baseCellSize = baseCellSize;
        }
    }

    std::shared_ptr<SpatialCell> SpatialGrid::GetOrCreateCell(const CellCoord& coord)
    {
        auto it = m_cells.find(coord);
        if (it != m_cells.end())
        {
            return it->second;
        }

        auto cell = std::make_shared<SpatialCell>(coord, m_baseCellSize);
        m_cells.emplace(coord, cell);
        return cell;
    }

    std::shared_ptr<SpatialCell> SpatialGrid::FindCell(const CellCoord& coord) const noexcept
    {
        auto it = m_cells.find(coord);
        if (it != m_cells.end())
        {
            return it->second;
        }
        return nullptr;
    }

    bool SpatialGrid::RemoveCell(const CellCoord& coord)
    {
        return m_cells.erase(coord) > 0;
    }

    void SpatialGrid::Clear() noexcept
    {
        m_cells.clear();
    }

    CellCoord SpatialGrid::GetCellCoord(const Vec3D& worldPosition, uint32_t level) const noexcept
    {
        return CellCoord::FromWorldPosition(worldPosition, level, m_baseCellSize);
    }

    BoundingBoxD SpatialGrid::GetCellBounds(const CellCoord& coord) const noexcept
    {
        return coord.GetBounds(m_baseCellSize);
    }

    Vec3D SpatialGrid::GetCellCenter(const CellCoord& coord) const noexcept
    {
        return coord.GetCenter(m_baseCellSize);
    }

    void SpatialGrid::QueryFrustum(
        const BoundingFrustumD& frustum,
        std::vector<SpatialCell*>& outVisibleCells
    ) const
    {
        for (const auto& [coord, cell] : m_cells)
        {
            if (cell && frustum.Intersects(cell->GetBounds()))
            {
                outVisibleCells.push_back(cell.get());
            }
        }
    }

    void SpatialGrid::QuerySphere(
        const Vec3D& center,
        double radius,
        std::vector<SpatialCell*>& outCells,
        uint32_t level
    ) const
    {
        const double radiusSq = radius * radius;
        for (const auto& [coord, cell] : m_cells)
        {
            if (!cell || coord.level != level)
            {
                continue;
            }

            // Approximate distance from sphere center to cell center
            const double distSq = (cell->GetCenter() - center).LengthSquared();
            const double extentsRadius = cell->GetSize() * 0.86602540378; // halfSize * sqrt(3)
            const double maxDist = radius + extentsRadius;

            if (distSq <= maxDist * maxDist)
            {
                outCells.push_back(cell.get());
            }
        }
    }

    void SpatialGrid::QueryBox(
        const BoundingBoxD& box,
        std::vector<SpatialCell*>& outCells,
        uint32_t level
    ) const
    {
        for (const auto& [coord, cell] : m_cells)
        {
            if (cell && coord.level == level && box.Intersects(cell->GetBounds()))
            {
                outCells.push_back(cell.get());
            }
        }
    }

    void SpatialGrid::UpdateVisibility(
        const Camera& camera,
        std::vector<SpatialCell*>& outVisibleCells
    )
    {
        const BoundingFrustumD frustum = camera.GetFrustumD();
        const Vec3D cameraPosition = camera.GetPosition();

        for (auto& [coord, cell] : m_cells)
        {
            if (!cell)
            {
                continue;
            }

            const bool visible = frustum.Intersects(cell->GetBounds());
            cell->SetVisible(visible);

            const double dist = (cell->GetCenter() - cameraPosition).Length();
            cell->SetDistanceToCamera(dist);

            if (visible)
            {
                outVisibleCells.push_back(cell.get());
            }
        }
    }
}

