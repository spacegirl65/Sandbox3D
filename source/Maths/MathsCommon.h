// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>

namespace Sandbox3D::Maths
{
    // High-precision mathematical constants
    template <std::floating_point T>
    inline constexpr T Pi = static_cast<T>(3.141592653589793238462643383279502884);

    template <std::floating_point T>
    inline constexpr T TwoPi = static_cast<T>(6.283185307179586476925286766559005768);

    template <std::floating_point T>
    inline constexpr T HalfPi = static_cast<T>(1.570796326794896619231321691639751442);

    template <std::floating_point T>
    inline constexpr T DegToRad = Pi<T> / static_cast<T>(180.0);

    template <std::floating_point T>
    inline constexpr T RadToDeg = static_cast<T>(180.0) / Pi<T>;

    template <typename T>
    inline constexpr T DefaultEpsilon = static_cast<T>(1e-5);

    template <>
    inline constexpr double DefaultEpsilon<double> = 1e-10;

    // Angle conversion routines
    template <std::floating_point T>
    [[nodiscard]] constexpr T ToRadians(T degrees) noexcept
    {
        return degrees * DegToRad<T>;
    }

    template <std::floating_point T>
    [[nodiscard]] constexpr T ToDegrees(T radians) noexcept
    {
        return radians * RadToDeg<T>;
    }

    // Scalar numerical utility functions
    template <typename T>
    [[nodiscard]] constexpr bool ApproximatelyEqual(T a, T b, T epsilon = DefaultEpsilon<T>) noexcept
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            return std::abs(a - b) <= epsilon;
        }
        else
        {
            return a == b;
        }
    }

    template <typename T>
    [[nodiscard]] constexpr T Clamp(T value, T minVal, T maxVal) noexcept
    {
        return (value < minVal) ? minVal : ((value > maxVal) ? maxVal : value);
    }

    template <std::floating_point T>
    [[nodiscard]] constexpr T Lerp(T start, T end, T t) noexcept
    {
        return start + t * (end - start);
    }
}

