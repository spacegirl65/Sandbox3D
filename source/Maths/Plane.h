// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "MathsCommon.h"
#include "Vec3.h"
#include "Vec4.h"

#include <cmath>
#include <concepts>
#include <iostream>

namespace Sandbox3D::Maths
{
    enum class PlaneIntersectionType
    {
        Front,
        Back,
        Intersecting
    };

    // 3D Plane struct adhering to Cartesian equation: n.x * x + n.y * y + n.z * z + distance = 0
    template <std::floating_point T>
    struct _Plane
    {
        _Vec3<T> normal{ 0, 1, 0 };
        T        distance{ 0 };

        constexpr _Plane() noexcept = default;
        constexpr _Plane(T a, T b, T c, T d) noexcept : normal(a, b, c), distance(d) {}
        constexpr _Plane(const _Vec3<T>& normal, T distance) noexcept : normal(normal), distance(distance) {}
        _Plane(const _Vec3<T>& normal, const _Vec3<T>& pointOnPlane) noexcept;
        _Plane(const _Vec3<T>& p0, const _Vec3<T>& p1, const _Vec3<T>& p2) noexcept;

        template <std::floating_point U>
        constexpr explicit _Plane(const _Plane<U>& other) noexcept
            : normal(other.normal)
            , distance(static_cast<T>(other.distance))
        {}

        // Normalisation
        [[nodiscard]] _Plane Normalised() const noexcept;
        void Normalise() noexcept;

        // Plane evaluations
        [[nodiscard]] T Dot(const _Vec4<T>& v) const noexcept;
        [[nodiscard]] T DotCoordinate(const _Vec3<T>& point) const noexcept;
        [[nodiscard]] T DotNormal(const _Vec3<T>& direction) const noexcept;
        [[nodiscard]] T SignedDistance(const _Vec3<T>& point) const noexcept { return DotCoordinate(point); }

        [[nodiscard]] PlaneIntersectionType ClassifyPoint(const _Vec3<T>& point, T epsilon = DefaultEpsilon<T>) const noexcept;

        [[nodiscard]] bool operator==(const _Plane& rhs) const noexcept;
        [[nodiscard]] bool operator!=(const _Plane& rhs) const noexcept { return !(*this == rhs); }
    };

    template <std::floating_point T>
    std::ostream& operator<<(std::ostream& os, const _Plane<T>& p)
    {
        return os << "Plane[normal: " << p.normal << ", d: " << p.distance << "]";
    }

    using Plane  = _Plane<float>;
    using PlaneD = _Plane<double>;
}

