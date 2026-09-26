// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Collider.h"
#include "Renderer/Mesh.h"

namespace Sandbox3D::Engine
{
    // =========================================================================
    // Base Collider
    // =========================================================================

    Collider::Collider(std::string_view typeName, const Vec3D& offset, bool isTrigger)
        : m_typeName(typeName)
        , m_offset(offset)
        , m_isTrigger(isTrigger)
    {
    }

    bool Collider::Intersects(const Collider& other, const Mat4x4D& thisTransform, const Mat4x4D& otherTransform) const noexcept
    {
        if (IsSphere() && other.IsSphere())
        {
            return GetWorldBoundingSphere(thisTransform).Intersects(other.GetWorldBoundingSphere(otherTransform));
        }

        if (IsSphere() && other.IsBox())
        {
            return GetWorldBoundingSphere(thisTransform).Intersects(other.GetWorldBoundingBox(otherTransform));
        }

        if (IsBox() && other.IsSphere())
        {
            return GetWorldBoundingBox(thisTransform).Intersects(other.GetWorldBoundingSphere(otherTransform));
        }

        if (IsCapsule() && other.IsCapsule())
        {
            const auto* c1 = dynamic_cast<const CapsuleCollider*>(this);
            const auto* c2 = dynamic_cast<const CapsuleCollider*>(&other);
            if (c1 && c2)
            {
                return c1->GetWorldBoundingCapsule(thisTransform).Intersects(c2->GetWorldBoundingCapsule(otherTransform));
            }
        }

        if (IsCapsule() && other.IsSphere())
        {
            const auto* c = dynamic_cast<const CapsuleCollider*>(this);
            if (c)
            {
                return c->GetWorldBoundingCapsule(thisTransform).Intersects(other.GetWorldBoundingSphere(otherTransform));
            }
        }

        if (IsSphere() && other.IsCapsule())
        {
            const auto* c = dynamic_cast<const CapsuleCollider*>(&other);
            if (c)
            {
                return c->GetWorldBoundingCapsule(otherTransform).Intersects(GetWorldBoundingSphere(thisTransform));
            }
        }

        if (IsCapsule() && other.IsBox())
        {
            const auto* c = dynamic_cast<const CapsuleCollider*>(this);
            if (c)
            {
                return c->GetWorldBoundingCapsule(thisTransform).Intersects(other.GetWorldBoundingBox(otherTransform));
            }
        }

        if (IsBox() && other.IsCapsule())
        {
            const auto* c = dynamic_cast<const CapsuleCollider*>(&other);
            if (c)
            {
                return c->GetWorldBoundingCapsule(otherTransform).Intersects(GetWorldBoundingBox(thisTransform));
            }
        }

        // Default to axis-aligned bounding box intersection for box-box or general shapes
        return GetWorldBoundingBox(thisTransform).Intersects(other.GetWorldBoundingBox(otherTransform));
    }

    // =========================================================================
    // BoxCollider
    // =========================================================================

    BoxCollider::BoxCollider()
        : Collider("Box", Vec3D{ 0.0, 0.0, 0.0 }, false)
        , m_halfExtents(0.5, 0.5, 0.5)
    {
    }

    BoxCollider::BoxCollider(const Vec3D& halfExtents, const Vec3D& offset, bool isTrigger)
        : Collider("Box", offset, isTrigger)
        , m_halfExtents(halfExtents)
    {
    }

    BoundingBoxD BoxCollider::GetLocalBoundingBox() const noexcept
    {
        return BoundingBoxD(m_offset - m_halfExtents, m_offset + m_halfExtents);
    }

    BoundingBoxD BoxCollider::GetWorldBoundingBox(const Mat4x4D& worldTransform) const noexcept
    {
        return GetLocalBoundingBox().Transformed(worldTransform);
    }

    BoundingSphereD BoxCollider::GetWorldBoundingSphere(const Mat4x4D& worldTransform) const noexcept
    {
        const BoundingBoxD worldBox = GetWorldBoundingBox(worldTransform);
        return BoundingSphereD(worldBox.GetCenter(), worldBox.GetExtents().Length());
    }

    bool BoxCollider::Contains(const Vec3D& point, const Mat4x4D& worldTransform) const noexcept
    {
        const Mat4x4D invTransform = worldTransform.Inverted();
        const Vec3D localPoint = invTransform.TransformPoint(point);
        return GetLocalBoundingBox().Contains(localPoint);
    }

    bool BoxCollider::Intersects(const RayD& ray, const Mat4x4D& worldTransform, double* outDistance) const noexcept
    {
        const BoundingBoxD worldBox = GetWorldBoundingBox(worldTransform);
        double hitDistance = 0.0;
        if (ray.Intersects(worldBox, hitDistance))
        {
            if (outDistance != nullptr)
            {
                *outDistance = hitDistance;
            }
            return true;
        }
        return false;
    }

    std::shared_ptr<BoxCollider> BoxCollider::CreateFromBoundingBox(const BoundingBoxD& box)
    {
        const Vec3D center  = box.GetCenter();
        const Vec3D extents = box.GetExtents();
        return std::make_shared<BoxCollider>(extents, center);
    }

    std::shared_ptr<BoxCollider> BoxCollider::CreateFromMesh(const Renderer::Mesh& mesh)
    {
        const Maths::BoundingBox localBox = mesh.GetBoundingBox();
        const BoundingBoxD boxD(localBox);
        return CreateFromBoundingBox(boxD);
    }

