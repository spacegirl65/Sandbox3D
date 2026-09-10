// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "MathsCommon.h"
#include "Vec3.h"

#include <concepts>
#include <iostream>

namespace Sandbox3D::Maths
{
    template <std::floating_point T>
    struct _Plane;

    template <std::floating_point T>
    struct _BoundingBox;

    template <std::floating_point T>
    struct _BoundingSphere;

    // 3D Ray for raycasting, mouse picking, and continuous collision detection
    template <std::floating_point T>
    struct _Ray
    {
        _Vec3<T> origin{ 0, 0, 0 };
        _Vec3<T> direction{ 0, 0, 1 };

        constexpr _Ray() noexcept = default;
        _Ray(const _Vec3<T>& origin, const _Vec3<T>& direction) noexcept;

        template <std::floating_point U>
        constexpr explicit _Ray(const _Ray<U>& other) noexcept
            : origin(other.origin)
            , direction(other.direction)
        {}

        [[nodiscard]] constexpr _Vec3<T> GetPoint(T distance) const noexcept
        {
            return origin + direction * distance;
        }

        // Intersection tests
        [[nodiscard]] bool Intersects(const _Plane<T>& plane, T& outDistance) const noexcept;
        [[nodiscard]] bool Intersects(const _BoundingSphere<T>& sphere, T& outDistance) const noexcept;
        [[nodiscard]] bool Intersects(const _BoundingBox<T>& box, T& outDistance) const noexcept;
        [[nodiscard]] bool IntersectsTriangle(
            const _Vec3<T>& v0,
            const _Vec3<T>& v1,
            const _Vec3<T>& v2,
            T& outDistance
        ) const noexcept;

        [[nodiscard]] bool operator==(const _Ray& rhs) const noexcept;
        [[nodiscard]] bool operator!=(const _Ray& rhs) const noexcept { return !(*this == rhs); }
    };

    template <std::floating_point T>
    std::ostream& operator<<(std::ostream& os, const _Ray<T>& r)
    {
        return os << "Ray[origin: " << r.origin << ", dir: " << r.direction << "]";
    }

    using Ray  = _Ray<float>;
    using RayD = _Ray<double>;
}

