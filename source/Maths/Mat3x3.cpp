// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Mat3x3.h"

namespace Sandbox3D::Maths
{
    template <std::floating_point T>
    _Mat3x3<T> _Mat3x3<T>::Transposed() const noexcept
    {
        return _Mat3x3(
            m[0][0], m[1][0], m[2][0],
            m[0][1], m[1][1], m[2][1],
            m[0][2], m[1][2], m[2][2]
        );
    }

    template <std::floating_point T>
    void _Mat3x3<T>::Transpose() noexcept
    {
        *this = Transposed();
    }

    template <std::floating_point T>
    T _Mat3x3<T>::Determinant() const noexcept
    {
        return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
             - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
             + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    }

    template <std::floating_point T>
    _Mat3x3<T> _Mat3x3<T>::Inverted() const noexcept
    {
        const T det = Determinant();
        if (std::abs(det) <= DefaultEpsilon<T>)
        {
            return Zero();
        }

        const T invDet = static_cast<T>(1) / det;

        return _Mat3x3(
            (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet,
            (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invDet,
            (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet,

            (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invDet,
            (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet,
            (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * invDet,

            (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet,
            (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * invDet,
            (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet
        );
    }

    template <std::floating_point T>
    bool _Mat3x3<T>::Invert() noexcept
    {
        const T det = Determinant();
        if (std::abs(det) <= DefaultEpsilon<T>)
        {
            return false;
        }
        *this = Inverted();
        return true;
    }

    template <std::floating_point T>
    _Mat3x3<T> _Mat3x3<T>::RotationAroundX(T radians) noexcept
    {
        const T c = std::cos(radians);
        const T s = std::sin(radians);

        return _Mat3x3(
            static_cast<T>(1), static_cast<T>(0), static_cast<T>(0),
            static_cast<T>(0), c,                 s,
            static_cast<T>(0), -s,                c
        );
    }

    template <std::floating_point T>
    _Mat3x3<T> _Mat3x3<T>::RotationAroundY(T radians) noexcept
    {
        const T c = std::cos(radians);
        const T s = std::sin(radians);

        return _Mat3x3(
            c,                 static_cast<T>(0), -s,
            static_cast<T>(0), static_cast<T>(1), static_cast<T>(0),
            s,                 static_cast<T>(0), c
        );
    }

    template <std::floating_point T>
    _Mat3x3<T> _Mat3x3<T>::RotationAroundZ(T radians) noexcept
    {
        const T c = std::cos(radians);
        const T s = std::sin(radians);

        return _Mat3x3(
            c,                 s,                 static_cast<T>(0),
            -s,                c,                 static_cast<T>(0),
            static_cast<T>(0), static_cast<T>(0), static_cast<T>(1)
        );
    }

    template <std::floating_point T>
    _Mat3x3<T> _Mat3x3<T>::RotationYawPitchRoll(T yaw, T pitch, T roll) noexcept
    {
        // Left-handed compound rotation: Roll (Z) * Pitch (X) * Yaw (Y)
        return RotationAroundZ(roll) * RotationAroundX(pitch) * RotationAroundY(yaw);
    }

    template <std::floating_point T>
    _Mat3x3<T> _Mat3x3<T>::Scale(T sx, T sy, T sz) noexcept
    {
        return _Mat3x3(
            sx,                static_cast<T>(0), static_cast<T>(0),
            static_cast<T>(0), sy,                static_cast<T>(0),
            static_cast<T>(0), static_cast<T>(0), sz
        );
    }

    template <std::floating_point T>
    _Mat3x3<T> _Mat3x3<T>::Scale(const _Vec3<T>& scale) noexcept
    {
        return Scale(scale.x, scale.y, scale.z);
    }

    template <std::floating_point T>
    _Vec3<T> _Mat3x3<T>::TransformVector(const _Vec3<T>& v) const noexcept
    {
        // Row-vector convention: v * M
        return _Vec3<T>(
            v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0],
            v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1],
            v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2]
        );
    }

    template <std::floating_point T>
    _Mat3x3<T> _Mat3x3<T>::operator*(const _Mat3x3& rhs) const noexcept
    {
        _Mat3x3 res = Zero();
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                res.m[i][j] = m[i][0] * rhs.m[0][j]
                            + m[i][1] * rhs.m[1][j]
                            + m[i][2] * rhs.m[2][j];
            }
        }
        return res;
    }

    template <std::floating_point T>
    bool _Mat3x3<T>::operator==(const _Mat3x3& rhs) const noexcept
    {
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                if (!ApproximatelyEqual(m[i][j], rhs.m[i][j]))
                {
                    return false;
                }
            }
        }
        return true;
    }

    // Explicit template instantiations
    template struct _Mat3x3<float>;
    template struct _Mat3x3<double>;
}

