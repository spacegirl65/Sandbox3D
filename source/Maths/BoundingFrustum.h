// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "MathsCommon.h"
#include "Plane.h"
#include "Mat4x4.h"

#include <concepts>
#include <array>
#include <iostream>

namespace Sandbox3D::Maths
{
    template <std::floating_point T>
    struct _BoundingBox;

    template <std::floating_point T>
    struct _BoundingSphere;

    // 6-plane viewing and culling frustum adhering to DirectX Left-Handed [0, 1] clip volume
    template <std::floating_point T>
    struct _BoundingFrustum
    {
        enum PlaneIndex
        {
            Near = 0,
            Far,
            Left,
            Right,
            Top,
            Bottom,
            PlaneCount = 6
        };

        std::array<_Plane<T>, PlaneCount> planes{};

        constexpr _BoundingFrustum() noexcept = default;
        explicit _BoundingFrustum(const _Mat4x4<T>& viewProjectionMatrix) noexcept;

        template <std::floating_point U>
        constexpr explicit _BoundingFrustum(const _BoundingFrustum<U>& other) noexcept
        {
            for (size_t i = 0; i < PlaneCount; ++i)
            {
                planes[i] = _Plane<T>(other.planes[i]);
            }
        }

        // Extracts frustum planes from a combined View-Projection matrix
        void ExtractFromMatrix(const _Mat4x4<T>& viewProjectionMatrix) noexcept;

        // Visibility & Culling queries
        [[nodiscard]] bool Contains(const _Vec3<T>& point) const noexcept;
        [[nodiscard]] bool Intersects(const _BoundingBox<T>& box) const noexcept;
        [[nodiscard]] bool Intersects(const _BoundingSphere<T>& sphere) const noexcept;

        [[nodiscard]] const _Plane<T>& GetPlane(PlaneIndex index) const noexcept { return planes[index]; }

        [[nodiscard]] bool operator==(const _BoundingFrustum& rhs) const noexcept;
        [[nodiscard]] bool operator!=(const _BoundingFrustum& rhs) const noexcept { return !(*this == rhs); }
    };

    template <std::floating_point T>
    std::ostream& operator<<(std::ostream& os, const _BoundingFrustum<T>& f)
    {
        return os << "BoundingFrustum[6 planes]";
    }

    using BoundingFrustum  = _BoundingFrustum<float>;
    using BoundingFrustumD = _BoundingFrustum<double>;
}

