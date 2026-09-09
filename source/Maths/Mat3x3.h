// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Vec3.h"

namespace Sandbox3D::Maths
{
    // 3x3 Matrix struct for 3D orientation, normal transformations, and scale
    // Stored in row-major memory order; vector transformations adhere to row-vector convention (v * M)
    template <std::floating_point T>
    struct _Mat3x3
    {
        // Elements stored row-major: m[row][column]
        T m[3][3]{
            { 1, 0, 0 },
            { 0, 1, 0 },
            { 0, 0, 1 }
        };

        constexpr _Mat3x3() noexcept = default;

        constexpr _Mat3x3(
            T m00, T m01, T m02,
            T m10, T m11, T m12,
            T m20, T m21, T m22
        ) noexcept : m{
            { m00, m01, m02 },
            { m10, m11, m12 },
            { m20, m21, m22 }
        } {}

        // Multi-precision conversion constructor
        template <std::floating_point U>
        constexpr explicit _Mat3x3(const _Mat3x3<U>& other) noexcept
            : m{
                { static_cast<T>(other.m[0][0]), static_cast<T>(other.m[0][1]), static_cast<T>(other.m[0][2]) },
                { static_cast<T>(other.m[1][0]), static_cast<T>(other.m[1][1]), static_cast<T>(other.m[1][2]) },
                { static_cast<T>(other.m[2][0]), static_cast<T>(other.m[2][1]), static_cast<T>(other.m[2][2]) }
            } {}

        // Algebraic properties and transformations
        [[nodiscard]] _Mat3x3 Transposed() const noexcept;
        void Transpose() noexcept;
        [[nodiscard]] T Determinant() const noexcept;
        [[nodiscard]] _Mat3x3 Inverted() const noexcept;
        bool Invert() noexcept;

        // Static factory rotation and scale transformations (Left-Handed orientation)
        [[nodiscard]] static constexpr _Mat3x3 Identity() noexcept { return _Mat3x3(); }
        [[nodiscard]] static constexpr _Mat3x3 Zero() noexcept { return _Mat3x3(0, 0, 0, 0, 0, 0, 0, 0, 0); }
        [[nodiscard]] static _Mat3x3 RotationX(T radians) noexcept;
        [[nodiscard]] static _Mat3x3 RotationY(T radians) noexcept;
        [[nodiscard]] static _Mat3x3 RotationZ(T radians) noexcept;
        [[nodiscard]] static _Mat3x3 RotationYawPitchRoll(T yaw, T pitch, T roll) noexcept;
        [[nodiscard]] static _Mat3x3 Scale(T sx, T sy, T sz) noexcept;
        [[nodiscard]] static _Mat3x3 Scale(const _Vec3<T>& scale) noexcept;

        // Vector and matrix operations
        [[nodiscard]] _Vec3<T> TransformVector(const _Vec3<T>& v) const noexcept;
        [[nodiscard]] _Mat3x3 operator*(const _Mat3x3& rhs) const noexcept;
        [[nodiscard]] _Vec3<T> operator*(const _Vec3<T>& rhs) const noexcept { return TransformVector(rhs); }

        bool operator==(const _Mat3x3& rhs) const noexcept;
        bool operator!=(const _Mat3x3& rhs) const noexcept { return !(*this == rhs); }
    };

    using Mat3x3  = _Mat3x3<float>;
    using Mat3x3D = _Mat3x3<double>;
}

