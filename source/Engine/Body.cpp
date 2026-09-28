// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Body.h"
#include "TerrainCollider.h"

namespace Sandbox3D::Engine
{
    Body::Body(std::string_view name)
        : Base(name)
        , m_material(Material::CreateDefault())
    {
        SynchroniseRenderItem();
    }

    Body::Body(std::shared_ptr<Renderer::Mesh> mesh, std::string_view name)
        : Base(name)
        , m_mesh(std::move(mesh))
        , m_material(Material::CreateDefault())
    {
        SynchroniseRenderItem();
    }

    Body::Body(std::shared_ptr<Renderer::Mesh> mesh, std::shared_ptr<Collider> collider, std::string_view name)
        : Base(name)
        , m_mesh(std::move(mesh))
        , m_collider(std::move(collider))
        , m_material(Material::CreateDefault())
    {
        SynchroniseRenderItem();
    }

    Body::Body(std::shared_ptr<Renderer::Mesh> mesh, std::shared_ptr<Material> material, std::string_view name)
        : Base(name)
        , m_mesh(std::move(mesh))
        , m_material(material ? std::move(material) : Material::CreateDefault())
    {
        SynchroniseRenderItem();
    }

    Body::Body(std::shared_ptr<Renderer::Mesh> mesh, std::shared_ptr<Collider> collider, std::shared_ptr<Material> material, std::string_view name)
        : Base(name)
        , m_mesh(std::move(mesh))
        , m_collider(std::move(collider))
        , m_material(material ? std::move(material) : Material::CreateDefault())
    {
        SynchroniseRenderItem();
    }

    void Body::Update(float deltaTime)
    {
        const double dt = static_cast<double>(deltaTime);
        if (dt <= 0.0)
        {
            return;
        }

        // Apply Earth gravity (9.80655 m/s^2) in the negative Y axis direction
        m_worldVelocity.y -= EarthGravity * dt;

        // Update spatial position based on current linear velocity
        Maths::Vec3D pos = m_worldMatrix.GetTranslation();
        pos += m_worldVelocity * dt;
        m_worldMatrix.SetTranslation(pos);

        // Resolve ground contact against terrain collider if present
        if (m_terrainCollider)
        {
            ResolveTerrainCollision();
        }

        SynchroniseRenderItem();
    }

