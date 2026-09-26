// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Collider.h"
#include <functional>
#include <vector>

namespace Sandbox3D::Engine
{
    // Result details for contact queries against the terrain surface
    struct TerrainContact
    {
        bool         hasContact{ false };
        double       groundHeight{ 0.0 };
        double       penetrationDepth{ 0.0 }; // Positive when penetrating below surface
        Maths::Vec3D surfaceNormal{ 0.0, 1.0, 0.0 };
        Maths::Vec3D contactPoint{ 0.0, 0.0, 0.0 };
        double       slopeAngle{ 0.0 };       // Slope in radians relative to vertical (0 = flat, pi/2 = vertical)
    };

    // Heightfield-based custom spatial collider representing complex landscape geometry
    class TerrainCollider final : public Collider
    {
    public:
        using HeightSampler = std::function<double(double worldX, double worldZ)>;

    public:
        TerrainCollider();

        // Continuous analytical height sampler constructor (for procedural landscapes)
        TerrainCollider(
            double width,
            double depth,
            HeightSampler sampler,
            double minElevation = -100.0,
            double maxElevation = 500.0,
            const Maths::Vec3D& offset = Maths::Vec3D{ 0.0, 0.0, 0.0 }
        );

        // Discrete elevation grid constructor (for LiDAR heightfield scans)
        TerrainCollider(
            uint32_t resX,
            uint32_t resZ,
            double width,
            double depth,
            std::vector<float> elevations,
            double centerElevation = 333.794,
            double scaleXZ = 1.0,
            double scaleY = 1.0,
            double minElevation = -100.0,
            double maxElevation = 1000.0,
            const Maths::Vec3D& offset = Maths::Vec3D{ 0.0, 0.0, 0.0 }
        );

        ~TerrainCollider() override = default;

        TerrainCollider(const TerrainCollider&) = default;
        TerrainCollider& operator=(const TerrainCollider&) = default;
        TerrainCollider(TerrainCollider&&) noexcept = default;
        TerrainCollider& operator=(TerrainCollider&&) noexcept = default;

        // Type identification
        [[nodiscard]] bool IsTerrain() const noexcept override { return true; }

        // Core spatial query contracts
        [[nodiscard]] Maths::BoundingBoxD GetWorldBoundingBox(const Maths::Mat4x4D& worldTransform) const noexcept override;
        [[nodiscard]] Maths::BoundingSphereD GetWorldBoundingSphere(const Maths::Mat4x4D& worldTransform) const noexcept override;

        // Point containment: checks if a point is within the terrain footprint and below the surface
        [[nodiscard]] bool Contains(const Maths::Vec3D& point, const Maths::Mat4x4D& worldTransform) const noexcept override;

        // Raycast query: finds entry point/intersection with terrain surface
        [[nodiscard]] bool Intersects(const Maths::RayD& ray, const Maths::Mat4x4D& worldTransform, double* outDistance = nullptr) const noexcept override;

        // Collider-to-collider intersection
        [[nodiscard]] bool Intersects(const Collider& other, const Maths::Mat4x4D& thisTransform, const Maths::Mat4x4D& otherTransform) const noexcept override;

        // Specific primitive intersection tests
        [[nodiscard]] bool IntersectsCapsule(const CapsuleCollider& capsule, const Maths::Mat4x4D& thisTransform, const Maths::Mat4x4D& otherTransform) const noexcept;
        [[nodiscard]] bool IntersectsSphere(const SphereCollider& sphere, const Maths::Mat4x4D& thisTransform, const Maths::Mat4x4D& otherTransform) const noexcept;
        [[nodiscard]] bool IntersectsBox(const BoxCollider& box, const Maths::Mat4x4D& thisTransform, const Maths::Mat4x4D& otherTransform) const noexcept;

        // Locomotion & contact testing helpers evaluated in world space
        [[nodiscard]] TerrainContact TestPoint(const Maths::Vec3D& worldPoint) const noexcept;
        [[nodiscard]] TerrainContact TestSphere(const Maths::Vec3D& worldCenter, double radius) const noexcept;
        [[nodiscard]] TerrainContact TestCapsule(const Maths::BoundingCapsuleD& worldCapsule) const noexcept;

        // Continuous elevation and surface geometry queries
        [[nodiscard]] double GetHeightAt(double worldX, double worldZ) const noexcept;
        [[nodiscard]] Maths::Vec3D GetNormalAt(double worldX, double worldZ) const noexcept;
        [[nodiscard]] double GetSlopeAt(double worldX, double worldZ) const noexcept; // Angle in radians [0, pi/2]
        [[nodiscard]] bool IsInBounds(double worldX, double worldZ) const noexcept;

        // Configuration accessors
        [[nodiscard]] uint32_t GetResolutionX() const noexcept { return m_resX; }
        [[nodiscard]] uint32_t GetResolutionZ() const noexcept { return m_resZ; }
        [[nodiscard]] double GetWidth() const noexcept { return m_width; }
        [[nodiscard]] double GetDepth() const noexcept { return m_depth; }
        [[nodiscard]] double GetScaleXZ() const noexcept { return m_scaleXZ; }
        [[nodiscard]] double GetScaleY() const noexcept { return m_scaleY; }
        [[nodiscard]] double GetCenterElevation() const noexcept { return m_centerElevation; }
        [[nodiscard]] double GetMinElevation() const noexcept { return m_minElevation; }
        [[nodiscard]] double GetMaxElevation() const noexcept { return m_maxElevation; }
        [[nodiscard]] bool HasElevationGrid() const noexcept { return !m_elevations.empty(); }

        // Configuration mutators
        void SetElevationGrid(
            uint32_t resX,
            uint32_t resZ,
            double width,
            double depth,
            std::vector<float> elevations,
            double centerElevation = 333.794,
            double scaleXZ = 1.0,
            double scaleY = 1.0,
            double minElevation = -100.0,
            double maxElevation = 1000.0
        );

        void SetHeightSampler(
            HeightSampler sampler,
            double width,
            double depth,
            double minElevation = -100.0,
            double maxElevation = 500.0
        );

    private:
        [[nodiscard]] double SampleGridHeight(double localX, double localZ) const noexcept;

    private:
        uint32_t           m_resX{ 0 };
        uint32_t           m_resZ{ 0 };
        double             m_width{ 0.0 };
        double             m_depth{ 0.0 };
        std::vector<float> m_elevations;
        double             m_centerElevation{ 0.0 };
        double             m_scaleXZ{ 1.0 };
        double             m_scaleY{ 1.0 };
        double             m_minElevation{ -100.0 };
        double             m_maxElevation{ 500.0 };
        HeightSampler      m_heightSampler;
    };
}

