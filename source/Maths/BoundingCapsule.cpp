// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "BoundingCapsule.h"
#include "BoundingBox.h"
#include "BoundingSphere.h"
#include "Plane.h"
#include "Ray.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Sandbox3D::Maths
{
    template <std::floating_point T>
    _BoundingCapsule<T> _BoundingCapsule<T>::FromVertical(
        const _Vec3<T>& center,
        T radius,
        T cylinderHeight
    ) noexcept
    {
        const T halfHeight = cylinderHeight * static_cast<T>(0.5);
        const _Vec3<T> p0(center.x, center.y - halfHeight, center.z);
        const _Vec3<T> p1(center.x, center.y + halfHeight, center.z);
        return _BoundingCapsule(p0, p1, radius);
    }

    template <std::floating_point T>
    _Vec3<T> _BoundingCapsule<T>::GetDirection() const noexcept
    {
        const _Vec3<T> dir = point1 - point0;
        const T len = dir.Length();
        return len > DefaultEpsilon<T> ? dir * (static_cast<T>(1) / len) : _Vec3<T>(0, 1, 0);
    }

    template <std::floating_point T>
    _Vec3<T> _BoundingCapsule<T>::ClosestPointOnSegment(const _Vec3<T>& point) const noexcept
    {
        const _Vec3<T> d = point1 - point0;
        const T lenSq = d.LengthSquared();
        if (lenSq <= DefaultEpsilon<T>)
        {
            return point0;
        }

        const T t = std::clamp((point - point0).Dot(d) / lenSq, static_cast<T>(0), static_cast<T>(1));
        return point0 + d * t;
    }

    template <std::floating_point T>
    _BoundingBox<T> _BoundingCapsule<T>::GetBoundingBox() const noexcept
    {
        const _Vec3<T> rVec(radius, radius, radius);
        const _Vec3<T> minPt = _Vec3<T>::Min(point0, point1) - rVec;
        const _Vec3<T> maxPt = _Vec3<T>::Max(point0, point1) + rVec;
        return _BoundingBox<T>(minPt, maxPt);
    }

    template <std::floating_point T>
    _BoundingSphere<T> _BoundingCapsule<T>::GetBoundingSphere() const noexcept
    {
        const _Vec3<T> center = GetCenter();
        const T halfSegment = GetSegmentLength() * static_cast<T>(0.5);
        return _BoundingSphere<T>(center, halfSegment + radius);
    }

    template <std::floating_point T>
    bool _BoundingCapsule<T>::Contains(const _Vec3<T>& point) const noexcept
    {
        const _Vec3<T> closest = ClosestPointOnSegment(point);
        return (point - closest).LengthSquared() <= (radius * radius + DefaultEpsilon<T>);
    }

    template <std::floating_point T>
    bool _BoundingCapsule<T>::Intersects(const _BoundingSphere<T>& sphere) const noexcept
    {
        const _Vec3<T> closest = ClosestPointOnSegment(sphere.center);
        const T totalRadius = radius + sphere.radius;
        return (sphere.center - closest).LengthSquared() <= (totalRadius * totalRadius);
    }

    template <std::floating_point T>
    bool _BoundingCapsule<T>::Intersects(const _BoundingCapsule& other) const noexcept
    {
        // Segment-to-segment distance algorithm
        const _Vec3<T> u = point1 - point0;
        const _Vec3<T> v = other.point1 - other.point0;
        const _Vec3<T> w = point0 - other.point0;

        const T a = u.Dot(u);
        const T b = u.Dot(v);
        const T c = v.Dot(v);
        const T d = u.Dot(w);
        const T e = v.Dot(w);
        const T denominator = a * c - b * b;

        T sN = static_cast<T>(0);
        T sD = denominator;
        T tN = static_cast<T>(0);
        T tD = denominator;

        if (denominator < DefaultEpsilon<T>)
        {
            sN = static_cast<T>(0);
            sD = static_cast<T>(1);
            tN = e;
            tD = c;
        }
        else
        {
            sN = b * e - c * d;
            tN = a * e - b * d;

            if (sN < static_cast<T>(0))
            {
                sN = static_cast<T>(0);
                tN = e;
                tD = c;
            }
            else if (sN > sD)
            {
                sN = sD;
                tN = e + b;
                tD = c;
            }
        }

        if (tN < static_cast<T>(0))
        {
            tN = static_cast<T>(0);
            if (-d < static_cast<T>(0))
            {
                sN = static_cast<T>(0);
            }
            else if (-d > a)
            {
                sN = sD;
            }
            else
            {
                sN = -d;
                sD = a;
            }
        }
        else if (tN > tD)
        {
            tN = tD;
            if ((-d + b) < static_cast<T>(0))
            {
                sN = static_cast<T>(0);
            }
            else if ((-d + b) > a)
            {
                sN = sD;
            }
            else
            {
                sN = -d + b;
                sD = a;
            }
        }

        const T sc = (std::abs(sN) < DefaultEpsilon<T>) ? static_cast<T>(0) : sN / sD;
        const T tc = (std::abs(tN) < DefaultEpsilon<T>) ? static_cast<T>(0) : tN / tD;

        const _Vec3<T> closestThis  = point0 + u * sc;
        const _Vec3<T> closestOther = other.point0 + v * tc;

        const T totalRadius = radius + other.radius;
        return (closestThis - closestOther).LengthSquared() <= (totalRadius * totalRadius);
    }

    template <std::floating_point T>
    bool _BoundingCapsule<T>::Intersects(const _BoundingBox<T>& box) const noexcept
    {
        // Broadphase rejection using axis-aligned bounding boxes
        if (!GetBoundingBox().Intersects(box))
        {
            return false;
        }

        // Iterative closest-point evaluation between line segment and box
        auto clampToBox = [&box](const _Vec3<T>& pt) noexcept -> _Vec3<T> {
            return _Vec3<T>(
                std::clamp(pt.x, box.min.x, box.max.x),
                std::clamp(pt.y, box.min.y, box.max.y),
                std::clamp(pt.z, box.min.z, box.max.z)
            );
        };

        _Vec3<T> q = clampToBox(GetCenter());
        _Vec3<T> p = ClosestPointOnSegment(q);
        q = clampToBox(p);
        p = ClosestPointOnSegment(q);
        q = clampToBox(p);

        return (p - q).LengthSquared() <= (radius * radius + DefaultEpsilon<T>);
    }

    template <std::floating_point T>
    bool _BoundingCapsule<T>::Intersects(const _Plane<T>& plane) const noexcept
    {
        const T d0 = plane.DotCoordinate(point0);
        const T d1 = plane.DotCoordinate(point1);

        if ((d0 * d1) <= static_cast<T>(0))
        {
            return true;
        }

        return std::min(std::abs(d0), std::abs(d1)) <= radius;
    }

    template <std::floating_point T>
    bool _BoundingCapsule<T>::Intersects(const _Ray<T>& ray, T& outDistance) const noexcept
    {
        constexpr T maxVal = std::numeric_limits<T>::infinity();
        T minHit = maxVal;

        // Test bottom hemisphere sphere
        T dist0 = static_cast<T>(0);
        if (ray.Intersects(_BoundingSphere<T>(point0, radius), dist0) && dist0 >= static_cast<T>(0))
        {
            minHit = std::min(minHit, dist0);
        }

        // Test top hemisphere sphere
        T dist1 = static_cast<T>(0);
        if (ray.Intersects(_BoundingSphere<T>(point1, radius), dist1) && dist1 >= static_cast<T>(0))
        {
            minHit = std::min(minHit, dist1);
        }

        // Test cylindrical body
        const _Vec3<T> axis = point1 - point0;
        const T axisLength = axis.Length();

        if (axisLength > DefaultEpsilon<T>)
        {
            const _Vec3<T> uAxis = axis * (static_cast<T>(1) / axisLength);
            const _Vec3<T> delta = ray.origin - point0;

            const _Vec3<T> rayDirPerp = ray.direction - uAxis * ray.direction.Dot(uAxis);
            const _Vec3<T> deltaPerp  = delta - uAxis * delta.Dot(uAxis);

            const T A = rayDirPerp.LengthSquared();
            const T B = static_cast<T>(2) * rayDirPerp.Dot(deltaPerp);
            const T C = deltaPerp.LengthSquared() - radius * radius;

            if (A > DefaultEpsilon<T>)
            {
                const T discriminant = B * B - static_cast<T>(4) * A * C;
                if (discriminant >= static_cast<T>(0))
                {
                    const T sqrtDisc = std::sqrt(discriminant);
                    const T inv2A = static_cast<T>(0.5) / A;
                    const T t0 = (-B - sqrtDisc) * inv2A;
                    const T t1 = (-B + sqrtDisc) * inv2A;

                    for (const T t : { t0, t1 })
                    {
                        if (t >= static_cast<T>(0) && t < minHit)
                        {
                            const _Vec3<T> hitPoint = ray.origin + ray.direction * t;
                            const T heightAlongAxis = (hitPoint - point0).Dot(uAxis);
                            if (heightAlongAxis >= static_cast<T>(0) && heightAlongAxis <= axisLength)
                            {
                                minHit = t;
                            }
                        }
                    }
                }
            }
        }

        if (minHit < maxVal)
        {
            outDistance = minHit;
            return true;
        }

        return false;
    }

    template <std::floating_point T>
    _BoundingCapsule<T> _BoundingCapsule<T>::Transformed(const _Mat4x4<T>& transform) const noexcept
    {
        const _Vec3<T> newP0 = transform.TransformPoint(point0);
        const _Vec3<T> newP1 = transform.TransformPoint(point1);

        const T scaleX = _Vec3<T>(transform.m[0][0], transform.m[0][1], transform.m[0][2]).Length();
        const T scaleY = _Vec3<T>(transform.m[1][0], transform.m[1][1], transform.m[1][2]).Length();
        const T scaleZ = _Vec3<T>(transform.m[2][0], transform.m[2][1], transform.m[2][2]).Length();

        const T maxScale = std::max({ scaleX, scaleY, scaleZ });
        return _BoundingCapsule(newP0, newP1, radius * maxScale);
    }

    template <std::floating_point T>
    bool _BoundingCapsule<T>::operator==(const _BoundingCapsule& rhs) const noexcept
    {
        return (point0 == rhs.point0) && (point1 == rhs.point1) && ApproximatelyEqual(radius, rhs.radius);
    }

    // Explicit template instantiations for dual-tier precision
    template struct _BoundingCapsule<float>;
    template struct _BoundingCapsule<double>;
}
