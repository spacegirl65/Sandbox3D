// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "TerrainCollider.h"
#include <algorithm>
#include <cmath>

namespace Sandbox3D::Engine
{
    using Maths::Vec3D;
    using Maths::Mat4x4D;
    using Maths::BoundingBoxD;
    using Maths::BoundingSphereD;
    using Maths::BoundingCapsuleD;
    using Maths::RayD;

    TerrainCollider::TerrainCollider()
        : Collider("Terrain", Vec3D{ 0.0, 0.0, 0.0 }, false)
    {
    }

    TerrainCollider::TerrainCollider(
        double width,
        double depth,
        HeightSampler sampler,
        double minElevation,
        double maxElevation,
        const Vec3D& offset
    )
        : Collider("Terrain", offset, false)
        , m_width(width)
        , m_depth(depth)
        , m_minElevation(minElevation)
        , m_maxElevation(maxElevation)
        , m_heightSampler(std::move(sampler))
    {
    }

    TerrainCollider::TerrainCollider(
        uint32_t resX,
        uint32_t resZ,
        double width,
        double depth,
        std::vector<float> elevations,
        double centerElevation,
        double scaleXZ,
        double scaleY,
        double minElevation,
        double maxElevation,
        const Vec3D& offset
    )
        : Collider("Terrain", offset, false)
        , m_resX(resX)
        , m_resZ(resZ)
        , m_width(width)
        , m_depth(depth)
        , m_elevations(std::move(elevations))
        , m_centerElevation(centerElevation)
        , m_scaleXZ(scaleXZ)
        , m_scaleY(scaleY)
        , m_minElevation(minElevation)
        , m_maxElevation(maxElevation)
    {
    }

    void TerrainCollider::SetElevationGrid(
        uint32_t resX,
        uint32_t resZ,
        double width,
        double depth,
        std::vector<float> elevations,
        double centerElevation,
        double scaleXZ,
        double scaleY,
        double minElevation,
        double maxElevation
    )
    {
        m_resX            = resX;
        m_resZ            = resZ;
        m_width           = width;
        m_depth           = depth;
        m_elevations      = std::move(elevations);
        m_centerElevation = centerElevation;
        m_scaleXZ         = scaleXZ;
        m_scaleY          = scaleY;
        m_minElevation    = minElevation;
        m_maxElevation    = maxElevation;
    }

    void TerrainCollider::SetHeightSampler(
        HeightSampler sampler,
        double width,
        double depth,
        double minElevation,
        double maxElevation
    )
    {
        m_heightSampler = std::move(sampler);
        m_width         = width;
        m_depth         = depth;
        m_minElevation  = minElevation;
        m_maxElevation  = maxElevation;
    }

    double TerrainCollider::SampleGridHeight(double localX, double localZ) const noexcept
    {
        if (m_elevations.empty() || m_resX < 2 || m_resZ < 2 || m_width <= 0.0 || m_depth <= 0.0)
        {
            return 0.0;
        }

        const double halfW = m_width * 0.5;
        const double halfD = m_depth * 0.5;

        // Map local coordinate to normalized grid unit coordinates
        const double u = ((localX + halfW) / m_width) * static_cast<double>(m_resX - 1);
        const double v = ((localZ + halfD) / m_depth) * static_cast<double>(m_resZ - 1);

        const double clampedU = std::clamp(u, 0.0, static_cast<double>(m_resX - 1));
        const double clampedV = std::clamp(v, 0.0, static_cast<double>(m_resZ - 1));

        const auto ix0 = static_cast<uint32_t>(std::floor(clampedU));
        const auto iz0 = static_cast<uint32_t>(std::floor(clampedV));
        const uint32_t ix1 = std::min(ix0 + 1, m_resX - 1);
        const uint32_t iz1 = std::min(iz0 + 1, m_resZ - 1);

        const double fx = clampedU - static_cast<double>(ix0);
        const double fz = clampedV - static_cast<double>(iz0);

        const double h00 = static_cast<double>(m_elevations[iz0 * m_resX + ix0]);
        const double h10 = static_cast<double>(m_elevations[iz0 * m_resX + ix1]);
        const double h01 = static_cast<double>(m_elevations[iz1 * m_resX + ix0]);
        const double h11 = static_cast<double>(m_elevations[iz1 * m_resX + ix1]);

        // Bilinear interpolation across grid quad
        const double h0 = h00 * (1.0 - fx) + h10 * fx;
        const double h1 = h01 * (1.0 - fx) + h11 * fx;
        return h0 * (1.0 - fz) + h1 * fz;
    }

