// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "BoundingSphere.h"
#include "BoundingBox.h"
#include "Plane.h"

#include <algorithm>

namespace Sandbox3D::Maths
{
    template <std::floating_point T>
    _BoundingSphere<T> _BoundingSphere<T>::FromPoints(std::span<const _Vec3<T>> points) noexcept
    {
        if (points.empty())
        {
            return _BoundingSphere();
        }

        const _BoundingBox<T> aabb = _BoundingBox<T>::FromPoints(points);
        const _Vec3<T> center = aabb.GetCenter();

        T maxDistSq = 0;
        for (const auto& pt : points)
        {
            const T distSq = (pt - center).LengthSquared();
            if (distSq > maxDistSq)
            {
                maxDistSq = distSq;
            }
        }

        return _BoundingSphere(center, std::sqrt(maxDistSq));
    }

    template <std::floating_point T>
    bool _BoundingSphere<T>::Contains(const _Vec3<T>& point) const noexcept
    {
        return (point - center).LengthSquared() <= (radius * radius + DefaultEpsilon<T>);
    }

    template <std::floating_point T>
    bool _BoundingSphere<T>::Intersects(const _BoundingSphere& other) const noexcept
    {
        const T totalRadius = radius + other.radius;
        return (center - other.center).LengthSquared() <= (totalRadius * totalRadius);
    }

    template <std::floating_point T>
    bool _BoundingSphere<T>::Intersects(const _BoundingBox<T>& box) const noexcept
    {
        return box.Intersects(*this);
    }

    template <std::floating_point T>
    bool _BoundingSphere<T>::Intersects(const _Plane<T>& plane) const noexcept
    {
        return std::abs(plane.DotCoordinate(center)) <= radius;
    }

    template <std::floating_point T>
    _BoundingSphere<T> _BoundingSphere<T>::Transformed(const _Mat4x4<T>& transform) const noexcept
    {
        const _Vec3<T> newCenter = transform.TransformPoint(center);

        const T scaleX = _Vec3<T>(transform.m[0][0], transform.m[0][1], transform.m[0][2]).Length();
        const T scaleY = _Vec3<T>(transform.m[1][0], transform.m[1][1], transform.m[1][2]).Length();
        const T scaleZ = _Vec3<T>(transform.m[2][0], transform.m[2][1], transform.m[2][2]).Length();

        const T maxScale = std::max({ scaleX, scaleY, scaleZ });
        return _BoundingSphere(newCenter, radius * maxScale);
    }

    template <std::floating_point T>
    bool _BoundingSphere<T>::operator==(const _BoundingSphere& rhs) const noexcept
    {
        return (center == rhs.center) && ApproximatelyEqual(radius, rhs.radius);
    }

    // Explicit template instantiations for dual-tier precision
    template struct _BoundingSphere<float>;
    template struct _BoundingSphere<double>;
}

