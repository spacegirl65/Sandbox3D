// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "CellCoord.h"
#include "Base.h"
#include "Maths/Maths.h"
#include "Renderer/Mesh.h"
#include "Renderer/Material.h"
#include "Renderer/RenderItem.h"

#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace Sandbox3D::Engine
{
    using Maths::Vec3D;
    using Maths::Mat4x4D;
    using Maths::Mat4x4;
    using Maths::BoundingBoxD;

    // Invisible 3D cubic cell representing a spatial volume segment in the multi-scale hierarchy
    class SpatialCell final
    {
    public:
        SpatialCell(const CellCoord& coord, double baseCellSize);
        ~SpatialCell() = default;

        SpatialCell(const SpatialCell&) = delete;
        SpatialCell& operator=(const SpatialCell&) = delete;
        SpatialCell(SpatialCell&&) noexcept = default;
        SpatialCell& operator=(SpatialCell&&) noexcept = default;

        // Coordinate and spatial bounds accessors
        [[nodiscard]] const CellCoord& GetCoord() const noexcept { return m_coord; }
        [[nodiscard]] const BoundingBoxD& GetBounds() const noexcept { return m_bounds; }
        [[nodiscard]] const Vec3D& GetCenter() const noexcept { return m_center; }
        [[nodiscard]] double GetSize() const noexcept { return m_size; }
        [[nodiscard]] uint32_t GetLevel() const noexcept { return m_coord.level; }

        // Geometry management
        void SetMesh(std::shared_ptr<Renderer::Mesh> mesh) noexcept;
        [[nodiscard]] const std::shared_ptr<Renderer::Mesh>& GetMesh() const noexcept { return m_mesh; }
        [[nodiscard]] bool HasMesh() const noexcept { return m_mesh != nullptr; }

        // Material management
        void SetMaterial(std::shared_ptr<Renderer::Material> material) noexcept;
        [[nodiscard]] const std::shared_ptr<Renderer::Material>& GetMaterial() const noexcept { return m_material; }

        // Entity membership management
        void AddEntity(std::shared_ptr<Base> entity);
        void RemoveEntity(uint32_t id);
        [[nodiscard]] std::span<const std::shared_ptr<Base>> GetEntities() const noexcept { return m_entities; }
        [[nodiscard]] size_t GetEntityCount() const noexcept { return m_entities.size(); }

        // Visibility and LOD state
        [[nodiscard]] bool IsVisible() const noexcept { return m_isVisible; }
        void SetVisible(bool visible) noexcept { m_isVisible = visible; }
        [[nodiscard]] double GetDistanceToCamera() const noexcept { return m_distanceToCamera; }
        void SetDistanceToCamera(double distance) noexcept { m_distanceToCamera = distance; }

        // Camera-relative world transformation evaluated from cell centre
        [[nodiscard]] Mat4x4 CalculateCameraRelativeWorld(const Vec3D& cameraPosition) const noexcept;
        [[nodiscard]] const Mat4x4D& GetWorldMatrix() const noexcept { return m_worldMatrix; }

        // Generates Direct3D 12 RenderItem for cell geometry (if mesh is present)
        [[nodiscard]] Renderer::RenderItem CreateRenderItem() const noexcept;

        // Generates Direct3D 12 RenderItem for cell wireframe debugging boundary
        [[nodiscard]] Renderer::RenderItem CreateDebugRenderItem(
            std::shared_ptr<Renderer::Mesh> debugMesh,
            std::shared_ptr<Renderer::Material> debugMaterial
        ) const noexcept;

    private:
        CellCoord                             m_coord;
        double                                m_size;
        Vec3D                                 m_center;
        BoundingBoxD                          m_bounds;
        Mat4x4D                               m_worldMatrix{ Mat4x4D::Identity() };

        std::shared_ptr<Renderer::Mesh>       m_mesh;
        std::shared_ptr<Renderer::Material>   m_material;
        std::vector<std::shared_ptr<Base>>    m_entities;

        double                                m_distanceToCamera{ 0.0 };
        bool                                  m_isVisible{ false };
    };
}

