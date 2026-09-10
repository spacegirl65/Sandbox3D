// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "BoundingBox.h"
#include "BoundingSphere.h"
#include "Plane.h"

#include <algorithm>

namespace Sandbox3D::Maths
{
    template <std::floating_point T>
    _BoundingBox<T> _BoundingBox<T>::FromPoints(std::span<const _Vec3<T>> points) noexcept
    {
        if (points.empty())
        {
            return _BoundingBox();
        }

        _Vec3<T> minPt = points[0];
        _Vec3<T> maxPt = points[0];

        for (size_t i = 1; i < points.size(); ++i)
        {
            minPt = minPt.Min(points[i]);
            maxPt = maxPt.Max(points[i]);
        }

        return _BoundingBox(minPt, maxPt);
    }

    template <std::floating_point T>
    _BoundingBox<T> _BoundingBox<T>::FromCenterAndExtents(const _Vec3<T>& center, const _Vec3<T>& extents) noexcept
    {
        return _BoundingBox(center - extents, center + extents);
    }

    template <std::floating_point T>
    void _BoundingBox<T>::GetCorners(std::array<_Vec3<T>, 8>& outCorners) const noexcept
    {
        outCorners[0] = _Vec3<T>(min.x, min.y, min.z);
        outCorners[1] = _Vec3<T>(max.x, min.y, min.z);
        outCorners[2] = _Vec3<T>(max.x, max.y, min.z);
        outCorners[3] = _Vec3<T>(min.x, max.y, min.z);
        outCorners[4] = _Vec3<T>(min.x, min.y, max.z);
        outCorners[5] = _Vec3<T>(max.x, min.y, max.z);
        outCorners[6] = _Vec3<T>(max.x, max.y, max.z);
        outCorners[7] = _Vec3<T>(min.x, max.y, max.z);
    }

    template <std::floating_point T>
    bool _BoundingBox<T>::Contains(const _Vec3<T>& point) const noexcept
    {
        return point.x >= min.x && point.x <= max.x
            && point.y >= min.y && point.y <= max.y
            && point.z >= min.z && point.z <= max.z;
    }

    template <std::floating_point T>
    bool _BoundingBox<T>::Contains(const _BoundingBox& other) const noexcept
    {
        return other.min.x >= min.x && other.max.x <= max.x
            && other.min.y >= min.y && other.max.y <= max.y
            && other.min.z >= min.z && other.max.z <= max.z;
    }

    template <std::floating_point T>
    bool _BoundingBox<T>::Intersects(const _BoundingBox& other) const noexcept
    {
        return !(other.min.x > max.x || other.max.x < min.x ||
                 other.min.y > max.y || other.max.y < min.y ||
                 other.min.z > max.z || other.max.z < min.z);
    }

    template <std::floating_point T>
    bool _BoundingBox<T>::Intersects(const _BoundingSphere<T>& sphere) const noexcept
    {
        const T closestX = Sandbox3D::Maths::Clamp(sphere.center.x, min.x, max.x);
        const T closestY = Sandbox3D::Maths::Clamp(sphere.center.y, min.y, max.y);
        const T closestZ = Sandbox3D::Maths::Clamp(sphere.center.z, min.z, max.z);

        const _Vec3<T> closestPoint(closestX, closestY, closestZ);
        return (sphere.center - closestPoint).LengthSquared() <= (sphere.radius * sphere.radius);
    }

    template <std::floating_point T>
    bool _BoundingBox<T>::Intersects(const _Plane<T>& plane) const noexcept
    {
        const _Vec3<T> center = GetCenter();
        const _Vec3<T> extents = GetExtents();

        const T projectedRadius = extents.x * std::abs(plane.normal.x)
                                + extents.y * std::abs(plane.normal.y)
                                + extents.z * std::abs(plane.normal.z);

        const T signedDistance = plane.DotCoordinate(center);
        return std::abs(signedDistance) <= projectedRadius;
    }

    template <std::floating_point T>
    _BoundingBox<T> _BoundingBox<T>::Transformed(const _Mat4x4<T>& transform) const noexcept
    {
        std::array<_Vec3<T>, 8> corners;
        GetCorners(corners);

        for (auto& corner : corners)
        {
            corner = transform.TransformPoint(corner);
        }

        return FromPoints(corners);
    }

    template <std::floating_point T>
    bool _BoundingBox<T>::operator==(const _BoundingBox& rhs) const noexcept
    {
        return (min == rhs.min) && (max == rhs.max);
    }

    // Explicit template instantiations for dual-tier precision
    template struct _BoundingBox<float>;
    template struct _BoundingBox<double>;
}