    double TerrainCollider::GetHeightAt(double worldX, double worldZ) const noexcept
    {
        if (!m_elevations.empty())
        {
            // Transform world coordinates into local terrain space
            const double localX = (m_scaleXZ > 0.0) ? (worldX / m_scaleXZ) : worldX;
            const double localZ = (m_scaleXZ > 0.0) ? (worldZ / m_scaleXZ) : worldZ;
            const double localHeight = SampleGridHeight(localX, localZ);
            return (localHeight - m_centerElevation) * m_scaleY;
        }

        if (m_heightSampler)
        {
            return m_heightSampler(worldX, worldZ);
        }

        return 0.0;
    }

    bool TerrainCollider::IsInBounds(double worldX, double worldZ) const noexcept
    {
        const double worldHalfW = (m_width * 0.5) * (m_scaleXZ > 0.0 ? m_scaleXZ : 1.0);
        const double worldHalfD = (m_depth * 0.5) * (m_scaleXZ > 0.0 ? m_scaleXZ : 1.0);
        return (worldX >= -worldHalfW && worldX <= worldHalfW &&
                worldZ >= -worldHalfD && worldZ <= worldHalfD);
    }

    Vec3D TerrainCollider::GetNormalAt(double worldX, double worldZ) const noexcept
    {
        // Central finite differences step size (0.25m ensures stable curvature on sub-metre terrain)
        constexpr double eps = 0.25;
        const double hL = GetHeightAt(worldX - eps, worldZ);
        const double hR = GetHeightAt(worldX + eps, worldZ);
        const double hD = GetHeightAt(worldX, worldZ - eps);
        const double hU = GetHeightAt(worldX, worldZ + eps);

        const double dh_dx = (hR - hL) / (2.0 * eps);
        const double dh_dz = (hU - hD) / (2.0 * eps);

        const Vec3D normal(-dh_dx, 1.0, -dh_dz);
        return normal.Normalised();
    }

    double TerrainCollider::GetSlopeAt(double worldX, double worldZ) const noexcept
    {
        const Vec3D normal = GetNormalAt(worldX, worldZ);
        const double cosTheta = std::clamp(normal.y, 0.0, 1.0);
        return std::acos(cosTheta);
    }

    BoundingBoxD TerrainCollider::GetWorldBoundingBox(const Mat4x4D& worldTransform) const noexcept
    {
        const double worldHalfW = (m_width * 0.5) * (m_scaleXZ > 0.0 ? m_scaleXZ : 1.0);
        const double worldHalfD = (m_depth * 0.5) * (m_scaleXZ > 0.0 ? m_scaleXZ : 1.0);
        const double scaleY     = (m_scaleY > 0.0) ? m_scaleY : 1.0;
        const double minY       = (m_minElevation - m_centerElevation) * scaleY;
        const double maxY       = (m_maxElevation - m_centerElevation) * scaleY;

        const BoundingBoxD localBox(
            Vec3D(-worldHalfW, minY, -worldHalfD) + m_offset,
            Vec3D( worldHalfW, maxY,  worldHalfD) + m_offset
        );
        return localBox.Transformed(worldTransform);
    }

    BoundingSphereD TerrainCollider::GetWorldBoundingSphere(const Mat4x4D& worldTransform) const noexcept
    {
        const BoundingBoxD box = GetWorldBoundingBox(worldTransform);
        return BoundingSphereD(box.GetCenter(), box.GetExtents().Length());
    }

