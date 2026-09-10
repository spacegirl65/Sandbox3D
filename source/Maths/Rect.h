// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "MathsCommon.h"
#include "Vec2.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <concepts>
#include <iostream>

namespace Sandbox3D::Maths
{
    // 2D Rectangle struct for viewport, scissor, and spatial layout operations
    template <std::floating_point T>
    struct _Rect
    {
        T x{ 0 };
        T y{ 0 };
        T width{ 0 };
        T height{ 0 };

        constexpr _Rect() noexcept = default;
        constexpr _Rect(T x, T y, T width, T height) noexcept : x(x), y(y), width(width), height(height) {}

        template <std::floating_point U>
        constexpr explicit _Rect(const _Rect<U>& other) noexcept
            : x(static_cast<T>(other.x))
            , y(static_cast<T>(other.y))
            , width(static_cast<T>(other.width))
            , height(static_cast<T>(other.height))
        {}

        // Bounds accessors (screen/window coordinate conventions: top-left origin)
        [[nodiscard]] constexpr T GetLeft() const noexcept { return x; }
        [[nodiscard]] constexpr T GetTop() const noexcept { return y; }
        [[nodiscard]] constexpr T GetRight() const noexcept { return x + width; }
        [[nodiscard]] constexpr T GetBottom() const noexcept { return y + height; }

        [[nodiscard]] constexpr _Vec2<T> GetCenter() const noexcept { return _Vec2<T>(x + width * static_cast<T>(0.5), y + height * static_cast<T>(0.5)); }
        [[nodiscard]] constexpr _Vec2<T> GetSize() const noexcept { return _Vec2<T>(width, height); }
        [[nodiscard]] constexpr T GetArea() const noexcept { return width * height; }

        // Containment and overlap tests
        [[nodiscard]] bool Contains(T px, T py) const noexcept;
        [[nodiscard]] bool Contains(const _Vec2<T>& point) const noexcept { return Contains(point.x, point.y); }
        [[nodiscard]] bool Intersects(const _Rect& other) const noexcept;

        // Conversion to API structures
        [[nodiscard]] RECT ToWin32Rect() const noexcept;
        [[nodiscard]] D3D12_RECT ToD3D12Rect() const noexcept;
        [[nodiscard]] D3D12_VIEWPORT ToD3D12Viewport(float minDepth = 0.0f, float maxDepth = 1.0f) const noexcept;

        [[nodiscard]] bool operator==(const _Rect& rhs) const noexcept;
        [[nodiscard]] bool operator!=(const _Rect& rhs) const noexcept { return !(*this == rhs); }
    };

    template <std::floating_point T>
    std::ostream& operator<<(std::ostream& os, const _Rect<T>& r)
    {
        return os << "[x: " << r.x << ", y: " << r.y << ", w: " << r.width << ", h: " << r.height << "]";
    }

    using Rect  = _Rect<float>;
    using RectD = _Rect<double>;
}

