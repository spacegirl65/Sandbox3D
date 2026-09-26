// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Maths/Maths.h"

#include <memory>
#include <string>
#include <string_view>

namespace Sandbox3D::Renderer
{
    class Mesh;
}

namespace Sandbox3D::Engine
{
    using Maths::Vec3D;
    using Maths::Mat4x4D;
    using Maths::BoundingBoxD;
    using Maths::BoundingSphereD;
    using Maths::BoundingCapsuleD;
    using Maths::RayD;

    // Abstract base class representing spatial collision geometry for physical simulation
    class Collider
    {
    public:
        explicit Collider(std::string_view typeName, const Vec3D& offset = Vec3D{ 0.0, 0.0, 0.0 }, bool isTrigger = false);
        virtual ~Collider() = default;

        Collider(const Collider&) = default;
        Collider& operator=(const Collider&) = default;
        Collider(Collider&&) noexcept = default;
        Collider& operator=(Collider&&) noexcept = default;

        // Identification & type member properties
        [[nodiscard]] std::string_view GetTypeName() const noexcept { return m_typeName; }

        [[nodiscard]] virtual bool IsBox() const noexcept { return false; }
        [[nodiscard]] virtual bool IsSphere() const noexcept { return false; }
        [[nodiscard]] virtual bool IsCapsule() const noexcept { return false; }
        [[nodiscard]] virtual bool IsMesh() const noexcept { return false; }

        // Dynamic type query helpers
        template<typename T>
        [[nodiscard]] bool Is() const noexcept
        {
            return dynamic_cast<const T*>(this) != nullptr;
        }

        template<typename T>
        [[nodiscard]] T* As() noexcept
        {
            return dynamic_cast<T*>(this);
        }

        template<typename T>
        [[nodiscard]] const T* As() const noexcept
        {
            return dynamic_cast<const T*>(this);
        }

        // Spatial offset & trigger properties
        [[nodiscard]] const Vec3D& GetOffset() const noexcept { return m_offset; }
        void SetOffset(const Vec3D& offset) noexcept { m_offset = offset; }

        [[nodiscard]] bool IsTrigger() const noexcept { return m_isTrigger; }
        void SetTrigger(bool isTrigger) noexcept { m_isTrigger = isTrigger; }

        // Core spatial query contracts evaluated in world space given the parent entity's world transform
        [[nodiscard]] virtual BoundingBoxD GetWorldBoundingBox(const Mat4x4D& worldTransform) const noexcept = 0;
        [[nodiscard]] virtual BoundingSphereD GetWorldBoundingSphere(const Mat4x4D& worldTransform) const noexcept = 0;

        // Spatial intersection and point containment contracts
        [[nodiscard]] virtual bool Contains(const Vec3D& point, const Mat4x4D& worldTransform) const noexcept = 0;
        [[nodiscard]] virtual bool Intersects(const RayD& ray, const Mat4x4D& worldTransform, double* outDistance = nullptr) const noexcept = 0;
        [[nodiscard]] virtual bool Intersects(const Collider& other, const Mat4x4D& thisTransform, const Mat4x4D& otherTransform) const noexcept;

    protected:
        std::string  m_typeName;
        Vec3D        m_offset{ 0.0, 0.0, 0.0 };
        bool         m_isTrigger{ false };
    };

    // Axis-aligned / oriented box collider defined by half-extents along each primary axis
    class BoxCollider final : public Collider
    {
    public:
        BoxCollider();
        explicit BoxCollider(const Vec3D& halfExtents, const Vec3D& offset = Vec3D{ 0.0, 0.0, 0.0 }, bool isTrigger = false);

        [[nodiscard]] bool IsBox() const noexcept override { return true; }

        [[nodiscard]] const Vec3D& GetHalfExtents() const noexcept { return m_halfExtents; }
        void SetHalfExtents(const Vec3D& halfExtents) noexcept { m_halfExtents = halfExtents; }

        [[nodiscard]] Vec3D GetSize() const noexcept { return m_halfExtents * 2.0; }
        void SetSize(const Vec3D& size) noexcept { m_halfExtents = size * 0.5; }

        [[nodiscard]] BoundingBoxD GetLocalBoundingBox() const noexcept;
        [[nodiscard]] BoundingBoxD GetWorldBoundingBox(const Mat4x4D& worldTransform) const noexcept override;
        [[nodiscard]] BoundingSphereD GetWorldBoundingSphere(const Mat4x4D& worldTransform) const noexcept override;