    bool TerrainCollider::Contains(const Vec3D& point, const Mat4x4D& worldTransform) const noexcept
    {
        // Transform point relative to collider transform
        const Vec3D localPoint = point - worldTransform.GetTranslation() - m_offset;
        if (!IsInBounds(localPoint.x, localPoint.z))
        {
            return false;
        }

        const double groundHeight = GetHeightAt(localPoint.x, localPoint.z);
        const double scaleY       = (m_scaleY > 0.0) ? m_scaleY : 1.0;
        const double bottomLimit  = (m_minElevation - m_centerElevation) * scaleY;

        return (localPoint.y <= groundHeight && localPoint.y >= bottomLimit);
    }

    TerrainContact TerrainCollider::TestPoint(const Vec3D& worldPoint) const noexcept
    {
        TerrainContact contact{};
        contact.groundHeight     = GetHeightAt(worldPoint.x, worldPoint.z);
        const double penetration = contact.groundHeight - worldPoint.y;

        contact.penetrationDepth = std::max(0.0, penetration);
        contact.hasContact       = (penetration >= 0.0);
        contact.contactPoint     = Vec3D(worldPoint.x, contact.groundHeight, worldPoint.z);
        contact.surfaceNormal    = GetNormalAt(worldPoint.x, worldPoint.z);
        contact.slopeAngle       = GetSlopeAt(worldPoint.x, worldPoint.z);

        return contact;
    }

    TerrainContact TerrainCollider::TestSphere(const Vec3D& worldCenter, double radius) const noexcept
    {
        TerrainContact contact{};
        contact.groundHeight     = GetHeightAt(worldCenter.x, worldCenter.z);
        const double lowestY     = worldCenter.y - radius;
        const double penetration = contact.groundHeight - lowestY;

        contact.penetrationDepth = std::max(0.0, penetration);
        contact.hasContact       = (penetration >= 0.0);
        contact.contactPoint     = Vec3D(worldCenter.x, contact.groundHeight, worldCenter.z);
        contact.surfaceNormal    = GetNormalAt(worldCenter.x, worldCenter.z);
        contact.slopeAngle       = GetSlopeAt(worldCenter.x, worldCenter.z);

        return contact;
    }

    TerrainContact TerrainCollider::TestCapsule(const BoundingCapsuleD& worldCapsule) const noexcept
    {
        TerrainContact contact{};
        // Identify lowest sphere center of capsule (typically base contact point for character)
        const Vec3D baseCenter = (worldCapsule.point0.y <= worldCapsule.point1.y) ? worldCapsule.point0 : worldCapsule.point1;
        contact.groundHeight   = GetHeightAt(baseCenter.x, baseCenter.z);

        const double lowestY     = baseCenter.y - worldCapsule.radius;
        const double penetration = contact.groundHeight - lowestY;

        contact.penetrationDepth = std::max(0.0, penetration);
        contact.hasContact       = (penetration >= 0.0);
        contact.contactPoint     = Vec3D(baseCenter.x, contact.groundHeight, baseCenter.z);
        contact.surfaceNormal    = GetNormalAt(baseCenter.x, baseCenter.z);
        contact.slopeAngle       = GetSlopeAt(baseCenter.x, baseCenter.z);

        return contact;
    }

