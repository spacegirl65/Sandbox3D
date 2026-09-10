// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "MathsCommon.h"
#include "Vec3.h"

#include <cmath>
#include <concepts>
#include <iostream>

namespace Sandbox3D::Maths
{
    // 4D Vector struct for homogeneous coordinates and RGBA colour representations
    template <std::floating_point T>
    struct _Vec4
    {
        T x{ 0 };
        T y{ 0 };
        T z{ 0 };
        T w{ 0 };

        constexpr _Vec4() noexcept = default;
        constexpr _Vec4(T x, T y, T z, T w) noexcept : x(x), y(y), z(z), w(w) {}
        constexpr _Vec4(const _Vec3<T>& v, T w) noexcept : x(v.x), y(v.y), z(v.z), w(w) {}
        constexpr _Vec4(const _Vec2<T>& v, T z, T w) noexcept : x(v.x), y(v.y), z(z), w(w) {}

        template <std::floating_point U>
        constexpr explicit _Vec4(const _Vec4<U>& other) noexcept
            : x(static_cast<T>(other.x))
            , y(static_cast<T>(other.y))
            , z(static_cast<T>(other.z))
            , w(static_cast<T>(other.w))
        {}

        // Colour component accessors
        [[nodiscard]] constexpr T r() const noexcept { return x; }
        [[nodiscard]] constexpr T g() const noexcept { return y; }
        [[nodiscard]] constexpr T b() const noexcept { return z; }
        [[nodiscard]] constexpr T a() const noexcept { return w; }
        constexpr T& r() noexcept { return x; }
        constexpr T& g() noexcept { return y; }
        constexpr T& b() noexcept { return z; }
        constexpr T& a() noexcept { return w; }

        // Conversion to Vec3 (drops w)
        [[nodiscard]] constexpr _Vec3<T> ToVec3() const noexcept { return _Vec3<T>(x, y, z); }

        // Vector magnitude and normalisation
        [[nodiscard]] T Length() const noexcept;
        [[nodiscard]] T LengthSquared() const noexcept;
        [[nodiscard]] T Magnitude() const noexcept { return Length(); }
        [[nodiscard]] T MagnitudeSquared() const noexcept { return LengthSquared(); }
        [[nodiscard]] T magnitude() const noexcept { return Length(); }
        [[nodiscard]] T magnitudesquared() const noexcept { return LengthSquared(); }

        [[nodiscard]] _Vec4 Normalised() const noexcept;
        [[nodiscard]] _Vec4 normalised() const noexcept { return Normalised(); }
        void Normalise() noexcept;

        // Algebraic and geometric operations
        [[nodiscard]] T Dot(const _Vec4& other) const noexcept;
        [[nodiscard]] T DotProduct(const _Vec4& other) const noexcept { return Dot(other); }
        [[nodiscard]] T dot_product(const _Vec4& other) const noexcept { return Dot(other); }

        [[nodiscard]] _Vec4 Abs() const noexcept;
        [[nodiscard]] _Vec4 abs() const noexcept { return Abs(); }

        [[nodiscard]] _Vec4 Negated() const noexcept;
        [[nodiscard]] _Vec4 Negate() const noexcept { return Negated(); }
        [[nodiscard]] _Vec4 negate() const noexcept { return Negated(); }

        [[nodiscard]] _Vec4 Clamp(const _Vec4& min, const _Vec4& max) const noexcept;
        [[nodiscard]] _Vec4 clamp(const _Vec4& min, const _Vec4& max) const noexcept { return Clamp(min, max); }
        [[nodiscard]] _Vec4 clamp(T minVal, T maxVal) const noexcept;

        [[nodiscard]] bool IsZero(T epsilon = DefaultEpsilon<T>) const noexcept;
        [[nodiscard]] bool iszero(T epsilon = DefaultEpsilon<T>) const noexcept { return IsZero(epsilon); }

        [[nodiscard]] bool IsUnit(T epsilon = DefaultEpsilon<T>) const noexcept;
        [[nodiscard]] bool isunit(T epsilon = DefaultEpsilon<T>) const noexcept { return IsUnit(epsilon); }

