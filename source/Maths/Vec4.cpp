// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Vec4.h"

namespace Sandbox3D::Maths
{
    template <std::floating_point T>
    T _Vec4<T>::Length() const noexcept
    {
        return std::sqrt(LengthSquared());
    }

    template <std::floating_point T>
    T _Vec4<T>::LengthSquared() const noexcept
    {
        return x * x + y * y + z * z + w * w;
    }

    template <std::floating_point T>
    _Vec4<T> _Vec4<T>::Normalised() const noexcept
    {
        const T len = Length();
        if (len > DefaultEpsilon<T>)
        {
            const T invLen = static_cast<T>(1) / len;
            return _Vec4(x * invLen, y * invLen, z * invLen, w * invLen);
        }
        return Zero();
    }

    template <std::floating_point T>
    void _Vec4<T>::Normalise() noexcept
    {
        *this = Normalised();
    }

    template <std::floating_point T>
    T _Vec4<T>::Dot(const _Vec4& other) const noexcept
    {
        return x * other.x + y * other.y + z * other.z + w * other.w;
    }

    template <std::floating_point T>
    _Vec4<T> _Vec4<T>::Abs() const noexcept
    {
        return _Vec4(std::abs(x), std::abs(y), std::abs(z), std::abs(w));
    }

    template <std::floating_point T>
    _Vec4<T> _Vec4<T>::Negated() const noexcept
    {
        return _Vec4(-x, -y, -z, -w);
    }

    template <std::floating_point T>
    _Vec4<T> _Vec4<T>::Clamp(const _Vec4& min, const _Vec4& max) const noexcept
    {
        return _Vec4(
            Sandbox3D::Maths::Clamp(x, min.x, max.x),
            Sandbox3D::Maths::Clamp(y, min.y, max.y),
            Sandbox3D::Maths::Clamp(z, min.z, max.z),
            Sandbox3D::Maths::Clamp(w, min.w, max.w)
        );
    }

    template <std::floating_point T>
    _Vec4<T> _Vec4<T>::clamp(T minVal, T maxVal) const noexcept
    {
        return _Vec4(
            Sandbox3D::Maths::Clamp(x, minVal, maxVal),
            Sandbox3D::Maths::Clamp(y, minVal, maxVal),
            Sandbox3D::Maths::Clamp(z, minVal, maxVal),
            Sandbox3D::Maths::Clamp(w, minVal, maxVal)
        );
    }

    template <std::floating_point T>
    bool _Vec4<T>::IsZero(T epsilon) const noexcept
    {
        return LengthSquared() <= (epsilon * epsilon);
    }

    template <std::floating_point T>
    bool _Vec4<T>::IsUnit(T epsilon) const noexcept
    {
        return std::abs(LengthSquared() - static_cast<T>(1)) <= epsilon;
    }

    template <std::floating_point T>
    T _Vec4<T>::Distance(const _Vec4& other) const noexcept
    {
        return (*this - other).Length();
    }

    template <std::floating_point T>
    T _Vec4<T>::DistanceSquared(const _Vec4& other) const noexcept
    {
        return (*this - other).LengthSquared();
    }

    template <std::floating_point T>
    _Vec4<T> _Vec4<T>::Lerp(const _Vec4& other, T t) const noexcept
    {
        return _Vec4(
            Sandbox3D::Maths::Lerp(x, other.x, t),
            Sandbox3D::Maths::Lerp(y, other.y, t),
            Sandbox3D::Maths::Lerp(z, other.z, t),
            Sandbox3D::Maths::Lerp(w, other.w, t)
        );
    }

    template <std::floating_point T>
    _Vec4<T> _Vec4<T>::SmoothStep(const _Vec4& other, T t) const noexcept
    {
        const T s = Sandbox3D::Maths::SmoothStep(static_cast<T>(0), static_cast<T>(1), t);
        return Lerp(other, s);
    }

    template <std::floating_point T>
    _Vec4<T> _Vec4<T>::SmootherStep(const _Vec4& other, T t) const noexcept
    {
        const T s = Sandbox3D::Maths::SmootherStep(static_cast<T>(0), static_cast<T>(1), t);
        return Lerp(other, s);
    }

    template <std::floating_point T>
    constexpr _Vec4<T> _Vec4<T>::operator/(T scalar) const noexcept
    {
        if (std::abs(scalar) > DefaultEpsilon<T>)
        {
            const T inv = static_cast<T>(1) / scalar;
            return _Vec4(x * inv, y * inv, z * inv, w * inv);
        }
        return Zero();
    }

    template <std::floating_point T>
    constexpr _Vec4<T>& _Vec4<T>::operator/=(T scalar) noexcept
    {
        *this = *this / scalar;
        return *this;
    }

    template <std::floating_point T>
    bool _Vec4<T>::operator==(const _Vec4& rhs) const noexcept
    {
        return ApproximatelyEqual(x, rhs.x)
            && ApproximatelyEqual(y, rhs.y)
            && ApproximatelyEqual(z, rhs.z)
            && ApproximatelyEqual(w, rhs.w);
    }

    // Explicit template instantiations for dual-tier precision
    template struct _Vec4<float>;
    template struct _Vec4<double>;
}

