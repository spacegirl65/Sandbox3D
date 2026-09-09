// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Vec3.h"

#include <algorithm>
#include <cmath>

namespace Sandbox3D::Maths
{
    template <std::floating_point T>
    T _Vec3<T>::Length() const noexcept
    {
        return std::sqrt(x * x + y * y + z * z);
    }

    template <std::floating_point T>
    T _Vec3<T>::Magnitude() const noexcept
    {
        return Length();
    }

    template <std::floating_point T>
    T _Vec3<T>::MagnitudeSquared() const noexcept
    {
        return LengthSquared();
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
    T _Vec3<T>::DotProduct(const _Vec3& other) const noexcept
    {
        return Dot(other);
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::CrossProduct(const _Vec3& other) const noexcept
    {
        return Cross(other);
    }

    template <std::floating_point T>
    T _Vec3<T>::Distance(const _Vec3& other) const noexcept
    {
        return (*this - other).Length();
    }

    template <std::floating_point T>
    T _Vec3<T>::DistanceSquared(const _Vec3& other) const noexcept
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
    _Vec3<T> _Vec3<T>::Min(const _Vec3& other) const noexcept
    {
        return _Vec3(
            (x < other.x) ? x : other.x,
            (y < other.y) ? y : other.y,
            (z < other.z) ? z : other.z
        );
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::Min(T scalar) const noexcept
    {
        return _Vec3(
            (x < scalar) ? x : scalar,
            (y < scalar) ? y : scalar,
            (z < scalar) ? z : scalar
        );
    }

    template <std::floating_point T>
    T _Vec3<T>::Min() const noexcept
    {
        return (x < y) ? ((x < z) ? x : z) : ((y < z) ? y : z);
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::Max(const _Vec3& other) const noexcept
    {
        return _Vec3(
            (x > other.x) ? x : other.x,
            (y > other.y) ? y : other.y,
            (z > other.z) ? z : other.z
        );
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::Max(T scalar) const noexcept
    {
        return _Vec3(
            (x > scalar) ? x : scalar,
            (y > scalar) ? y : scalar,
            (z > scalar) ? z : scalar
        );
    }

    template <std::floating_point T>
    T _Vec3<T>::Max() const noexcept
    {
        return (x > y) ? ((x > z) ? x : z) : ((y > z) ? y : z);
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::Abs() const noexcept
    {
        return _Vec3(
            (x < static_cast<T>(0)) ? -x : x,
            (y < static_cast<T>(0)) ? -y : y,
            (z < static_cast<T>(0)) ? -z : z
        );
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::Negate() const noexcept
    {
        return _Vec3(-x, -y, -z);
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::Negated() const noexcept
    {
        return Negate();
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::Clamp(const _Vec3& minVec, const _Vec3& maxVec) const noexcept
    {
        return _Vec3(
            Sandbox3D::Maths::Clamp(x, minVec.x, maxVec.x),
            Sandbox3D::Maths::Clamp(y, minVec.y, maxVec.y),
            Sandbox3D::Maths::Clamp(z, minVec.z, maxVec.z)
        );
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::Clamp(T minVal, T maxVal) const noexcept
    {
        return _Vec3(
            Sandbox3D::Maths::Clamp(x, minVal, maxVal),
            Sandbox3D::Maths::Clamp(y, minVal, maxVal),
            Sandbox3D::Maths::Clamp(z, minVal, maxVal)
        );
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::ClampMagnitude(T minLen, T maxLen) const noexcept
    {
        const T lenSq = LengthSquared();
        if (lenSq <= static_cast<T>(0))
        {
            return Zero();
        }
        const T len = std::sqrt(lenSq);
        const T clampedLen = Sandbox3D::Maths::Clamp(len, minLen, maxLen);
        return *this * (clampedLen / len);
    }

    template <std::floating_point T>
    bool _Vec3<T>::IsZero(T epsilon) const noexcept
    {
        return ApproximatelyEqual(x, static_cast<T>(0), epsilon) &&
               ApproximatelyEqual(y, static_cast<T>(0), epsilon) &&
               ApproximatelyEqual(z, static_cast<T>(0), epsilon);
    }

    template <std::floating_point T>
    bool _Vec3<T>::IsUnit(T epsilon) const noexcept
    {
        return ApproximatelyEqual(LengthSquared(), static_cast<T>(1), epsilon);
    }

    template <std::floating_point T>
    bool _Vec3<T>::ArePerpendicular(const _Vec3& other, T epsilon) const noexcept
    {
        const T lenSqProd = LengthSquared() * other.LengthSquared();
        if (lenSqProd <= static_cast<T>(0))
        {
            return false;
        }
        const T d = Dot(other);
        return (d * d) <= (epsilon * epsilon * lenSqProd);
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::Lerp(const _Vec3& target, T t) const noexcept
    {
        return _Vec3(
            Sandbox3D::Maths::Lerp(x, target.x, t),
            Sandbox3D::Maths::Lerp(y, target.y, t),
            Sandbox3D::Maths::Lerp(z, target.z, t)
        );
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::Lerp(const _Vec3& target, const _Vec3& t) const noexcept
    {
        return _Vec3(
            Sandbox3D::Maths::Lerp(x, target.x, t.x),
            Sandbox3D::Maths::Lerp(y, target.y, t.y),
            Sandbox3D::Maths::Lerp(z, target.z, t.z)
        );
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::SmoothStep(const _Vec3& target, T t) const noexcept
    {
        const T clampedT = Sandbox3D::Maths::Clamp(t, static_cast<T>(0), static_cast<T>(1));
        const T factor = clampedT * clampedT * (static_cast<T>(3) - static_cast<T>(2) * clampedT);
        return Lerp(target, factor);
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::SmoothStep(const _Vec3& edge0, const _Vec3& edge1) const noexcept
    {
        return _Vec3(
            Sandbox3D::Maths::SmoothStep(edge0.x, edge1.x, x),
            Sandbox3D::Maths::SmoothStep(edge0.y, edge1.y, y),
            Sandbox3D::Maths::SmoothStep(edge0.z, edge1.z, z)
        );
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::SmoothStep(T edge0, T edge1) const noexcept
    {
        return _Vec3(
            Sandbox3D::Maths::SmoothStep(edge0, edge1, x),
            Sandbox3D::Maths::SmoothStep(edge0, edge1, y),
            Sandbox3D::Maths::SmoothStep(edge0, edge1, z)
        );
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::SmootherStep(const _Vec3& target, T t) const noexcept
    {
        const T clampedT = Sandbox3D::Maths::Clamp(t, static_cast<T>(0), static_cast<T>(1));
        const T factor = clampedT * clampedT * clampedT * (clampedT * (clampedT * static_cast<T>(6) - static_cast<T>(15)) + static_cast<T>(10));
        return Lerp(target, factor);
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::SmootherStep(const _Vec3& edge0, const _Vec3& edge1) const noexcept
    {
        return _Vec3(
            Sandbox3D::Maths::SmootherStep(edge0.x, edge1.x, x),
            Sandbox3D::Maths::SmootherStep(edge0.y, edge1.y, y),
            Sandbox3D::Maths::SmootherStep(edge0.z, edge1.z, z)
        );
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::SmootherStep(T edge0, T edge1) const noexcept
    {
        return _Vec3(
            Sandbox3D::Maths::SmootherStep(edge0, edge1, x),
            Sandbox3D::Maths::SmootherStep(edge0, edge1, y),
            Sandbox3D::Maths::SmootherStep(edge0, edge1, z)
        );
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::Nlerp(const _Vec3& target, T t) const noexcept
    {
        return Lerp(target, t).Normalised();
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::Slerp(const _Vec3& target, T t) const noexcept
    {
        const T lenA = Length();
        const T lenB = target.Length();
        if (lenA <= static_cast<T>(0) || lenB <= static_cast<T>(0))
        {
            return Lerp(target, t);
        }

        const _Vec3 uA = *this / lenA;
        const _Vec3 uB = target / lenB;

        T cosOmega = uA.Dot(uB);
        cosOmega = Sandbox3D::Maths::Clamp(cosOmega, static_cast<T>(-1), static_cast<T>(1));

        if (cosOmega > static_cast<T>(1) - DefaultEpsilon<T>)
        {
            return Lerp(target, t);
        }

        const T interpolatedLength = Sandbox3D::Maths::Lerp(lenA, lenB, t);

        if (cosOmega < -(static_cast<T>(1) - DefaultEpsilon<T>))
        {
            const _Vec3 orthoAxis = (std::abs(uA.x) < static_cast<T>(0.9)) ? _Vec3::UnitX() : _Vec3::UnitY();
            const _Vec3 perp = uA.Cross(orthoAxis).Normalised();
            const T angle = Pi<T> * t;
            const _Vec3 uResult = uA * std::cos(angle) + perp * std::sin(angle);
            return uResult * interpolatedLength;
        }

        const T omega = std::acos(cosOmega);
        const T sinOmega = std::sin(omega);
        const T scaleA = std::sin((static_cast<T>(1) - t) * omega) / sinOmega;
        const T scaleB = std::sin(t * omega) / sinOmega;

        const _Vec3 uResult = uA * scaleA + uB * scaleB;
        return uResult * interpolatedLength;
    }

    template <std::floating_point T>
    _Vec3<T> _Vec3<T>::MoveTowards(const _Vec3& target, T maxDistanceDelta) const noexcept
    {
        const _Vec3 delta = target - *this;
        const T distSq = delta.LengthSquared();
        if (distSq <= maxDistanceDelta * maxDistanceDelta || distSq <= static_cast<T>(0))
        {
            return target;
        }
        const T dist = std::sqrt(distSq);
        return *this + delta * (maxDistanceDelta / dist);
    }

    // Lowercase aliases
    template <std::floating_point T> T _Vec3<T>::magnitude() const noexcept { return Magnitude(); }
    template <std::floating_point T> T _Vec3<T>::magnitude_squared() const noexcept { return MagnitudeSquared(); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::min(const _Vec3& other) const noexcept { return Min(other); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::min(T scalar) const noexcept { return Min(scalar); }
    template <std::floating_point T> T _Vec3<T>::min() const noexcept { return Min(); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::max(const _Vec3& other) const noexcept { return Max(other); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::max(T scalar) const noexcept { return Max(scalar); }
    template <std::floating_point T> T _Vec3<T>::max() const noexcept { return Max(); }
    template <std::floating_point T> T _Vec3<T>::dot_product(const _Vec3& other) const noexcept { return DotProduct(other); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::cross_product(const _Vec3& other) const noexcept { return CrossProduct(other); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::normalised() const noexcept { return Normalised(); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::abs() const noexcept { return Abs(); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::negate() const noexcept { return Negate(); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::clamp(const _Vec3& minVec, const _Vec3& maxVec) const noexcept { return Clamp(minVec, maxVec); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::clamp(T minVal, T maxVal) const noexcept { return Clamp(minVal, maxVal); }
    template <std::floating_point T> bool _Vec3<T>::iszero(T epsilon) const noexcept { return IsZero(epsilon); }
    template <std::floating_point T> bool _Vec3<T>::isunit(T epsilon) const noexcept { return IsUnit(epsilon); }
    template <std::floating_point T> bool _Vec3<T>::areperpendicular(const _Vec3& other, T epsilon) const noexcept { return ArePerpendicular(other, epsilon); }
    template <std::floating_point T> T _Vec3<T>::distance(const _Vec3& other) const noexcept { return Distance(other); }
    template <std::floating_point T> T _Vec3<T>::distancesquared(const _Vec3& other) const noexcept { return DistanceSquared(other); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::lerp(const _Vec3& target, T t) const noexcept { return Lerp(target, t); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::smoothstep(const _Vec3& target, T t) const noexcept { return SmoothStep(target, t); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::smootherstep(const _Vec3& target, T t) const noexcept { return SmootherStep(target, t); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::nlerp(const _Vec3& target, T t) const noexcept { return Nlerp(target, t); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::slerp(const _Vec3& target, T t) const noexcept { return Slerp(target, t); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::move_towards(const _Vec3& target, T maxDistanceDelta) const noexcept { return MoveTowards(target, maxDistanceDelta); }

    // Static utility methods
    template <std::floating_point T> _Vec3<T> _Vec3<T>::Min(const _Vec3& a, const _Vec3& b) noexcept { return a.Min(b); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::Max(const _Vec3& a, const _Vec3& b) noexcept { return a.Max(b); }
    template <std::floating_point T> T _Vec3<T>::Dot(const _Vec3& a, const _Vec3& b) noexcept { return a.Dot(b); }
    template <std::floating_point T> T _Vec3<T>::DotProduct(const _Vec3& a, const _Vec3& b) noexcept { return a.DotProduct(b); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::Cross(const _Vec3& a, const _Vec3& b) noexcept { return a.Cross(b); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::CrossProduct(const _Vec3& a, const _Vec3& b) noexcept { return a.CrossProduct(b); }
    template <std::floating_point T> T _Vec3<T>::Distance(const _Vec3& a, const _Vec3& b) noexcept { return a.Distance(b); }
    template <std::floating_point T> T _Vec3<T>::DistanceSquared(const _Vec3& a, const _Vec3& b) noexcept { return a.DistanceSquared(b); }
    template <std::floating_point T> bool _Vec3<T>::ArePerpendicular(const _Vec3& a, const _Vec3& b, T epsilon) noexcept { return a.ArePerpendicular(b, epsilon); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::Clamp(const _Vec3& v, const _Vec3& minVec, const _Vec3& maxVec) noexcept { return v.Clamp(minVec, maxVec); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::Clamp(const _Vec3& v, T minVal, T maxVal) noexcept { return v.Clamp(minVal, maxVal); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::Abs(const _Vec3& v) noexcept { return v.Abs(); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::Negate(const _Vec3& v) noexcept { return v.Negate(); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::Lerp(const _Vec3& a, const _Vec3& b, T t) noexcept { return a.Lerp(b, t); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::SmoothStep(const _Vec3& a, const _Vec3& b, T t) noexcept { return a.SmoothStep(b, t); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::SmootherStep(const _Vec3& a, const _Vec3& b, T t) noexcept { return a.SmootherStep(b, t); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::Nlerp(const _Vec3& a, const _Vec3& b, T t) noexcept { return a.Nlerp(b, t); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::Slerp(const _Vec3& a, const _Vec3& b, T t) noexcept { return a.Slerp(b, t); }
    template <std::floating_point T> _Vec3<T> _Vec3<T>::MoveTowards(const _Vec3& current, const _Vec3& target, T maxDistanceDelta) noexcept { return current.MoveTowards(target, maxDistanceDelta); }

    template <std::floating_point T>
    bool _Vec3<T>::operator==(const _Vec3& rhs) const noexcept
    {
        return ApproximatelyEqual(x, rhs.x) && ApproximatelyEqual(y, rhs.y) && ApproximatelyEqual(z, rhs.z);
    }

    template struct _Vec3<float>;
    template struct _Vec3<double>;
}