    void Body::ResolveTerrainCollision() noexcept
    {
        if (!m_terrainCollider)
        {
            return;
        }

        Maths::Vec3D pos = m_worldMatrix.GetTranslation();

        if (m_collider)
        {
            if (m_collider->IsCapsule())
            {
                const auto* capsule = static_cast<const CapsuleCollider*>(m_collider.get());
                const Maths::BoundingCapsuleD worldCapsule = capsule->GetWorldBoundingCapsule(m_worldMatrix);
                const TerrainContact contact = m_terrainCollider->TestCapsule(worldCapsule);
                m_groundHeight = contact.groundHeight;
                m_groundNormal = contact.surfaceNormal;

                if (contact.hasContact)
                {
                    pos.y += contact.penetrationDepth;
                    m_worldMatrix.SetTranslation(pos);
                    if (m_worldVelocity.y < 0.0)
                    {
                        m_worldVelocity.y = 0.0;
                    }
                    m_isGrounded = true;
                }
                else
                {
                    const double lowestY = std::min(worldCapsule.point0.y, worldCapsule.point1.y) - worldCapsule.radius;
                    constexpr double maxStepDown = 0.15; // 15 cm step-down allowance for walking smoothly downhill
                    if (m_isGrounded && (lowestY - contact.groundHeight) <= maxStepDown && m_worldVelocity.y <= 0.0)
                    {
                        pos.y -= (lowestY - contact.groundHeight);
                        m_worldMatrix.SetTranslation(pos);
                        m_worldVelocity.y = 0.0;
                        m_isGrounded = true;
                    }
                    else
                    {
                        m_isGrounded = (lowestY - contact.groundHeight <= 0.02);
                    }
                }
                return;
            }

            if (m_collider->IsSphere())
            {
                const auto* sphere = static_cast<const SphereCollider*>(m_collider.get());
                const Maths::BoundingSphereD worldSphere = sphere->GetWorldBoundingSphere(m_worldMatrix);
                const TerrainContact contact = m_terrainCollider->TestSphere(worldSphere.center, worldSphere.radius);
                m_groundHeight = contact.groundHeight;
                m_groundNormal = contact.surfaceNormal;

                if (contact.hasContact)
                {
                    pos.y += contact.penetrationDepth;
                    m_worldMatrix.SetTranslation(pos);
                    if (m_worldVelocity.y < 0.0)
                    {
                        m_worldVelocity.y = 0.0;
                    }
                    m_isGrounded = true;
                }
                else
                {
                    const double lowestY = worldSphere.center.y - worldSphere.radius;
                    m_isGrounded = (lowestY - contact.groundHeight <= 0.02);
                }
                return;
            }

            if (m_collider->IsBox())
            {
                const auto* box = static_cast<const BoxCollider*>(m_collider.get());
                const Maths::BoundingBoxD worldBox = box->GetWorldBoundingBox(m_worldMatrix);
                const Maths::Vec3D center = worldBox.GetCenter();
                const double groundHeight = m_terrainCollider->GetHeightAt(center.x, center.z);
                m_groundHeight = groundHeight;
                m_groundNormal = m_terrainCollider->GetNormalAt(center.x, center.z);

                if (worldBox.min.y <= groundHeight)
                {
                    pos.y += (groundHeight - worldBox.min.y);
                    m_worldMatrix.SetTranslation(pos);
                    if (m_worldVelocity.y < 0.0)
                    {
                        m_worldVelocity.y = 0.0;
                    }
                    m_isGrounded = true;
                }
                else
                {
                    m_isGrounded = (worldBox.min.y - groundHeight <= 0.02);
                }
                return;
            }
        }

        // Point-based ground collision fallback if no spatial collider is attached
        const double groundHeight = m_terrainCollider->GetHeightAt(pos.x, pos.z);
        m_groundHeight = groundHeight;
        m_groundNormal = m_terrainCollider->GetNormalAt(pos.x, pos.z);

        if (pos.y <= groundHeight)
        {
            pos.y = groundHeight;
            m_worldMatrix.SetTranslation(pos);
            if (m_worldVelocity.y < 0.0)
            {
                m_worldVelocity.y = 0.0;
            }
            m_isGrounded = true;
        }
        else
        {
            constexpr double maxStepDown = 0.15;
            if (m_isGrounded && (pos.y - groundHeight) <= maxStepDown && m_worldVelocity.y <= 0.0)
            {
                pos.y = groundHeight;
                m_worldMatrix.SetTranslation(pos);
                m_worldVelocity.y = 0.0;
                m_isGrounded = true;
            }
            else
            {
                m_isGrounded = (pos.y - groundHeight <= 0.02);
            }
        }
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

    void Body::SetMaterial(std::shared_ptr<Material> material)
    {
        m_material = material ? std::move(material) : Material::CreateDefault();
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

    std::shared_ptr<CapsuleCollider> Body::SetCapsuleCollider(double radius, double cylinderHeight, const Maths::Vec3D& offset)
    {
        auto collider = std::make_shared<CapsuleCollider>(radius, cylinderHeight, offset);
        m_collider = collider;
        return collider;
    }

    std::shared_ptr<CapsuleCollider> Body::SetCapsuleCollider(const Maths::Vec3D& point0, const Maths::Vec3D& point1, double radius)
    {
        auto collider = std::make_shared<CapsuleCollider>(point0, point1, radius);
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
        const Maths::Vec3D pos = GetPosition();
        return Maths::BoundingBoxD(pos, pos);
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
        return Maths::BoundingSphereD(GetPosition(), 0.0);
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
        m_renderItem.material    = m_material;
        m_renderItem.worldMatrix = m_worldMatrix;
        m_renderItem.isVisible   = m_isVisible;
        m_renderItem.name        = m_name;
    }
}

