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

    // Represents a physical simulation entity possessing a visual mesh and an optional spatial collider
    class Body : public Base
    {
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

        // Renderable query interface overrides
        [[nodiscard]] bool IsRenderable() const noexcept override { return m_mesh != nullptr; }
        [[nodiscard]] std::span<const Renderer::RenderItem> GetRenderItems() const noexcept override;

    protected:
        void SynchroniseRenderItem() const noexcept;

    protected:
        std::shared_ptr<Renderer::Mesh>   m_mesh;
        std::shared_ptr<Collider>         m_collider;
        std::shared_ptr<Material>         m_material;
        mutable Renderer::RenderItem      m_renderItem;
    };
}

