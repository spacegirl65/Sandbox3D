// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "SpatialCell.h"

#include <algorithm>

namespace Sandbox3D::Engine
{
    SpatialCell::SpatialCell(const CellCoord& coord, double baseCellSize)
        : m_coord(coord)
        , m_size(coord.GetCellSize(baseCellSize))
        , m_center(coord.GetCenter(baseCellSize))
        , m_bounds(coord.GetBounds(baseCellSize))
        , m_worldMatrix(Mat4x4D::Translation(m_center))
    {
    }

    void SpatialCell::SetMesh(std::shared_ptr<Renderer::Mesh> mesh) noexcept
    {
        m_mesh = std::move(mesh);
    }

    void SpatialCell::SetMaterial(std::shared_ptr<Renderer::Material> material) noexcept
    {
        m_material = std::move(material);
    }

    void SpatialCell::AddEntity(std::shared_ptr<Base> entity)
    {
        if (entity)
        {
            m_entities.push_back(std::move(entity));
        }
    }

    void SpatialCell::RemoveEntity(uint32_t id)
    {
        std::erase_if(m_entities, [id](const std::shared_ptr<Base>& entity) {
            return entity && entity->GetId() == id;
        });
    }

    Mat4x4 SpatialCell::CalculateCameraRelativeWorld(const Vec3D& cameraPosition) const noexcept
    {
        return Mat4x4D::CreateCameraRelativeWorld(m_worldMatrix, cameraPosition);
    }

    Renderer::RenderItem SpatialCell::CreateRenderItem() const noexcept
    {
        Renderer::RenderItem item;
        item.mesh        = m_mesh;
        item.material    = m_material ? m_material : Renderer::Material::CreateTerrain();
        item.worldMatrix = m_worldMatrix;
        item.isVisible   = m_isVisible && (m_mesh != nullptr);
        item.name        = "SpatialCell_" + std::to_string(m_coord.level) + "_" +
                           std::to_string(m_coord.x) + "_" +
                           std::to_string(m_coord.y) + "_" +
                           std::to_string(m_coord.z);
        return item;
    }
}

