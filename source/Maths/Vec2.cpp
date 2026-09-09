// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Vec2.h"

namespace Sandbox3D::Maths
{
    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::Normalised() const noexcept
        requires std::is_floating_point_v<T>
    {
        const T len = Length();
        if (len > static_cast<T>(0))
        {
            const T invLen = static_cast<T>(1) / len;
            return _Vec2(x * invLen, y * invLen);
        }
        return Zero();
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    void _Vec2<T>::Normalise() noexcept
        requires std::is_floating_point_v<T>
    {
        *this = Normalised();
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::Reflect(const _Vec2& normal) const noexcept
        requires std::is_floating_point_v<T>
    {
        return *this - normal * (static_cast<T>(2) * Dot(normal));
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    bool _Vec2<T>::operator==(const _Vec2& rhs) const noexcept
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            return ApproximatelyEqual(x, rhs.x) && ApproximatelyEqual(y, rhs.y);
        }
        else
        {
            return x == rhs.x && y == rhs.y;
        }
    }

    template struct _Vec2<float>;
    template struct _Vec2<double>;
    template struct _Vec2<int32_t>;
}

