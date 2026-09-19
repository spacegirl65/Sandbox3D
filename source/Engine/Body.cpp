// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Body.h"

namespace Sandbox3D::Engine
{
    Body::Body(std::string_view name)
        : Base(name)
    {
        SynchroniseRenderItem();
    }

    Body::Body(std::shared_ptr<Renderer::Mesh> mesh, std::string_view name)
        : Base(name)
        , m_mesh(std::move(mesh))
    {
        SynchroniseRenderItem();
    }

    Body::Body(std::shared_ptr<Renderer::Mesh> mesh, std::shared_ptr<Collider> collider, std::string_view name)
        : Base(name)
        , m_mesh(std::move(mesh))
        , m_collider(std::move(collider))
    {
        SynchroniseRenderItem();
    }

    void Body::Update([[maybe_unused]] float deltaTime)
    {
        // Core polymorphic update loop for Body entity.
        // Physics execution will be handled here or in specialised subclasses.
    }

    void Body::SetPosition(const Maths::Vec3D& position)
    {
        Base::SetPosition(position);
        SynchroniseRenderItem();
    }

    void Body::SetWorldMatrix(const Maths::Mat4x4D& worldMatrix)
    {
        Base::SetWorldMatrix(worldMatrix);
        SynchroniseRenderItem();
    }

    void Body::SetVisible(bool visible) noexcept
    {
        Base::SetVisible(visible);
        SynchroniseRenderItem();
    }

    void Body::SetMesh(std::shared_ptr<Renderer::Mesh> mesh)
    {
        m_mesh = std::move(mesh);
        SynchroniseRenderItem();
    }

    void Body::SetCollider(std::shared_ptr<Collider> collider)
    {
        m_collider = std::move(collider);
    }

    void Body::RemoveCollider() noexcept
    {
        m_collider.reset();
    }

    std::shared_ptr<BoxCollider> Body::SetBoxCollider(const Maths::Vec3D& halfExtents, const Maths::Vec3D& offset)
    {
        auto collider = std::make_shared<BoxCollider>(halfExtents, offset);
        m_collider = collider;
        return collider;
    }

    std::shared_ptr<SphereCollider> Body::SetSphereCollider(double radius, const Maths::Vec3D& offset)
    {
        auto collider = std::make_shared<SphereCollider>(radius, offset);
        m_collider = collider;
        return collider;
    }

    std::shared_ptr<Collider> Body::GenerateColliderFromMesh()
    {
        if (m_mesh)
        {
            m_collider = BoxCollider::CreateFromMesh(*m_mesh);
            return m_collider;
        }
        return nullptr;
    }

    bool Body::Intersects(const Body& other) const noexcept
    {
        if (!m_collider || !other.m_collider)
        {
            return false;
        }
        return m_collider->Intersects(*other.m_collider, m_worldMatrix, other.m_worldMatrix);
    }

    bool Body::Intersects(const Maths::RayD& ray, double* outDistance) const noexcept
    {
        if (!m_collider)
        {
            return false;
        }
        return m_collider->Intersects(ray, m_worldMatrix, outDistance);
    }

    Maths::BoundingBoxD Body::GetWorldBoundingBox() const noexcept
    {
        if (m_collider)
        {
            return m_collider->GetWorldBoundingBox(m_worldMatrix);
        }
        if (m_mesh)
        {
            return Maths::BoundingBoxD(m_mesh->GetBoundingBox()).Transformed(m_worldMatrix);
        }
        return Maths::BoundingBoxD(m_position, m_position);
    }

    Maths::BoundingSphereD Body::GetWorldBoundingSphere() const noexcept
    {
        if (m_collider)
        {
            return m_collider->GetWorldBoundingSphere(m_worldMatrix);
        }
        if (m_mesh)
        {
            return Maths::BoundingSphereD(m_mesh->GetBoundingSphere()).Transformed(m_worldMatrix);
        }
        return Maths::BoundingSphereD(m_position, 0.0);
    }

    std::span<const Renderer::RenderItem> Body::GetRenderItems() const noexcept
    {
        if (!m_mesh || !m_isVisible)
        {
            return {};
        }
        SynchroniseRenderItem();
        return std::span<const Renderer::RenderItem>(&m_renderItem, 1);
    }

    void Body::SynchroniseRenderItem() const noexcept
    {
        m_renderItem.mesh        = m_mesh;
        m_renderItem.worldMatrix = m_worldMatrix;
        m_renderItem.isVisible   = m_isVisible;
        m_renderItem.name        = m_name;
    }
}

