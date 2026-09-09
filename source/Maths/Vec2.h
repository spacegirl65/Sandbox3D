// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "MathsCommon.h"
#include <type_traits>

namespace Sandbox3D::Maths
{
    // Two-dimensional mathematical vector struct adhering to the Value Object design pattern
    template <typename T>
        requires std::is_arithmetic_v<T>
    struct _Vec2
    {
        T x{ static_cast<T>(0) };
        T y{ static_cast<T>(0) };

        constexpr _Vec2() noexcept = default;
        constexpr _Vec2(T inX, T inY) noexcept : x(inX), y(inY) {}
        constexpr explicit _Vec2(T scalar) noexcept : x(scalar), y(scalar) {}

        // Multi-precision conversion constructor
        template <typename U>
            requires std::is_arithmetic_v<U>
        constexpr explicit _Vec2(const _Vec2<U>& other) noexcept
            : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)) {}

        // Magnitude and normalisation
        [[nodiscard]] auto Length() const noexcept
        {
            if constexpr (std::is_floating_point_v<T>)
            {
                return std::sqrt(x * x + y * y);
            }
            else
            {
                return std::sqrt(static_cast<double>(x * x + y * y));
            }
        }

        [[nodiscard]] constexpr T LengthSquared() const noexcept
        {
            return x * x + y * y;
        }

        [[nodiscard]] _Vec2 Normalised() const noexcept
            requires std::is_floating_point_v<T>;

        void Normalise() noexcept
            requires std::is_floating_point_v<T>;

        // Geometric operations
        [[nodiscard]] constexpr T Dot(const _Vec2& other) const noexcept
        {
            return x * other.x + y * other.y;
        }

        // 2D perpendicular dot product (signed area / pseudo-cross product)
        [[nodiscard]] constexpr T Cross(const _Vec2& other) const noexcept
        {
            return x * other.y - y * other.x;
        }

        [[nodiscard]] auto Distance(const _Vec2& other) const noexcept
        {
            return (*this - other).Length();
        }

        [[nodiscard]] constexpr T DistanceSquared(const _Vec2& other) const noexcept
        {
            return (*this - other).LengthSquared();
        }

        [[nodiscard]] _Vec2 Reflect(const _Vec2& normal) const noexcept
            requires std::is_floating_point_v<T>;

        // Static factory directions
        [[nodiscard]] static constexpr _Vec2 Zero() noexcept { return _Vec2(0, 0); }
        [[nodiscard]] static constexpr _Vec2 One() noexcept { return _Vec2(1, 1); }
        [[nodiscard]] static constexpr _Vec2 UnitX() noexcept { return _Vec2(1, 0); }
        [[nodiscard]] static constexpr _Vec2 UnitY() noexcept { return _Vec2(0, 1); }

        // Operators
        constexpr _Vec2 operator+(const _Vec2& rhs) const noexcept { return _Vec2(x + rhs.x, y + rhs.y); }
        constexpr _Vec2 operator-(const _Vec2& rhs) const noexcept { return _Vec2(x - rhs.x, y - rhs.y); }
        constexpr _Vec2 operator*(T scalar) const noexcept { return _Vec2(x * scalar, y * scalar); }
        constexpr _Vec2 operator/(T scalar) const noexcept { return _Vec2(x / scalar, y / scalar); }
        constexpr _Vec2 operator-() const noexcept { return _Vec2(-x, -y); }

        constexpr _Vec2& operator+=(const _Vec2& rhs) noexcept { x += rhs.x; y += rhs.y; return *this; }
        constexpr _Vec2& operator-=(const _Vec2& rhs) noexcept { x -= rhs.x; y -= rhs.y; return *this; }
        constexpr _Vec2& operator*=(T scalar) noexcept { x *= scalar; y *= scalar; return *this; }
        constexpr _Vec2& operator/=(T scalar) noexcept { x /= scalar; y /= scalar; return *this; }

        [[nodiscard]] bool operator==(const _Vec2& rhs) const noexcept;
        [[nodiscard]] bool operator!=(const _Vec2& rhs) const noexcept { return !(*this == rhs); }
    };

    template <typename T>
    constexpr _Vec2<T> operator*(T scalar, const _Vec2<T>& vec) noexcept
    {
        return vec * scalar;
    }

    using Vec2  = _Vec2<float>;
    using Vec2D = _Vec2<double>;
    using Vec2I = _Vec2<int32_t>;
}