        [[nodiscard]] bool Contains(const Vec3D& point, const Mat4x4D& worldTransform) const noexcept override;
        [[nodiscard]] bool Intersects(const RayD& ray, const Mat4x4D& worldTransform, double* outDistance = nullptr) const noexcept override;

        // Static factory helpers
        [[nodiscard]] static std::shared_ptr<BoxCollider> CreateFromBoundingBox(const BoundingBoxD& box);
        [[nodiscard]] static std::shared_ptr<BoxCollider> CreateFromMesh(const Renderer::Mesh& mesh);

    private:
        Vec3D m_halfExtents{ 0.5, 0.5, 0.5 };
    };

    // Spherical collider defined by radial distance from the local offset center
    class SphereCollider final : public Collider
    {
    public:
        SphereCollider();
        explicit SphereCollider(double radius, const Vec3D& offset = Vec3D{ 0.0, 0.0, 0.0 }, bool isTrigger = false);

        [[nodiscard]] bool IsSphere() const noexcept override { return true; }

        [[nodiscard]] double GetRadius() const noexcept { return m_radius; }
        void SetRadius(double radius) noexcept { m_radius = radius; }

        [[nodiscard]] BoundingSphereD GetLocalBoundingSphere() const noexcept;
        [[nodiscard]] BoundingBoxD GetWorldBoundingBox(const Mat4x4D& worldTransform) const noexcept override;
        [[nodiscard]] BoundingSphereD GetWorldBoundingSphere(const Mat4x4D& worldTransform) const noexcept override;

        [[nodiscard]] bool Contains(const Vec3D& point, const Mat4x4D& worldTransform) const noexcept override;
        [[nodiscard]] bool Intersects(const RayD& ray, const Mat4x4D& worldTransform, double* outDistance = nullptr) const noexcept override;

        // Static factory helpers
        [[nodiscard]] static std::shared_ptr<SphereCollider> CreateFromBoundingSphere(const BoundingSphereD& sphere);
        [[nodiscard]] static std::shared_ptr<SphereCollider> CreateFromMesh(const Renderer::Mesh& mesh);

    private:
        double m_radius{ 0.5 };
    };

    // Capsule collider defined by radius and cylindrical segment height along the local Y axis
    class CapsuleCollider final : public Collider
    {
    public:
        CapsuleCollider();
        explicit CapsuleCollider(
            double radius,
            double cylinderHeight,
            const Vec3D& offset = Vec3D{ 0.0, 0.0, 0.0 },
            bool isTrigger = false
        );
        CapsuleCollider(
            const Vec3D& point0,
            const Vec3D& point1,
            double radius,
            bool isTrigger = false
        );

        [[nodiscard]] bool IsCapsule() const noexcept override { return true; }

        [[nodiscard]] double GetRadius() const noexcept { return m_radius; }
        void SetRadius(double radius) noexcept { m_radius = radius; }

        [[nodiscard]] double GetCylinderHeight() const noexcept { return m_cylinderHeight; }
        void SetCylinderHeight(double cylinderHeight) noexcept { m_cylinderHeight = cylinderHeight; }

        [[nodiscard]] double GetTotalHeight() const noexcept { return m_cylinderHeight + m_radius * 2.0; }

        [[nodiscard]] BoundingCapsuleD GetLocalBoundingCapsule() const noexcept;
        [[nodiscard]] BoundingCapsuleD GetWorldBoundingCapsule(const Mat4x4D& worldTransform) const noexcept;

        [[nodiscard]] BoundingBoxD GetWorldBoundingBox(const Mat4x4D& worldTransform) const noexcept override;
        [[nodiscard]] BoundingSphereD GetWorldBoundingSphere(const Mat4x4D& worldTransform) const noexcept override;

        [[nodiscard]] bool Contains(const Vec3D& point, const Mat4x4D& worldTransform) const noexcept override;
        [[nodiscard]] bool Intersects(const RayD& ray, const Mat4x4D& worldTransform, double* outDistance = nullptr) const noexcept override;
        [[nodiscard]] bool Intersects(const Collider& other, const Mat4x4D& thisTransform, const Mat4x4D& otherTransform) const noexcept override;

        // Static factory helpers
        [[nodiscard]] static std::shared_ptr<CapsuleCollider> CreateFromBoundingCapsule(const BoundingCapsuleD& capsule);

    private:
        double m_radius{ 0.35 };
        double m_cylinderHeight{ 0.70 };
    };
}

