// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Ray.h"
#include "Plane.h"
#include "BoundingSphere.h"
#include "BoundingBox.h"

#include <algorithm>

namespace Sandbox3D::Maths
{
    template <std::floating_point T>
    _Ray<T>::_Ray(const _Vec3<T>& origin, const _Vec3<T>& direction) noexcept
        : origin(origin)
        , direction(direction.Normalised())
    {
    }

    template <std::floating_point T>
    bool _Ray<T>::Intersects(const _Plane<T>& plane, T& outDistance) const noexcept
    {
        const T denom = plane.DotNormal(direction);
        if (std::abs(denom) > DefaultEpsilon<T>)
        {
            const T t = -(plane.DotCoordinate(origin)) / denom;
            if (t >= static_cast<T>(0))
            {
                outDistance = t;
                return true;
            }
        }
        return false;
    }

    template <std::floating_point T>
    bool _Ray<T>::Intersects(const _BoundingSphere<T>& sphere, T& outDistance) const noexcept
    {
        const _Vec3<T> m = origin - sphere.center;
        const T b = m.Dot(direction);
        const T c = m.Dot(m) - sphere.radius * sphere.radius;

        // If origin is outside sphere and ray is pointing away from sphere
        if (c > static_cast<T>(0) && b > static_cast<T>(0))
        {
            return false;
        }

        const T discriminant = b * b - c;
        if (discriminant < static_cast<T>(0))
        {
            return false;
        }

        T t = -b - std::sqrt(discriminant);
        if (t < static_cast<T>(0))
        {
            t = static_cast<T>(0); // Origin inside sphere
        }

        outDistance = t;
        return true;
    }

    template <std::floating_point T>
    bool _Ray<T>::Intersects(const _BoundingBox<T>& box, T& outDistance) const noexcept
    {
        T tMin = static_cast<T>(0);
        T tMax = std::numeric_limits<T>::max();

        // X axis slab
        if (std::abs(direction.x) < DefaultEpsilon<T>)
        {
            if (origin.x < box.min.x || origin.x > box.max.x)
            {
                return false;
            }
        }
        else
        {
            const T invD = static_cast<T>(1) / direction.x;
            T t1 = (box.min.x - origin.x) * invD;
            T t2 = (box.max.x - origin.x) * invD;
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return false;
        }

        // Y axis slab
        if (std::abs(direction.y) < DefaultEpsilon<T>)
        {
            if (origin.y < box.min.y || origin.y > box.max.y)
            {
                return false;
            }
        }
        else
        {
            const T invD = static_cast<T>(1) / direction.y;
            T t1 = (box.min.y - origin.y) * invD;
            T t2 = (box.max.y - origin.y) * invD;
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return false;
        }

        // Z axis slab
        if (std::abs(direction.z) < DefaultEpsilon<T>)
        {
            if (origin.z < box.min.z || origin.z > box.max.z)
            {
                return false;
            }
        }
        else
        {
            const T invD = static_cast<T>(1) / direction.z;
            T t1 = (box.min.z - origin.z) * invD;
            T t2 = (box.max.z - origin.z) * invD;
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return false;
        }

        outDistance = tMin;
        return true;
    }

    template <std::floating_point T>
    bool _Ray<T>::IntersectsTriangle(
        const _Vec3<T>& v0,
        const _Vec3<T>& v1,
        const _Vec3<T>& v2,
        T& outDistance
    ) const noexcept
    {
        // Möller–Trumbore ray-triangle intersection algorithm
        const _Vec3<T> edge1 = v1 - v0;
        const _Vec3<T> edge2 = v2 - v0;
        const _Vec3<T> h = direction.Cross(edge2);
        const T a = edge1.Dot(h);

        if (std::abs(a) < DefaultEpsilon<T>)
        {
            return false; // Ray is parallel to triangle
        }

        const T f = static_cast<T>(1) / a;
        const _Vec3<T> s = origin - v0;
        const T u = f * s.Dot(h);

        if (u < static_cast<T>(0) || u > static_cast<T>(1))
        {
            return false;
        }

        const _Vec3<T> q = s.Cross(edge1);
        const T v = f * direction.Dot(q);

        if (v < static_cast<T>(0) || u + v > static_cast<T>(1))
        {
            return false;
        }

        const T t = f * edge2.Dot(q);
        if (t > DefaultEpsilon<T>)
        {
            outDistance = t;
            return true;
        }

        return false;
    }

    template <std::floating_point T>
    bool _Ray<T>::operator==(const _Ray& rhs) const noexcept
    {
        return (origin == rhs.origin) && (direction == rhs.direction);
    }

    // Explicit template instantiations for dual-tier precision
    template struct _Ray<float>;
    template struct _Ray<double>;
}

