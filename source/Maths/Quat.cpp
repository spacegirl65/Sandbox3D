// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Quat.h"

#include <algorithm>
#include <cmath>

namespace Sandbox3D::Maths
{
    template <std::floating_point T>
    T _Quat<T>::Length() const noexcept
    {
        return std::sqrt(LengthSquared());
    }

    template <std::floating_point T>
    _Quat<T> _Quat<T>::Normalised() const noexcept
    {
        const T len = Length();
        if (len > static_cast<T>(0))
        {
            const T invLen = static_cast<T>(1) / len;
            return _Quat<T>(x * invLen, y * invLen, z * invLen, w * invLen);
        }
        return Identity();
    }

    template <std::floating_point T>
    void _Quat<T>::Normalise() noexcept
    {
        *this = Normalised();
    }

    template <std::floating_point T>
    _Quat<T> _Quat<T>::Inverted() const noexcept
    {
        const T lenSq = LengthSquared();
        if (lenSq > static_cast<T>(0))
        {
            const T invLenSq = static_cast<T>(1) / lenSq;
            return _Quat<T>(-x * invLenSq, -y * invLenSq, -z * invLenSq, w * invLenSq);
        }
        return Identity();
    }

    template <std::floating_point T>
    void _Quat<T>::Invert() noexcept
    {
        *this = Inverted();
    }

    template <std::floating_point T>
    _Quat<T> _Quat<T>::operator*(const _Quat& rhs) const noexcept
    {
        return _Quat<T>(
            w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
            w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
            w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w,
            w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z
        );
    }

    template <std::floating_point T>
    _Quat<T>& _Quat<T>::operator*=(const _Quat& rhs) noexcept
    {
        *this = *this * rhs;
        return *this;
    }

    template <std::floating_point T>
    _Vec3<T> _Quat<T>::Rotate(const _Vec3<T>& vec) const noexcept
    {
        const _Vec3<T> qv(x, y, z);
        const _Vec3<T> t = static_cast<T>(2) * qv.Cross(vec);
        return vec + t * w + qv.Cross(t);
    }

    template <std::floating_point T>
    _Mat3x3<T> _Quat<T>::ToRotationMatrix3x3() const noexcept
    {
        const T xx = x * x;
        const T yy = y * y;
        const T zz = z * z;
        const T xy = x * y;
        const T xz = x * z;
        const T yz = y * z;
        const T wx = w * x;
        const T wy = w * y;
        const T wz = w * z;

        return _Mat3x3<T>(
            static_cast<T>(1) - static_cast<T>(2) * (yy + zz),
            static_cast<T>(2) * (xy + wz),
            static_cast<T>(2) * (xz - wy),

            static_cast<T>(2) * (xy - wz),
            static_cast<T>(1) - static_cast<T>(2) * (xx + zz),
            static_cast<T>(2) * (yz + wx),

            static_cast<T>(2) * (xz + wy),
            static_cast<T>(2) * (yz - wx),
            static_cast<T>(1) - static_cast<T>(2) * (xx + yy)
        );
    }