        [[nodiscard]] T Distance(const _Vec4& other) const noexcept;
        [[nodiscard]] T distance(const _Vec4& other) const noexcept { return Distance(other); }
        [[nodiscard]] T DistanceSquared(const _Vec4& other) const noexcept;
        [[nodiscard]] T distancesquared(const _Vec4& other) const noexcept { return DistanceSquared(other); }

        // Interpolations
        [[nodiscard]] _Vec4 Lerp(const _Vec4& other, T t) const noexcept;
        [[nodiscard]] _Vec4 lerp(const _Vec4& other, T t) const noexcept { return Lerp(other, t); }
        [[nodiscard]] _Vec4 SmoothStep(const _Vec4& other, T t) const noexcept;
        [[nodiscard]] _Vec4 smoothstep(const _Vec4& other, T t) const noexcept { return SmoothStep(other, t); }
        [[nodiscard]] _Vec4 SmootherStep(const _Vec4& other, T t) const noexcept;
        [[nodiscard]] _Vec4 smootherstep(const _Vec4& other, T t) const noexcept { return SmootherStep(other, t); }

        // Operators
        constexpr _Vec4 operator+(const _Vec4& rhs) const noexcept { return _Vec4(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w); }
        constexpr _Vec4 operator-(const _Vec4& rhs) const noexcept { return _Vec4(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w); }
        constexpr _Vec4 operator-() const noexcept { return _Vec4(-x, -y, -z, -w); }
        constexpr _Vec4 operator*(T scalar) const noexcept { return _Vec4(x * scalar, y * scalar, z * scalar, w * scalar); }
        constexpr _Vec4 operator/(T scalar) const noexcept;
        constexpr _Vec4 operator*(const _Vec4& rhs) const noexcept { return _Vec4(x * rhs.x, y * rhs.y, z * rhs.z, w * rhs.w); }

        constexpr _Vec4& operator+=(const _Vec4& rhs) noexcept { x += rhs.x; y += rhs.y; z += rhs.z; w += rhs.w; return *this; }
        constexpr _Vec4& operator-=(const _Vec4& rhs) noexcept { x -= rhs.x; y -= rhs.y; z -= rhs.z; w -= rhs.w; return *this; }
        constexpr _Vec4& operator*=(T scalar) noexcept { x *= scalar; y *= scalar; z *= scalar; w *= scalar; return *this; }
        constexpr _Vec4& operator/=(T scalar) noexcept;

        [[nodiscard]] bool operator==(const _Vec4& rhs) const noexcept;
        [[nodiscard]] bool operator!=(const _Vec4& rhs) const noexcept { return !(*this == rhs); }

        // Standard constant factories
        [[nodiscard]] static constexpr _Vec4 Zero() noexcept { return _Vec4(0, 0, 0, 0); }
        [[nodiscard]] static constexpr _Vec4 One() noexcept { return _Vec4(1, 1, 1, 1); }
        [[nodiscard]] static constexpr _Vec4 Red() noexcept { return _Vec4(1, 0, 0, 1); }
        [[nodiscard]] static constexpr _Vec4 Green() noexcept { return _Vec4(0, 1, 0, 1); }
        [[nodiscard]] static constexpr _Vec4 Blue() noexcept { return _Vec4(0, 0, 1, 1); }
        [[nodiscard]] static constexpr _Vec4 Yellow() noexcept { return _Vec4(1, 1, 0, 1); }
        [[nodiscard]] static constexpr _Vec4 Cyan() noexcept { return _Vec4(0, 1, 1, 1); }
        [[nodiscard]] static constexpr _Vec4 Magenta() noexcept { return _Vec4(1, 0, 1, 1); }
        [[nodiscard]] static constexpr _Vec4 White() noexcept { return _Vec4(1, 1, 1, 1); }
        [[nodiscard]] static constexpr _Vec4 Black() noexcept { return _Vec4(0, 0, 0, 1); }
    };

    // Free scalar operator
    template <std::floating_point T>
    constexpr _Vec4<T> operator*(T scalar, const _Vec4<T>& v) noexcept
    {
        return v * scalar;
    }

    template <std::floating_point T>
    std::ostream& operator<<(std::ostream& os, const _Vec4<T>& v)
    {
        return os << "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
    }

    using Vec4  = _Vec4<float>;
    using Vec4D = _Vec4<double>;
}

