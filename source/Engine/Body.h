// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Base.h"
#include "Collider.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/RenderItem.h"

#include <memory>
#include <span>
#include <string_view>

namespace Sandbox3D::Engine
{
    using Renderer::Material;
    class TerrainCollider;

    // Represents a physical simulation entity possessing a visual mesh and an optional spatial collider
    class Body : public Base
    {
    public:
        // Physical constants: standard acceleration due to Earth gravity in m/s^2
        static constexpr double EarthGravity = 9.80655;

    public:
        explicit Body(std::string_view name = "Body");
        explicit Body(std::shared_ptr<Renderer::Mesh> mesh, std::string_view name = "Body");
        Body(std::shared_ptr<Renderer::Mesh> mesh, std::shared_ptr<Collider> collider, std::string_view name = "Body");
        Body(std::shared_ptr<Renderer::Mesh> mesh, std::shared_ptr<Material> material, std::string_view name = "Body");
        Body(std::shared_ptr<Renderer::Mesh> mesh, std::shared_ptr<Collider> collider, std::shared_ptr<Material> material, std::string_view name = "Body");
        ~Body() override = default;

        Body(const Body&) = delete;
        Body& operator=(const Body&) = delete;
        Body(Body&&) noexcept = default;
        Body& operator=(Body&&) noexcept = default;

        // Core polymorphic update loop (to be extended by physics systems or specialised subclasses)
        void Update(float deltaTime) override;

        // Spatial transform configuration overrides
        void SetPosition(const Maths::Vec3D& position) override;
        void SetWorldMatrix(const Maths::Mat4x4D& worldMatrix) override;
        void SetVisible(bool visible) noexcept override;

        // Visual mesh access & configuration
        [[nodiscard]] std::shared_ptr<Renderer::Mesh> GetMesh() const noexcept { return m_mesh; }
        void SetMesh(std::shared_ptr<Renderer::Mesh> mesh);

        // Material access & configuration
        [[nodiscard]] std::shared_ptr<Material> GetMaterial() const noexcept { return m_material; }
        void SetMaterial(std::shared_ptr<Material> material);
        [[nodiscard]] bool HasMaterial() const noexcept { return m_material != nullptr; }

        // Spatial collider access & configuration
        [[nodiscard]] bool HasCollider() const noexcept { return m_collider != nullptr; }
        [[nodiscard]] std::shared_ptr<Collider> GetCollider() const noexcept { return m_collider; }
        void SetCollider(std::shared_ptr<Collider> collider);
        void RemoveCollider() noexcept;

        // Spatial collider type queries via member properties
        [[nodiscard]] std::string_view GetColliderTypeName() const noexcept { return m_collider ? m_collider->GetTypeName() : "None"; }
        [[nodiscard]] bool IsBoxCollider() const noexcept { return m_collider && m_collider->IsBox(); }
        [[nodiscard]] bool IsSphereCollider() const noexcept { return m_collider && m_collider->IsSphere(); }
        [[nodiscard]] bool IsCapsuleCollider() const noexcept { return m_collider && m_collider->IsCapsule(); }
        [[nodiscard]] bool IsTerrainCollider() const noexcept { return m_collider && m_collider->IsTerrain(); }

        template<typename T>
        [[nodiscard]] bool HasColliderOfType() const noexcept
        {
            return m_collider && m_collider->Is<T>();
        }

        template<typename T>
        [[nodiscard]] std::shared_ptr<T> GetColliderAs() const noexcept
        {
            return std::dynamic_pointer_cast<T>(m_collider);
        }

        // Convenience collider creation helpers
        std::shared_ptr<BoxCollider> SetBoxCollider(const Maths::Vec3D& halfExtents, const Maths::Vec3D& offset = Maths::Vec3D{ 0.0, 0.0, 0.0 });
        std::shared_ptr<SphereCollider> SetSphereCollider(double radius, const Maths::Vec3D& offset = Maths::Vec3D{ 0.0, 0.0, 0.0 });
        std::shared_ptr<CapsuleCollider> SetCapsuleCollider(double radius, double cylinderHeight, const Maths::Vec3D& offset = Maths::Vec3D{ 0.0, 0.0, 0.0 });
        std::shared_ptr<CapsuleCollider> SetCapsuleCollider(const Maths::Vec3D& point0, const Maths::Vec3D& point1, double radius);
        std::shared_ptr<Collider> GenerateColliderFromMesh();

        // Spatial & collision query interface
        [[nodiscard]] bool Intersects(const Body& other) const noexcept;
        [[nodiscard]] bool Intersects(const Maths::RayD& ray, double* outDistance = nullptr) const noexcept;
        [[nodiscard]] Maths::BoundingBoxD GetWorldBoundingBox() const noexcept;
        [[nodiscard]] Maths::BoundingSphereD GetWorldBoundingSphere() const noexcept;

        // Kinematics and world velocity (Vec3)
        [[nodiscard]] const Maths::Vec3D& GetVelocity() const noexcept { return m_worldVelocity; }
        void SetVelocity(const Maths::Vec3D& velocity) noexcept { m_worldVelocity = velocity; }

        // Ground contact & terrain collision
        [[nodiscard]] bool IsGrounded() const noexcept { return m_isGrounded; }
        [[nodiscard]] double GetGroundHeight() const noexcept { return m_groundHeight; }
        [[nodiscard]] const Maths::Vec3D& GetGroundNormal() const noexcept { return m_groundNormal; }

        void SetTerrainCollider(std::shared_ptr<TerrainCollider> collider) noexcept { m_terrainCollider = std::move(collider); }
        [[nodiscard]] std::shared_ptr<TerrainCollider> GetTerrainCollider() const noexcept { return m_terrainCollider; }

        // Renderable query interface overrides
        [[nodiscard]] bool IsRenderable() const noexcept override { return m_mesh != nullptr; }
        [[nodiscard]] std::span<const Renderer::RenderItem> GetRenderItems() const noexcept override;

    protected:
        void SynchroniseRenderItem() const noexcept;
        void ResolveTerrainCollision() noexcept;

    protected:
        std::shared_ptr<Renderer::Mesh>   m_mesh;
        std::shared_ptr<Collider>         m_collider;
        std::shared_ptr<Material>         m_material;
        mutable Renderer::RenderItem      m_renderItem;
        Maths::Vec3D                      m_worldVelocity{ 0.0, 0.0, 0.0 };
        std::shared_ptr<TerrainCollider>  m_terrainCollider;
        Maths::Vec3D                      m_groundNormal{ 0.0, 1.0, 0.0 };
        double                            m_groundHeight{ 0.0 };
        bool                              m_isGrounded{ false };
    };
}

