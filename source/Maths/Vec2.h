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
        using LengthType = std::conditional_t<std::is_floating_point_v<T>, T, double>;

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

        [[nodiscard]] LengthType Magnitude() const noexcept;
        [[nodiscard]] T MagnitudeSquared() const noexcept;

        [[nodiscard]] _Vec2 Normalised() const noexcept
            requires std::is_floating_point_v<T>;

        void Normalise() noexcept
            requires std::is_floating_point_v<T>;

        // Geometric operations
        [[nodiscard]] constexpr T Dot(const _Vec2& other) const noexcept
        {
            return x * other.x + y * other.y;
        }

        [[nodiscard]] T DotProduct(const _Vec2& other) const noexcept;

        // 2D perpendicular dot product (signed area / pseudo-cross product)
        [[nodiscard]] constexpr T Cross(const _Vec2& other) const noexcept
        {
            return x * other.y - y * other.x;
        }

        [[nodiscard]] T CrossProduct(const _Vec2& other) const noexcept;

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

        // Component-wise and scalar Min / Max
        [[nodiscard]] _Vec2 Min(const _Vec2& other) const noexcept;
        [[nodiscard]] _Vec2 Min(T scalar) const noexcept;
        [[nodiscard]] T Min() const noexcept;
        [[nodiscard]] _Vec2 Max(const _Vec2& other) const noexcept;
        [[nodiscard]] _Vec2 Max(T scalar) const noexcept;
        [[nodiscard]] T Max() const noexcept;

        // Absolute value and negation
        [[nodiscard]] _Vec2 Abs() const noexcept;
        [[nodiscard]] _Vec2 Negate() const noexcept;
        [[nodiscard]] _Vec2 Negated() const noexcept;

        // Clamping operations
        [[nodiscard]] _Vec2 Clamp(const _Vec2& minVec, const _Vec2& maxVec) const noexcept;
        [[nodiscard]] _Vec2 Clamp(T minVal, T maxVal) const noexcept;
        [[nodiscard]] _Vec2 ClampMagnitude(T minLen, T maxLen) const noexcept
            requires std::is_floating_point_v<T>;

        // Geometric state queries
        [[nodiscard]] bool IsZero(T epsilon = DefaultEpsilon<T>) const noexcept;
        [[nodiscard]] bool IsUnit(T epsilon = DefaultEpsilon<T>) const noexcept
            requires std::is_floating_point_v<T>;
        [[nodiscard]] bool ArePerpendicular(const _Vec2& other, T epsilon = DefaultEpsilon<T>) const noexcept;

        // Interpolation operations
        [[nodiscard]] _Vec2 Lerp(const _Vec2& target, T t) const noexcept
            requires std::is_floating_point_v<T>;
        [[nodiscard]] _Vec2 Lerp(const _Vec2& target, const _Vec2& t) const noexcept
            requires std::is_floating_point_v<T>;
        [[nodiscard]] _Vec2 SmoothStep(const _Vec2& target, T t) const noexcept
            requires std::is_floating_point_v<T>;
        [[nodiscard]] _Vec2 SmoothStep(const _Vec2& edge0, const _Vec2& edge1) const noexcept
            requires std::is_floating_point_v<T>;
        [[nodiscard]] _Vec2 SmoothStep(T edge0, T edge1) const noexcept
            requires std::is_floating_point_v<T>;
        [[nodiscard]] _Vec2 SmootherStep(const _Vec2& target, T t) const noexcept
            requires std::is_floating_point_v<T>;
        [[nodiscard]] _Vec2 SmootherStep(const _Vec2& edge0, const _Vec2& edge1) const noexcept
            requires std::is_floating_point_v<T>;
        [[nodiscard]] _Vec2 SmootherStep(T edge0, T edge1) const noexcept
            requires std::is_floating_point_v<T>;
        [[nodiscard]] _Vec2 Nlerp(const _Vec2& target, T t) const noexcept
            requires std::is_floating_point_v<T>;
        [[nodiscard]] _Vec2 Slerp(const _Vec2& target, T t) const noexcept
            requires std::is_floating_point_v<T>;
        [[nodiscard]] _Vec2 MoveTowards(const _Vec2& target, T maxDistanceDelta) const noexcept
            requires std::is_floating_point_v<T>;

        // Lowercase alias forwarding methods matching plain-English API naming
        [[nodiscard]] LengthType magnitude() const noexcept;
        [[nodiscard]] T magnitude_squared() const noexcept;
        [[nodiscard]] _Vec2 min(const _Vec2& other) const noexcept;
        [[nodiscard]] _Vec2 min(T scalar) const noexcept;
        [[nodiscard]] T min() const noexcept;
        [[nodiscard]] _Vec2 max(const _Vec2& other) const noexcept;
        [[nodiscard]] _Vec2 max(T scalar) const noexcept;
        [[nodiscard]] T max() const noexcept;
        [[nodiscard]] T dot_product(const _Vec2& other) const noexcept;
        [[nodiscard]] T cross_product(const _Vec2& other) const noexcept;
        [[nodiscard]] _Vec2 normalised() const noexcept requires std::is_floating_point_v<T>;
        [[nodiscard]] _Vec2 abs() const noexcept;
        [[nodiscard]] _Vec2 negate() const noexcept;
        [[nodiscard]] _Vec2 clamp(const _Vec2& minVec, const _Vec2& maxVec) const noexcept;
        [[nodiscard]] _Vec2 clamp(T minVal, T maxVal) const noexcept;
        [[nodiscard]] bool iszero(T epsilon = DefaultEpsilon<T>) const noexcept;
        [[nodiscard]] bool isunit(T epsilon = DefaultEpsilon<T>) const noexcept requires std::is_floating_point_v<T>;
        [[nodiscard]] bool areperpendicular(const _Vec2& other, T epsilon = DefaultEpsilon<T>) const noexcept;
        [[nodiscard]] LengthType distance(const _Vec2& other) const noexcept;
        [[nodiscard]] T distancesquared(const _Vec2& other) const noexcept;
        [[nodiscard]] _Vec2 lerp(const _Vec2& target, T t) const noexcept requires std::is_floating_point_v<T>;
        [[nodiscard]] _Vec2 smoothstep(const _Vec2& target, T t) const noexcept requires std::is_floating_point_v<T>;
        [[nodiscard]] _Vec2 smootherstep(const _Vec2& target, T t) const noexcept requires std::is_floating_point_v<T>;
        [[nodiscard]] _Vec2 nlerp(const _Vec2& target, T t) const noexcept requires std::is_floating_point_v<T>;
        [[nodiscard]] _Vec2 slerp(const _Vec2& target, T t) const noexcept requires std::is_floating_point_v<T>;
        [[nodiscard]] _Vec2 move_towards(const _Vec2& target, T maxDistanceDelta) const noexcept requires std::is_floating_point_v<T>;

        // Static utility methods
        [[nodiscard]] static _Vec2 Min(const _Vec2& a, const _Vec2& b) noexcept;
        [[nodiscard]] static _Vec2 Max(const _Vec2& a, const _Vec2& b) noexcept;
        [[nodiscard]] static T Dot(const _Vec2& a, const _Vec2& b) noexcept;
        [[nodiscard]] static T DotProduct(const _Vec2& a, const _Vec2& b) noexcept;
        [[nodiscard]] static T Cross(const _Vec2& a, const _Vec2& b) noexcept;
        [[nodiscard]] static T CrossProduct(const _Vec2& a, const _Vec2& b) noexcept;
        [[nodiscard]] static LengthType Distance(const _Vec2& a, const _Vec2& b) noexcept;
        [[nodiscard]] static T DistanceSquared(const _Vec2& a, const _Vec2& b) noexcept;
        [[nodiscard]] static bool ArePerpendicular(const _Vec2& a, const _Vec2& b, T epsilon = DefaultEpsilon<T>) noexcept;
        [[nodiscard]] static _Vec2 Clamp(const _Vec2& v, const _Vec2& minVec, const _Vec2& maxVec) noexcept;
        [[nodiscard]] static _Vec2 Clamp(const _Vec2& v, T minVal, T maxVal) noexcept;
        [[nodiscard]] static _Vec2 Abs(const _Vec2& v) noexcept;
        [[nodiscard]] static _Vec2 Negate(const _Vec2& v) noexcept;
        [[nodiscard]] static _Vec2 Lerp(const _Vec2& a, const _Vec2& b, T t) noexcept requires std::is_floating_point_v<T>;
        [[nodiscard]] static _Vec2 SmoothStep(const _Vec2& a, const _Vec2& b, T t) noexcept requires std::is_floating_point_v<T>;
        [[nodiscard]] static _Vec2 SmootherStep(const _Vec2& a, const _Vec2& b, T t) noexcept requires std::is_floating_point_v<T>;
        [[nodiscard]] static _Vec2 Nlerp(const _Vec2& a, const _Vec2& b, T t) noexcept requires std::is_floating_point_v<T>;
        [[nodiscard]] static _Vec2 Slerp(const _Vec2& a, const _Vec2& b, T t) noexcept requires std::is_floating_point_v<T>;
        [[nodiscard]] static _Vec2 MoveTowards(const _Vec2& current, const _Vec2& target, T maxDistanceDelta) noexcept requires std::is_floating_point_v<T>;

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

