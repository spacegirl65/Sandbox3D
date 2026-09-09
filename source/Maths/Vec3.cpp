// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Vec3.h"

namespace Sandbox3D::Maths
{
    template <std::floating_point T>
    T _Vec3<T>::Length() const noexcept
    {
        return std::sqrt(x * x + y * y + z * z);
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::Normalised() const noexcept
    {
        const T len = Length();
        if (len > static_cast<T>(0))
        {
            const T invLen = static_cast<T>(1) / len;
            return _Vec3(x * invLen, y * invLen, z * invLen);
        }
        return Zero();
    }

    template <std::floating_point T>
    void _Vec3<T>::Normalise() noexcept
    {
        *this = Normalised();
    }

    template <std::floating_point T>
    T _Vec3<T>::Distance(const _Vec3& other) const noexcept
    {
        return (*this - other).Length();
    }

    template <std::floating_point T>
    constexpr T _Vec3<T>::DistanceSquared(const _Vec3& other) const noexcept
    {
        return (*this - other).LengthSquared();
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::Reflect(const _Vec3& normal) const noexcept
    {
        return *this - normal * (static_cast<T>(2) * Dot(normal));
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::Project(const _Vec3& onto) const noexcept
    {
        const T ontoLenSq = onto.LengthSquared();
        if (ontoLenSq > static_cast<T>(0))
        {
            return onto * (Dot(onto) / ontoLenSq);
        }
        return Zero();
    }

    template <std::floating_point T>
    bool _Vec3<T>::operator==(const _Vec3& rhs) const noexcept
    {
        return ApproximatelyEqual(x, rhs.x) && ApproximatelyEqual(y, rhs.y) && ApproximatelyEqual(z, rhs.z);
    }

    template struct _Vec3<float>;
    template struct _Vec3<double>;
}

