// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Vec2.h"

#include <algorithm>
#include <cmath>

namespace Sandbox3D::Maths
{
    template <typename T>
        requires std::is_arithmetic_v<T>
    typename _Vec2<T>::LengthType _Vec2<T>::Magnitude() const noexcept
    {
        return Length();
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    T _Vec2<T>::MagnitudeSquared() const noexcept
    {
        return LengthSquared();
    }

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
    T _Vec2<T>::DotProduct(const _Vec2& other) const noexcept
    {
        return Dot(other);
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    T _Vec2<T>::CrossProduct(const _Vec2& other) const noexcept
    {
        return Cross(other);
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
    _Vec2<T> _Vec2<T>::Min(const _Vec2& other) const noexcept
    {
        return _Vec2((x < other.x) ? x : other.x, (y < other.y) ? y : other.y);
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::Min(T scalar) const noexcept
    {
        return _Vec2((x < scalar) ? x : scalar, (y < scalar) ? y : scalar);
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    T _Vec2<T>::Min() const noexcept
    {
        return (x < y) ? x : y;
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::Max(const _Vec2& other) const noexcept
    {
        return _Vec2((x > other.x) ? x : other.x, (y > other.y) ? y : other.y);
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::Max(T scalar) const noexcept
    {
        return _Vec2((x > scalar) ? x : scalar, (y > scalar) ? y : scalar);
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    T _Vec2<T>::Max() const noexcept
    {
        return (x > y) ? x : y;
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::Abs() const noexcept
    {
        return _Vec2(
            (x < static_cast<T>(0)) ? -x : x,
            (y < static_cast<T>(0)) ? -y : y
        );
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::Negate() const noexcept
    {
        return _Vec2(-x, -y);
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::Negated() const noexcept
    {
        return Negate();
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::Clamp(const _Vec2& minVec, const _Vec2& maxVec) const noexcept
    {
        return _Vec2(
            Sandbox3D::Maths::Clamp(x, minVec.x, maxVec.x),
            Sandbox3D::Maths::Clamp(y, minVec.y, maxVec.y)
        );
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::Clamp(T minVal, T maxVal) const noexcept
    {
        return _Vec2(
            Sandbox3D::Maths::Clamp(x, minVal, maxVal),
            Sandbox3D::Maths::Clamp(y, minVal, maxVal)
        );
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::ClampMagnitude(T minLen, T maxLen) const noexcept
        requires std::is_floating_point_v<T>
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

    template <typename T>
        requires std::is_arithmetic_v<T>
    bool _Vec2<T>::IsZero(T epsilon) const noexcept
    {
        return ApproximatelyEqual(x, static_cast<T>(0), epsilon) &&
               ApproximatelyEqual(y, static_cast<T>(0), epsilon);
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    bool _Vec2<T>::IsUnit(T epsilon) const noexcept
        requires std::is_floating_point_v<T>
    {
        return ApproximatelyEqual(LengthSquared(), static_cast<T>(1), epsilon);
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    bool _Vec2<T>::ArePerpendicular(const _Vec2& other, T epsilon) const noexcept
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            const T lenSqProd = LengthSquared() * other.LengthSquared();
            if (lenSqProd <= static_cast<T>(0))
            {
                return false;
            }
            const T d = Dot(other);
            return (d * d) <= (epsilon * epsilon * lenSqProd);
        }
        else
        {
            return Dot(other) == static_cast<T>(0);
        }
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::Lerp(const _Vec2& target, T t) const noexcept
        requires std::is_floating_point_v<T>
    {
        return _Vec2(
            Sandbox3D::Maths::Lerp(x, target.x, t),
            Sandbox3D::Maths::Lerp(y, target.y, t)
        );
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::Lerp(const _Vec2& target, const _Vec2& t) const noexcept
        requires std::is_floating_point_v<T>
    {
        return _Vec2(
            Sandbox3D::Maths::Lerp(x, target.x, t.x),
            Sandbox3D::Maths::Lerp(y, target.y, t.y)
        );
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::SmoothStep(const _Vec2& target, T t) const noexcept
        requires std::is_floating_point_v<T>
    {
        const T clampedT = Sandbox3D::Maths::Clamp(t, static_cast<T>(0), static_cast<T>(1));
        const T factor = clampedT * clampedT * (static_cast<T>(3) - static_cast<T>(2) * clampedT);
        return Lerp(target, factor);
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::SmoothStep(const _Vec2& edge0, const _Vec2& edge1) const noexcept
        requires std::is_floating_point_v<T>
    {
        return _Vec2(
            Sandbox3D::Maths::SmoothStep(edge0.x, edge1.x, x),
            Sandbox3D::Maths::SmoothStep(edge0.y, edge1.y, y)
        );
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::SmoothStep(T edge0, T edge1) const noexcept
        requires std::is_floating_point_v<T>
    {
        return _Vec2(
            Sandbox3D::Maths::SmoothStep(edge0, edge1, x),
            Sandbox3D::Maths::SmoothStep(edge0, edge1, y)
        );
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::SmootherStep(const _Vec2& target, T t) const noexcept
        requires std::is_floating_point_v<T>
    {
        const T clampedT = Sandbox3D::Maths::Clamp(t, static_cast<T>(0), static_cast<T>(1));
        const T factor = clampedT * clampedT * clampedT * (clampedT * (clampedT * static_cast<T>(6) - static_cast<T>(15)) + static_cast<T>(10));
        return Lerp(target, factor);
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::SmootherStep(const _Vec2& edge0, const _Vec2& edge1) const noexcept
        requires std::is_floating_point_v<T>
    {
        return _Vec2(
            Sandbox3D::Maths::SmootherStep(edge0.x, edge1.x, x),
            Sandbox3D::Maths::SmootherStep(edge0.y, edge1.y, y)
        );
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::SmootherStep(T edge0, T edge1) const noexcept
        requires std::is_floating_point_v<T>
    {
        return _Vec2(
            Sandbox3D::Maths::SmootherStep(edge0, edge1, x),
            Sandbox3D::Maths::SmootherStep(edge0, edge1, y)
        );
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::Nlerp(const _Vec2& target, T t) const noexcept
        requires std::is_floating_point_v<T>
    {
        return Lerp(target, t).Normalised();
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::Slerp(const _Vec2& target, T t) const noexcept
        requires std::is_floating_point_v<T>
    {
        const T lenA = Length();
        const T lenB = target.Length();
        if (lenA <= static_cast<T>(0) || lenB <= static_cast<T>(0))
        {
            return Lerp(target, t);
        }

        const _Vec2 uA = *this / lenA;
        const _Vec2 uB = target / lenB;

        T cosOmega = uA.Dot(uB);
        cosOmega = Sandbox3D::Maths::Clamp(cosOmega, static_cast<T>(-1), static_cast<T>(1));

        if (cosOmega > static_cast<T>(1) - DefaultEpsilon<T>)
        {
            return Lerp(target, t);
        }

        const T interpolatedLength = Sandbox3D::Maths::Lerp(lenA, lenB, t);

        if (cosOmega < -(static_cast<T>(1) - DefaultEpsilon<T>))
        {
            const _Vec2 uPerp(-uA.y, uA.x);
            const T angle = Pi<T> * t;
            const _Vec2 uResult = uA * std::cos(angle) + uPerp * std::sin(angle);
            return uResult * interpolatedLength;
        }

        const T omega = std::acos(cosOmega);
        const T sinOmega = std::sin(omega);
        const T scaleA = std::sin((static_cast<T>(1) - t) * omega) / sinOmega;
        const T scaleB = std::sin(t * omega) / sinOmega;

        const _Vec2 uResult = uA * scaleA + uB * scaleB;
        return uResult * interpolatedLength;
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    _Vec2<T> _Vec2<T>::MoveTowards(const _Vec2& target, T maxDistanceDelta) const noexcept
        requires std::is_floating_point_v<T>
    {
        const _Vec2 delta = target - *this;
        const T distSq = delta.LengthSquared();
        if (distSq <= maxDistanceDelta * maxDistanceDelta || distSq <= static_cast<T>(0))
        {
            return target;
        }
        const T dist = std::sqrt(distSq);
        return *this + delta * (maxDistanceDelta / dist);
    }

    // Lowercase aliases
    template <typename T> requires std::is_arithmetic_v<T> typename _Vec2<T>::LengthType _Vec2<T>::magnitude() const noexcept { return Magnitude(); }
    template <typename T> requires std::is_arithmetic_v<T> T _Vec2<T>::magnitude_squared() const noexcept { return MagnitudeSquared(); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::min(const _Vec2& other) const noexcept { return Min(other); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::min(T scalar) const noexcept { return Min(scalar); }
    template <typename T> requires std::is_arithmetic_v<T> T _Vec2<T>::min() const noexcept { return Min(); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::max(const _Vec2& other) const noexcept { return Max(other); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::max(T scalar) const noexcept { return Max(scalar); }
    template <typename T> requires std::is_arithmetic_v<T> T _Vec2<T>::max() const noexcept { return Max(); }
    template <typename T> requires std::is_arithmetic_v<T> T _Vec2<T>::dot_product(const _Vec2& other) const noexcept { return DotProduct(other); }
    template <typename T> requires std::is_arithmetic_v<T> T _Vec2<T>::cross_product(const _Vec2& other) const noexcept { return CrossProduct(other); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::normalised() const noexcept requires std::is_floating_point_v<T> { return Normalised(); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::abs() const noexcept { return Abs(); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::negate() const noexcept { return Negate(); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::clamp(const _Vec2& minVec, const _Vec2& maxVec) const noexcept { return Clamp(minVec, maxVec); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::clamp(T minVal, T maxVal) const noexcept { return Clamp(minVal, maxVal); }
    template <typename T> requires std::is_arithmetic_v<T> bool _Vec2<T>::iszero(T epsilon) const noexcept { return IsZero(epsilon); }
    template <typename T> requires std::is_arithmetic_v<T> bool _Vec2<T>::isunit(T epsilon) const noexcept requires std::is_floating_point_v<T> { return IsUnit(epsilon); }
    template <typename T> requires std::is_arithmetic_v<T> bool _Vec2<T>::areperpendicular(const _Vec2& other, T epsilon) const noexcept { return ArePerpendicular(other, epsilon); }
    template <typename T> requires std::is_arithmetic_v<T> typename _Vec2<T>::LengthType _Vec2<T>::distance(const _Vec2& other) const noexcept { return Distance(other); }
    template <typename T> requires std::is_arithmetic_v<T> T _Vec2<T>::distancesquared(const _Vec2& other) const noexcept { return DistanceSquared(other); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::lerp(const _Vec2& target, T t) const noexcept requires std::is_floating_point_v<T> { return Lerp(target, t); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::smoothstep(const _Vec2& target, T t) const noexcept requires std::is_floating_point_v<T> { return SmoothStep(target, t); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::smootherstep(const _Vec2& target, T t) const noexcept requires std::is_floating_point_v<T> { return SmootherStep(target, t); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::nlerp(const _Vec2& target, T t) const noexcept requires std::is_floating_point_v<T> { return Nlerp(target, t); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::slerp(const _Vec2& target, T t) const noexcept requires std::is_floating_point_v<T> { return Slerp(target, t); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::move_towards(const _Vec2& target, T maxDistanceDelta) const noexcept requires std::is_floating_point_v<T> { return MoveTowards(target, maxDistanceDelta); }

    // Static utility methods
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::Min(const _Vec2& a, const _Vec2& b) noexcept { return a.Min(b); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::Max(const _Vec2& a, const _Vec2& b) noexcept { return a.Max(b); }
    template <typename T> requires std::is_arithmetic_v<T> T _Vec2<T>::Dot(const _Vec2& a, const _Vec2& b) noexcept { return a.Dot(b); }
    template <typename T> requires std::is_arithmetic_v<T> T _Vec2<T>::DotProduct(const _Vec2& a, const _Vec2& b) noexcept { return a.DotProduct(b); }
    template <typename T> requires std::is_arithmetic_v<T> T _Vec2<T>::Cross(const _Vec2& a, const _Vec2& b) noexcept { return a.Cross(b); }
    template <typename T> requires std::is_arithmetic_v<T> T _Vec2<T>::CrossProduct(const _Vec2& a, const _Vec2& b) noexcept { return a.CrossProduct(b); }
    template <typename T> requires std::is_arithmetic_v<T> typename _Vec2<T>::LengthType _Vec2<T>::Distance(const _Vec2& a, const _Vec2& b) noexcept { return a.Distance(b); }
    template <typename T> requires std::is_arithmetic_v<T> T _Vec2<T>::DistanceSquared(const _Vec2& a, const _Vec2& b) noexcept { return a.DistanceSquared(b); }
    template <typename T> requires std::is_arithmetic_v<T> bool _Vec2<T>::ArePerpendicular(const _Vec2& a, const _Vec2& b, T epsilon) noexcept { return a.ArePerpendicular(b, epsilon); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::Clamp(const _Vec2& v, const _Vec2& minVec, const _Vec2& maxVec) noexcept { return v.Clamp(minVec, maxVec); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::Clamp(const _Vec2& v, T minVal, T maxVal) noexcept { return v.Clamp(minVal, maxVal); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::Abs(const _Vec2& v) noexcept { return v.Abs(); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::Negate(const _Vec2& v) noexcept { return v.Negate(); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::Lerp(const _Vec2& a, const _Vec2& b, T t) noexcept requires std::is_floating_point_v<T> { return a.Lerp(b, t); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::SmoothStep(const _Vec2& a, const _Vec2& b, T t) noexcept requires std::is_floating_point_v<T> { return a.SmoothStep(b, t); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::SmootherStep(const _Vec2& a, const _Vec2& b, T t) noexcept requires std::is_floating_point_v<T> { return a.SmootherStep(b, t); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::Nlerp(const _Vec2& a, const _Vec2& b, T t) noexcept requires std::is_floating_point_v<T> { return a.Nlerp(b, t); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::Slerp(const _Vec2& a, const _Vec2& b, T t) noexcept requires std::is_floating_point_v<T> { return a.Slerp(b, t); }
    template <typename T> requires std::is_arithmetic_v<T> _Vec2<T> _Vec2<T>::MoveTowards(const _Vec2& current, const _Vec2& target, T maxDistanceDelta) noexcept requires std::is_floating_point_v<T> { return current.MoveTowards(target, maxDistanceDelta); }

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

