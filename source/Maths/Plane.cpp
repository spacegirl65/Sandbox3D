// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Plane.h"

namespace Sandbox3D::Maths
{
    template <std::floating_point T>
    _Plane<T>::_Plane(const _Vec3<T>& normal, const _Vec3<T>& pointOnPlane) noexcept
        : normal(normal.Normalised())
        , distance(-normal.Normalised().Dot(pointOnPlane))
    {
    }

    template <std::floating_point T>
    _Plane<T>::_Plane(const _Vec3<T>& p0, const _Vec3<T>& p1, const _Vec3<T>& p2) noexcept
    {
        const _Vec3<T> edge1 = p1 - p0;
        const _Vec3<T> edge2 = p2 - p0;
        normal = edge1.Cross(edge2).Normalised();
        distance = -normal.Dot(p0);
    }

    template <std::floating_point T>
    _Plane<T> _Plane<T>::Normalised() const noexcept
    {
        const T len = normal.Length();
        if (len > DefaultEpsilon<T>)
        {
            const T invLen = static_cast<T>(1) / len;
            return _Plane(normal * invLen, distance * invLen);
        }
        return *this;
    }

    template <std::floating_point T>
    void _Plane<T>::Normalise() noexcept
    {
        *this = Normalised();
    }

    template <std::floating_point T>
    T _Plane<T>::Dot(const _Vec4<T>& v) const noexcept
    {
        return normal.x * v.x + normal.y * v.y + normal.z * v.z + distance * v.w;
    }

    template <std::floating_point T>
    T _Plane<T>::DotCoordinate(const _Vec3<T>& point) const noexcept
    {
        return normal.Dot(point) + distance;
    }

    template <std::floating_point T>
    T _Plane<T>::DotNormal(const _Vec3<T>& direction) const noexcept
    {
        return normal.Dot(direction);
    }

    template <std::floating_point T>
    PlaneIntersectionType _Plane<T>::ClassifyPoint(const _Vec3<T>& point, T epsilon) const noexcept
    {
        const T dist = DotCoordinate(point);
        if (dist > epsilon)
        {
            return PlaneIntersectionType::Front;
        }
        if (dist < -epsilon)
        {
            return PlaneIntersectionType::Back;
        }
        return PlaneIntersectionType::Intersecting;
    }

    template <std::floating_point T>
    bool _Plane<T>::operator==(const _Plane& rhs) const noexcept
    {
        return (normal == rhs.normal) && ApproximatelyEqual(distance, rhs.distance);
    }

    // Explicit template instantiations for dual-tier precision
    template struct _Plane<float>;
    template struct _Plane<double>;
}

