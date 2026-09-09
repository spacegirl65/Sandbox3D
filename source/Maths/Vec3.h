// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Vec2.h"

namespace Sandbox3D::Maths
{
    // Three-dimensional mathematical vector struct adhering to the Value Object design pattern
    // All directional orientations conform strictly to the Left-Handed (LH) coordinate standard
    template <std::floating_point T>
    struct _Vec3
    {
        T x{ static_cast<T>(0) };
        T y{ static_cast<T>(0) };
        T z{ static_cast<T>(0) };

        constexpr _Vec3() noexcept = default;
        constexpr _Vec3(T inX, T inY, T inZ) noexcept : x(inX), y(inY), z(inZ) {}
        constexpr explicit _Vec3(T scalar) noexcept : x(scalar), y(scalar), z(scalar) {}
        constexpr _Vec3(const _Vec2<T>& xy, T inZ) noexcept : x(xy.x), y(xy.y), z(inZ) {}

        // Multi-precision conversion constructor
        template <std::floating_point U>
        constexpr explicit _Vec3(const _Vec3<U>& other) noexcept
            : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)), z(static_cast<T>(other.z)) {}

        // Magnitude and normalisation
        [[nodiscard]] T Length() const noexcept;
        [[nodiscard]] constexpr T LengthSquared() const noexcept { return x * x + y * y + z * z; }
        [[nodiscard]] _Vec3 Normalised() const noexcept;
        void Normalise() noexcept;

        // Geometric operations
        [[nodiscard]] constexpr T Dot(const _Vec3& other) const noexcept
        {
            return x * other.x + y * other.y + z * other.z;
        }

        [[nodiscard]] constexpr _Vec3 Cross(const _Vec3& other) const noexcept
        {
            return _Vec3(
                y * other.z - z * other.y,
                z * other.x - x * other.z,
                x * other.y - y * other.x
            );
        }

        [[nodiscard]] T Distance(const _Vec3& other) const noexcept;
        [[nodiscard]] constexpr T DistanceSquared(const _Vec3& other) const noexcept;
        [[nodiscard]] _Vec3 Reflect(const _Vec3& normal) const noexcept;
        [[nodiscard]] _Vec3 Project(const _Vec3& onto) const noexcept;

        // Static factory directions for Left-Handed (LH) system
        [[nodiscard]] static constexpr _Vec3 Zero() noexcept { return _Vec3(0, 0, 0); }
        [[nodiscard]] static constexpr _Vec3 One() noexcept { return _Vec3(1, 1, 1); }
        [[nodiscard]] static constexpr _Vec3 UnitX() noexcept { return _Vec3(1, 0, 0); }
        [[nodiscard]] static constexpr _Vec3 UnitY() noexcept { return _Vec3(0, 1, 0); }
        [[nodiscard]] static constexpr _Vec3 UnitZ() noexcept { return _Vec3(0, 0, 1); }

        [[nodiscard]] static constexpr _Vec3 Up() noexcept { return _Vec3(0, 1, 0); }
        [[nodiscard]] static constexpr _Vec3 Down() noexcept { return _Vec3(0, -1, 0); }
        [[nodiscard]] static constexpr _Vec3 Right() noexcept { return _Vec3(1, 0, 0); }
        [[nodiscard]] static constexpr _Vec3 Left() noexcept { return _Vec3(-1, 0, 0); }
        [[nodiscard]] static constexpr _Vec3 Forward() noexcept { return _Vec3(0, 0, 1); }   // LH: +Z forward into screen
        [[nodiscard]] static constexpr _Vec3 Backward() noexcept { return _Vec3(0, 0, -1); } // LH: -Z backward

        // Operators
        constexpr _Vec3 operator+(const _Vec3& rhs) const noexcept { return _Vec3(x + rhs.x, y + rhs.y, z + rhs.z); }
        constexpr _Vec3 operator-(const _Vec3& rhs) const noexcept { return _Vec3(x - rhs.x, y - rhs.y, z - rhs.z); }
        constexpr _Vec3 operator*(T scalar) const noexcept { return _Vec3(x * scalar, y * scalar, z * scalar); }
        constexpr _Vec3 operator/(T scalar) const noexcept { return _Vec3(x / scalar, y / scalar, z / scalar); }
        constexpr _Vec3 operator-() const noexcept { return _Vec3(-x, -y, -z); }

        constexpr _Vec3& operator+=(const _Vec3& rhs) noexcept { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
        constexpr _Vec3& operator-=(const _Vec3& rhs) noexcept { x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; }
        constexpr _Vec3& operator*=(T scalar) noexcept { x *= scalar; y *= scalar; z *= scalar; return *this; }
        constexpr _Vec3& operator/=(T scalar) noexcept { x /= scalar; y /= scalar; z /= scalar; return *this; }

        [[nodiscard]] bool operator==(const _Vec3& rhs) const noexcept;
        [[nodiscard]] bool operator!=(const _Vec3& rhs) const noexcept { return !(*this == rhs); }
    };

    template <std::floating_point T>
    constexpr _Vec3<T> operator*(T scalar, const _Vec3<T>& vec) noexcept
    {
        return vec * scalar;
    }

    using Vec3  = _Vec3<float>;
    using Vec3D = _Vec3<double>;
}

