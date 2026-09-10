// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "MathsCommon.h"
#include "Vec3.h"
#include "Mat4x4.h"

#include <concepts>
#include <span>
#include <iostream>

namespace Sandbox3D::Maths
{
    template <std::floating_point T>
    struct _BoundingBox;

    template <std::floating_point T>
    struct _Plane;

    // Bounding Sphere for coarse broad-phase collision and visibility tests
    template <std::floating_point T>
    struct _BoundingSphere
    {
        _Vec3<T> center{ 0, 0, 0 };
        T        radius{ 0 };

        constexpr _BoundingSphere() noexcept = default;
        constexpr _BoundingSphere(const _Vec3<T>& center, T radius) noexcept : center(center), radius(radius) {}

        template <std::floating_point U>
        constexpr explicit _BoundingSphere(const _BoundingSphere<U>& other) noexcept
            : center(other.center)
            , radius(static_cast<T>(other.radius))
        {}

        // Factory constructor
        [[nodiscard]] static _BoundingSphere FromPoints(std::span<const _Vec3<T>> points) noexcept;

        // Spatial queries
        [[nodiscard]] bool Contains(const _Vec3<T>& point) const noexcept;
        [[nodiscard]] bool Intersects(const _BoundingSphere& other) const noexcept;
        [[nodiscard]] bool Intersects(const _BoundingBox<T>& box) const noexcept;
        [[nodiscard]] bool Intersects(const _Plane<T>& plane) const noexcept;

        // Affine transformation
        [[nodiscard]] _BoundingSphere Transformed(const _Mat4x4<T>& transform) const noexcept;

        [[nodiscard]] bool operator==(const _BoundingSphere& rhs) const noexcept;
        [[nodiscard]] bool operator!=(const _BoundingSphere& rhs) const noexcept { return !(*this == rhs); }
    };

    template <std::floating_point T>
    std::ostream& operator<<(std::ostream& os, const _BoundingSphere<T>& s)
    {
        return os << "Sphere[center: " << s.center << ", r: " << s.radius << "]";
    }

    using BoundingSphere  = _BoundingSphere<float>;
    using BoundingSphereD = _BoundingSphere<double>;
}

