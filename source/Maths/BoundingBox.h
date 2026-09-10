// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "MathsCommon.h"
#include "Vec3.h"
#include "Mat4x4.h"

#include <concepts>
#include <array>
#include <span>
#include <iostream>

namespace Sandbox3D::Maths
{
    template <std::floating_point T>
    struct _BoundingSphere;

    template <std::floating_point T>
    struct _Plane;

    // Axis-Aligned Bounding Box (AABB) for broad-phase spatial queries and culling
    template <std::floating_point T>
    struct _BoundingBox
    {
        _Vec3<T> min{ 0, 0, 0 };
        _Vec3<T> max{ 0, 0, 0 };

        constexpr _BoundingBox() noexcept = default;
        constexpr _BoundingBox(const _Vec3<T>& min, const _Vec3<T>& max) noexcept : min(min), max(max) {}

        template <std::floating_point U>
        constexpr explicit _BoundingBox(const _BoundingBox<U>& other) noexcept
            : min(other.min)
            , max(other.max)
        {}

        // Factory constructors
        [[nodiscard]] static _BoundingBox FromPoints(std::span<const _Vec3<T>> points) noexcept;
        [[nodiscard]] static _BoundingBox FromCenterAndExtents(const _Vec3<T>& center, const _Vec3<T>& extents) noexcept;

        // Accessors
        [[nodiscard]] constexpr _Vec3<T> GetCenter() const noexcept { return (min + max) * static_cast<T>(0.5); }
        [[nodiscard]] constexpr _Vec3<T> GetExtents() const noexcept { return (max - min) * static_cast<T>(0.5); }
        [[nodiscard]] constexpr _Vec3<T> GetSize() const noexcept { return max - min; }
        void GetCorners(std::array<_Vec3<T>, 8>& outCorners) const noexcept;

        // Spatial queries
        [[nodiscard]] bool Contains(const _Vec3<T>& point) const noexcept;
        [[nodiscard]] bool Contains(const _BoundingBox& other) const noexcept;
        [[nodiscard]] bool Intersects(const _BoundingBox& other) const noexcept;
        [[nodiscard]] bool Intersects(const _BoundingSphere<T>& sphere) const noexcept;
        [[nodiscard]] bool Intersects(const _Plane<T>& plane) const noexcept;

        // Affine transformation (recalculates enclosing AABB)
        [[nodiscard]] _BoundingBox Transformed(const _Mat4x4<T>& transform) const noexcept;

        [[nodiscard]] bool operator==(const _BoundingBox& rhs) const noexcept;
        [[nodiscard]] bool operator!=(const _BoundingBox& rhs) const noexcept { return !(*this == rhs); }
    };

    template <std::floating_point T>
    std::ostream& operator<<(std::ostream& os, const _BoundingBox<T>& b)
    {
        return os << "AABB[min: " << b.min << ", max: " << b.max << "]";
    }

    using BoundingBox  = _BoundingBox<float>;
    using BoundingBoxD = _BoundingBox<double>;
}