    template <std::floating_point T>
    _Mat4x4<T> _Quat<T>::ToRotationMatrix4x4() const noexcept
    {
        const T xx = x * x;
        const T yy = y * y;
        const T zz = z * z;
        const T xy = x * y;
        const T xz = x * z;
        const T yz = y * z;
        const T wx = w * x;
        const T wy = w * y;
        const T wz = w * z;

        return _Mat4x4<T>(
            static_cast<T>(1) - static_cast<T>(2) * (yy + zz),
            static_cast<T>(2) * (xy + wz),
            static_cast<T>(2) * (xz - wy),
            static_cast<T>(0),

            static_cast<T>(2) * (xy - wz),
            static_cast<T>(1) - static_cast<T>(2) * (xx + zz),
            static_cast<T>(2) * (yz + wx),
            static_cast<T>(0),

            static_cast<T>(2) * (xz + wy),
            static_cast<T>(2) * (yz - wx),
            static_cast<T>(1) - static_cast<T>(2) * (xx + yy),
            static_cast<T>(0),

            static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), static_cast<T>(1)
        );
    }

    template <std::floating_point T>
    _Vec3<T> _Quat<T>::ToEulerAngles() const noexcept
    {
        _Vec3<T> angles{};

        const T sinPitch = static_cast<T>(2) * (w * x - y * z);
        if (std::abs(sinPitch) >= static_cast<T>(0.99999))
        {
            // Gimbal lock condition at pitch +/- 90 degrees
            angles.x = std::copysign(Maths::HalfPi<T>, sinPitch);
            angles.y = static_cast<T>(2) * std::atan2(y, w);
            angles.z = static_cast<T>(0);
        }
        else
        {
            angles.x = std::asin(std::clamp(sinPitch, static_cast<T>(-1), static_cast<T>(1)));
            angles.y = std::atan2(static_cast<T>(2) * (w * y + x * z), static_cast<T>(1) - static_cast<T>(2) * (x * x + y * y));
            angles.z = std::atan2(static_cast<T>(2) * (w * z + x * y), static_cast<T>(1) - static_cast<T>(2) * (x * x + z * z));
        }

        return angles;
    }

    template <std::floating_point T>
    void _Quat<T>::ToAxisAngle(_Vec3<T>& outAxis, T& outAngleRadians) const noexcept
    {
        const _Quat<T> q = (w > static_cast<T>(1) || w < static_cast<T>(-1)) ? Normalised() : *this;
        outAngleRadians = static_cast<T>(2) * std::acos(std::clamp(q.w, static_cast<T>(-1), static_cast<T>(1)));
        const T s = std::sqrt(std::max(static_cast<T>(0), static_cast<T>(1) - q.w * q.w));
        if (s < static_cast<T>(1e-6))
        {
            outAxis = _Vec3<T>(static_cast<T>(1), static_cast<T>(0), static_cast<T>(0));
        }
        else
        {
            outAxis = _Vec3<T>(q.x / s, q.y / s, q.z / s);
        }
    }

    template <std::floating_point T>
    _Quat<T> _Quat<T>::Integrate(const _Vec3<T>& angularVelocity, T deltaTime) const noexcept
    {
        const _Quat<T> omega(angularVelocity.x, angularVelocity.y, angularVelocity.z, static_cast<T>(0));
        const _Quat<T> dq = omega * (*this) * (static_cast<T>(0.5) * deltaTime);
        return (*this + dq).Normalised();
    }

    template <std::floating_point T>
    _Quat<T> _Quat<T>::FromAxisAngle(const _Vec3<T>& axis, T angleRadians) noexcept
    {
        const _Vec3<T> normAxis = axis.Normalised();
        const T halfAngle = angleRadians * static_cast<T>(0.5);
        const T s = std::sin(halfAngle);
        return _Quat<T>(normAxis.x * s, normAxis.y * s, normAxis.z * s, std::cos(halfAngle));
    }

    template <std::floating_point T>
    _Quat<T> _Quat<T>::FromEulerAngles(T pitch, T yaw, T roll) noexcept
    {
        const T hp = pitch * static_cast<T>(0.5);
        const T hy = yaw * static_cast<T>(0.5);
        const T hr = roll * static_cast<T>(0.5);

        const T sp = std::sin(hp);
        const T cp = std::cos(hp);
        const T sy = std::sin(hy);
        const T cy = std::cos(hy);
        const T sr = std::sin(hr);
        const T cr = std::cos(hr);

        return _Quat<T>(
            sp * cy * cr - cp * sy * sr,
            cp * sy * cr + sp * cy * sr,
            cp * cy * sr - sp * sy * cr,
            cp * cy * cr + sp * sy * sr
        ).Normalised();
    }

    template <std::floating_point T>
    _Quat<T> _Quat<T>::FromEuler(const _Vec3<T>& eulerRadians) noexcept
    {
        return FromEulerAngles(eulerRadians.x, eulerRadians.y, eulerRadians.z);
    }

    template <std::floating_point T>
    _Quat<T> _Quat<T>::FromRotationMatrix(const _Mat3x3<T>& mat) noexcept
    {
        const T trace = mat.m[0][0] + mat.m[1][1] + mat.m[2][2];
        if (trace > static_cast<T>(0))
        {
            const T s = static_cast<T>(0.5) / std::sqrt(trace + static_cast<T>(1));
            return _Quat<T>(
                (mat.m[1][2] - mat.m[2][1]) * s,
                (mat.m[2][0] - mat.m[0][2]) * s,
                (mat.m[0][1] - mat.m[1][0]) * s,
                static_cast<T>(0.25) / s
            ).Normalised();
        }

        if (mat.m[0][0] > mat.m[1][1] && mat.m[0][0] > mat.m[2][2])
        {
            const T s = static_cast<T>(2) * std::sqrt(static_cast<T>(1) + mat.m[0][0] - mat.m[1][1] - mat.m[2][2]);
            return _Quat<T>(
                static_cast<T>(0.25) * s,
                (mat.m[0][1] + mat.m[1][0]) / s,
                (mat.m[0][2] + mat.m[2][0]) / s,
                (mat.m[1][2] - mat.m[2][1]) / s
            ).Normalised();
        }

        if (mat.m[1][1] > mat.m[2][2])
        {
            const T s = static_cast<T>(2) * std::sqrt(static_cast<T>(1) + mat.m[1][1] - mat.m[0][0] - mat.m[2][2]);
            return _Quat<T>(
                (mat.m[0][1] + mat.m[1][0]) / s,
                static_cast<T>(0.25) * s,
                (mat.m[1][2] + mat.m[2][1]) / s,
                (mat.m[2][0] - mat.m[0][2]) / s
            ).Normalised();
        }

        const T s = static_cast<T>(2) * std::sqrt(static_cast<T>(1) + mat.m[2][2] - mat.m[0][0] - mat.m[1][1]);
        return _Quat<T>(
            (mat.m[0][2] + mat.m[2][0]) / s,
            (mat.m[1][2] + mat.m[2][1]) / s,
            static_cast<T>(0.25) * s,
            (mat.m[0][1] - mat.m[1][0]) / s
        ).Normalised();
    }

    template <std::floating_point T>
    _Quat<T> _Quat<T>::FromRotationMatrix(const _Mat4x4<T>& mat) noexcept
    {
        return FromRotationMatrix(mat.GetRotationMatrix());
    }

    template <std::floating_point T>
    _Quat<T> _Quat<T>::FromToRotation(const _Vec3<T>& from, const _Vec3<T>& to) noexcept
    {
        const _Vec3<T> v0 = from.Normalised();
        const _Vec3<T> v1 = to.Normalised();
        const T dot = v0.Dot(v1);

        if (dot >= static_cast<T>(1) - static_cast<T>(1e-6))
        {
            return Identity();
        }

        if (dot <= static_cast<T>(-1) + static_cast<T>(1e-6))
        {
            _Vec3<T> ortho = _Vec3<T>::Right().Cross(v0);
            if (ortho.LengthSquared() < static_cast<T>(1e-4))
            {
                ortho = _Vec3<T>::Up().Cross(v0);
            }
            return FromAxisAngle(ortho.Normalised(), Maths::Pi<T>);
        }

        const _Vec3<T> axis = v0.Cross(v1);
        return _Quat<T>(axis.x, axis.y, axis.z, static_cast<T>(1) + dot).Normalised();
    }

    template <std::floating_point T>
    _Quat<T> _Quat<T>::Lerp(const _Quat& a, const _Quat& b, T t) noexcept
    {
        const T cosHalfTheta = a.Dot(b);
        _Quat<T> targetB = b;
        if (cosHalfTheta < static_cast<T>(0))
        {
            targetB = -b;
        }
        return (a * (static_cast<T>(1) - t) + targetB * t).Normalised();
    }

    template <std::floating_point T>
    _Quat<T> _Quat<T>::Slerp(const _Quat& a, const _Quat& b, T t) noexcept
    {
        T cosHalfTheta = a.Dot(b);
        _Quat<T> targetB = b;
        if (cosHalfTheta < static_cast<T>(0))
        {
            targetB = -b;
            cosHalfTheta = -cosHalfTheta;
        }

        if (cosHalfTheta >= static_cast<T>(0.9995))
        {
            return Lerp(a, targetB, t);
        }

        const T halfTheta = std::acos(std::clamp(cosHalfTheta, static_cast<T>(-1), static_cast<T>(1)));
        const T sinHalfTheta = std::sin(halfTheta);
        const T wa = std::sin((static_cast<T>(1) - t) * halfTheta) / sinHalfTheta;
        const T wb = std::sin(t * halfTheta) / sinHalfTheta;

        return a * wa + targetB * wb;
    }

    template <std::floating_point T>
    bool _Quat<T>::operator==(const _Quat& rhs) const noexcept
    {
        return ApproximatelyEqual(x, rhs.x)
            && ApproximatelyEqual(y, rhs.y)
            && ApproximatelyEqual(z, rhs.z)
            && ApproximatelyEqual(w, rhs.w);
    }

    template struct _Quat<float>;
    template struct _Quat<double>;
}