    bool TerrainCollider::Intersects(const RayD& ray, const Mat4x4D& worldTransform, double* outDistance) const noexcept
    {
        // Fast O(1) downward raycast optimisation for vertical ground probes
        if (std::abs(ray.direction.x) < 1e-5 && std::abs(ray.direction.z) < 1e-5 && ray.direction.y < -1e-5)
        {
            if (IsInBounds(ray.origin.x, ray.origin.z))
            {
                const double groundHeight = GetHeightAt(ray.origin.x, ray.origin.z);
                const double t = (groundHeight - ray.origin.y) / ray.direction.y;
                if (t >= 0.0)
                {
                    if (outDistance)
                    {
                        *outDistance = t;
                    }
                    return true;
                }
            }
            return false;
        }

        // Bounding volume entry check
        const BoundingBoxD worldBox = GetWorldBoundingBox(worldTransform);
        double boxEntryDist = 0.0;
        if (!ray.Intersects(worldBox, boxEntryDist))
        {
            return false;
        }

        // Raymarching across the heightfield
        double t = std::max(0.0, boxEntryDist);
        constexpr double stepSize = 1.0; // 1 metre steps
        constexpr int maxSteps    = 400;

        double prevT = t;
        double prevDiff = 1.0;

        for (int step = 0; step < maxSteps; ++step)
        {
            const Vec3D p = ray.GetPoint(t);
            if (!IsInBounds(p.x, p.z))
            {
                if (step > 0)
                {
                    break;
                }
            }

            const double surfaceH = GetHeightAt(p.x, p.z);
            const double diff = p.y - surfaceH;

            if (diff <= 0.0)
            {
                // Surface penetrated: refine with binary search bisection
                double t0 = prevT;
                double t1 = t;
                for (int iter = 0; iter < 8; ++iter)
                {
                    const double tMid = (t0 + t1) * 0.5;
                    const Vec3D midP = ray.GetPoint(tMid);
                    const double midH = GetHeightAt(midP.x, midP.z);
                    if (midP.y <= midH)
                    {
                        t1 = tMid;
                    }
                    else
                    {
                        t0 = tMid;
                    }
                }

                if (outDistance)
                {
                    *outDistance = (t0 + t1) * 0.5;
                }
                return true;
            }

            prevT    = t;
            prevDiff = diff;
            t += stepSize;
        }

        return false;
    }

    bool TerrainCollider::IntersectsCapsule(const CapsuleCollider& capsule, const Mat4x4D& /*thisTransform*/, const Mat4x4D& otherTransform) const noexcept
    {
        const BoundingCapsuleD worldCapsule = capsule.GetWorldBoundingCapsule(otherTransform);
        return TestCapsule(worldCapsule).hasContact;
    }

    bool TerrainCollider::IntersectsSphere(const SphereCollider& sphere, const Mat4x4D& /*thisTransform*/, const Mat4x4D& otherTransform) const noexcept
    {
        const BoundingSphereD worldSphere = sphere.GetWorldBoundingSphere(otherTransform);
        return TestSphere(worldSphere.center, worldSphere.radius).hasContact;
    }

    bool TerrainCollider::IntersectsBox(const BoxCollider& box, const Mat4x4D& thisTransform, const Mat4x4D& otherTransform) const noexcept
    {
        const BoundingBoxD worldBox    = box.GetWorldBoundingBox(otherTransform);
        const BoundingBoxD terrainBox  = GetWorldBoundingBox(thisTransform);
        if (!terrainBox.Intersects(worldBox))
        {
            return false;
        }

        const Vec3D center = worldBox.GetCenter();
        const double groundHeight = GetHeightAt(center.x, center.z);
        return worldBox.min.y <= groundHeight;
    }

    bool TerrainCollider::Intersects(const Collider& other, const Mat4x4D& thisTransform, const Mat4x4D& otherTransform) const noexcept
    {
        if (other.IsCapsule())
        {
            const auto* capsule = dynamic_cast<const CapsuleCollider*>(&other);
            if (capsule)
            {
                return IntersectsCapsule(*capsule, thisTransform, otherTransform);
            }
        }

        if (other.IsSphere())
        {
            const auto* sphere = dynamic_cast<const SphereCollider*>(&other);
            if (sphere)
            {
                return IntersectsSphere(*sphere, thisTransform, otherTransform);
            }
        }

        if (other.IsBox())
        {
            const auto* box = dynamic_cast<const BoxCollider*>(&other);
            if (box)
            {
                return IntersectsBox(*box, thisTransform, otherTransform);
            }
        }

        return GetWorldBoundingBox(thisTransform).Intersects(other.GetWorldBoundingBox(otherTransform));
    }
}