    // =========================================================================
    // SphereCollider
    // =========================================================================

    SphereCollider::SphereCollider()
        : Collider("Sphere", Vec3D{ 0.0, 0.0, 0.0 }, false)
        , m_radius(0.5)
    {
    }

    SphereCollider::SphereCollider(double radius, const Vec3D& offset, bool isTrigger)
        : Collider("Sphere", offset, isTrigger)
        , m_radius(radius)
    {
    }

    BoundingSphereD SphereCollider::GetLocalBoundingSphere() const noexcept
    {
        return BoundingSphereD(m_offset, m_radius);
    }

    BoundingSphereD SphereCollider::GetWorldBoundingSphere(const Mat4x4D& worldTransform) const noexcept
    {
        return GetLocalBoundingSphere().Transformed(worldTransform);
    }

    BoundingBoxD SphereCollider::GetWorldBoundingBox(const Mat4x4D& worldTransform) const noexcept
    {
        const BoundingSphereD worldSphere = GetWorldBoundingSphere(worldTransform);
        const Vec3D r(worldSphere.radius, worldSphere.radius, worldSphere.radius);
        return BoundingBoxD(worldSphere.center - r, worldSphere.center + r);
    }

    bool SphereCollider::Contains(const Vec3D& point, const Mat4x4D& worldTransform) const noexcept
    {
        return GetWorldBoundingSphere(worldTransform).Contains(point);
    }

    bool SphereCollider::Intersects(const RayD& ray, const Mat4x4D& worldTransform, double* outDistance) const noexcept
    {
        const BoundingSphereD worldSphere = GetWorldBoundingSphere(worldTransform);
        double hitDistance = 0.0;
        if (ray.Intersects(worldSphere, hitDistance))
        {
            if (outDistance != nullptr)
            {
                *outDistance = hitDistance;
            }
            return true;
        }
        return false;
    }

    std::shared_ptr<SphereCollider> SphereCollider::CreateFromBoundingSphere(const BoundingSphereD& sphere)
    {
        return std::make_shared<SphereCollider>(sphere.radius, sphere.center);
    }

    std::shared_ptr<SphereCollider> SphereCollider::CreateFromMesh(const Renderer::Mesh& mesh)
    {
        const Maths::BoundingSphere localSphere = mesh.GetBoundingSphere();
        const BoundingSphereD sphereD(localSphere);
        return CreateFromBoundingSphere(sphereD);
    }

    // =========================================================================
    // CapsuleCollider
    // =========================================================================

    CapsuleCollider::CapsuleCollider()
        : Collider("Capsule", Vec3D{ 0.0, 0.0, 0.0 }, false)
        , m_radius(0.35)
        , m_cylinderHeight(0.70)
    {
    }

    CapsuleCollider::CapsuleCollider(
        double radius,
        double cylinderHeight,
        const Vec3D& offset,
        bool isTrigger
    )
        : Collider("Capsule", offset, isTrigger)
        , m_radius(radius)
        , m_cylinderHeight(cylinderHeight)
    {
    }

    CapsuleCollider::CapsuleCollider(
        const Vec3D& point0,
        const Vec3D& point1,
        double radius,
        bool isTrigger
    )
        : Collider("Capsule", (point0 + point1) * 0.5, isTrigger)
        , m_radius(radius)
        , m_cylinderHeight((point1 - point0).Length())
    {
    }

    BoundingCapsuleD CapsuleCollider::GetLocalBoundingCapsule() const noexcept
    {
        return BoundingCapsuleD::FromVertical(m_offset, m_radius, m_cylinderHeight);
    }

    BoundingCapsuleD CapsuleCollider::GetWorldBoundingCapsule(const Mat4x4D& worldTransform) const noexcept
    {
        return GetLocalBoundingCapsule().Transformed(worldTransform);
    }

    BoundingBoxD CapsuleCollider::GetWorldBoundingBox(const Mat4x4D& worldTransform) const noexcept
    {
        return GetWorldBoundingCapsule(worldTransform).GetBoundingBox();
    }

    BoundingSphereD CapsuleCollider::GetWorldBoundingSphere(const Mat4x4D& worldTransform) const noexcept
    {
        return GetWorldBoundingCapsule(worldTransform).GetBoundingSphere();
    }

    bool CapsuleCollider::Contains(const Vec3D& point, const Mat4x4D& worldTransform) const noexcept
    {
        return GetWorldBoundingCapsule(worldTransform).Contains(point);
    }

    bool CapsuleCollider::Intersects(const RayD& ray, const Mat4x4D& worldTransform, double* outDistance) const noexcept
    {
        const BoundingCapsuleD worldCapsule = GetWorldBoundingCapsule(worldTransform);
        double hitDistance = 0.0;
        if (worldCapsule.Intersects(ray, hitDistance))
        {
            if (outDistance != nullptr)
            {
                *outDistance = hitDistance;
            }
            return true;
        }
        return false;
    }

    bool CapsuleCollider::Intersects(const Collider& other, const Mat4x4D& thisTransform, const Mat4x4D& otherTransform) const noexcept
    {
        return Collider::Intersects(other, thisTransform, otherTransform);
    }

    std::shared_ptr<CapsuleCollider> CapsuleCollider::CreateFromBoundingCapsule(const BoundingCapsuleD& capsule)
    {
        return std::make_shared<CapsuleCollider>(
            capsule.radius,
            capsule.GetSegmentLength(),
            capsule.GetCenter()
        );
    }
}

