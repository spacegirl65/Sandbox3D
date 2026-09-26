// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "MathsCommon.h"
#include "Vec3.h"
#include "Mat4x4.h"

#include <concepts>
#include <iostream>

namespace Sandbox3D::Maths
{
    template <std::floating_point T>
    struct _BoundingBox;

    template <std::floating_point T>
    struct _BoundingSphere;

    template <std::floating_point T>
    struct _Plane;

    template <std::floating_point T>
    struct _Ray;

    // Bounding Capsule defined by a line segment between point0 and point1 with radial expansion
    template <std::floating_point T>
    struct _BoundingCapsule
    {
        _Vec3<T> point0{ 0, 0, 0 };
        _Vec3<T> point1{ 0, 1, 0 };
        T        radius{ static_cast<T>(0.5) };

        constexpr _BoundingCapsule() noexcept = default;
        constexpr _BoundingCapsule(const _Vec3<T>& p0, const _Vec3<T>& p1, T radius) noexcept
            : point0(p0), point1(p1), radius(radius) {}

        template <std::floating_point U>
        constexpr explicit _BoundingCapsule(const _BoundingCapsule<U>& other) noexcept
            : point0(other.point0)
            , point1(other.point1)
            , radius(static_cast<T>(other.radius))
        {}

        // Factory constructor for vertical capsules aligned along the Y-axis
        [[nodiscard]] static _BoundingCapsule FromVertical(
            const _Vec3<T>& center,
            T radius,
            T cylinderHeight
        ) noexcept;

        // Accessors & metrics
        [[nodiscard]] constexpr _Vec3<T> GetCenter() const noexcept { return (point0 + point1) * static_cast<T>(0.5); }
        [[nodiscard]] _Vec3<T> GetDirection() const noexcept;
        [[nodiscard]] T GetSegmentLength() const noexcept { return (point1 - point0).Length(); }
        [[nodiscard]] T GetTotalHeight() const noexcept { return GetSegmentLength() + radius * static_cast<T>(2); }
        [[nodiscard]] _Vec3<T> ClosestPointOnSegment(const _Vec3<T>& point) const noexcept;

        // Bounding volume conversions
        [[nodiscard]] _BoundingBox<T> GetBoundingBox() const noexcept;
        [[nodiscard]] _BoundingSphere<T> GetBoundingSphere() const noexcept;

        // Spatial containment & intersection tests
        [[nodiscard]] bool Contains(const _Vec3<T>& point) const noexcept;
        [[nodiscard]] bool Intersects(const _BoundingSphere<T>& sphere) const noexcept;
        [[nodiscard]] bool Intersects(const _BoundingCapsule& other) const noexcept;
        [[nodiscard]] bool Intersects(const _BoundingBox<T>& box) const noexcept;
        [[nodiscard]] bool Intersects(const _Plane<T>& plane) const noexcept;
        [[nodiscard]] bool Intersects(const _Ray<T>& ray, T& outDistance) const noexcept;

        // Affine transformation
        [[nodiscard]] _BoundingCapsule Transformed(const _Mat4x4<T>& transform) const noexcept;

        [[nodiscard]] bool operator==(const _BoundingCapsule& rhs) const noexcept;
        [[nodiscard]] bool operator!=(const _BoundingCapsule& rhs) const noexcept { return !(*this == rhs); }
    };

    template <std::floating_point T>
    std::ostream& operator<<(std::ostream& os, const _BoundingCapsule<T>& c)
    {
        return os << "Capsule[p0: " << c.point0 << ", p1: " << c.point1 << ", r: " << c.radius << "]";
    }

    using BoundingCapsule  = _BoundingCapsule<float>;
    using BoundingCapsuleD = _BoundingCapsule<double>;
}

