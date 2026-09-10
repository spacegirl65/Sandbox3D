// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Rect.h"

namespace Sandbox3D::Maths
{
    template <std::floating_point T>
    bool _Rect<T>::Contains(T px, T py) const noexcept
    {
        return px >= x && px <= (x + width) && py >= y && py <= (y + height);
    }

    template <std::floating_point T>
    bool _Rect<T>::Intersects(const _Rect& other) const noexcept
    {
        return !(other.GetLeft() > GetRight() ||
                 other.GetRight() < GetLeft() ||
                 other.GetTop() > GetBottom() ||
                 other.GetBottom() < GetTop());
    }

    template <std::floating_point T>
    RECT _Rect<T>::ToWin32Rect() const noexcept
    {
        RECT r;
        r.left   = static_cast<LONG>(std::floor(x));
        r.top    = static_cast<LONG>(std::floor(y));
        r.right  = static_cast<LONG>(std::ceil(x + width));
        r.bottom = static_cast<LONG>(std::ceil(y + height));
        return r;
    }

    template <std::floating_point T>
    D3D12_RECT _Rect<T>::ToD3D12Rect() const noexcept
    {
        D3D12_RECT r;
        r.left   = static_cast<LONG>(std::floor(x));
        r.top    = static_cast<LONG>(std::floor(y));
        r.right  = static_cast<LONG>(std::ceil(x + width));
        r.bottom = static_cast<LONG>(std::ceil(y + height));
        return r;
    }

    template <std::floating_point T>
    D3D12_VIEWPORT _Rect<T>::ToD3D12Viewport(float minDepth, float maxDepth) const noexcept
    {
        D3D12_VIEWPORT vp;
        vp.TopLeftX = static_cast<FLOAT>(x);
        vp.TopLeftY = static_cast<FLOAT>(y);
        vp.Width    = static_cast<FLOAT>(width);
        vp.Height   = static_cast<FLOAT>(height);
        vp.MinDepth = minDepth;
        vp.MaxDepth = maxDepth;
        return vp;
    }

    template <std::floating_point T>
    bool _Rect<T>::operator==(const _Rect& rhs) const noexcept
    {
        return ApproximatelyEqual(x, rhs.x)
            && ApproximatelyEqual(y, rhs.y)
            && ApproximatelyEqual(width, rhs.width)
            && ApproximatelyEqual(height, rhs.height);
    }

    // Explicit template instantiations for dual-tier precision
    template struct _Rect<float>;
    template struct _Rect<double>;
}

