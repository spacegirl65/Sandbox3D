// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Mat4x4.h"

namespace Sandbox3D::Maths
{
    template <std::floating_point T>
    constexpr _Mat4x4<T> _Mat4x4<T>::Zero() noexcept
    {
        return _Mat4x4(
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0
        );
    }

    template <std::floating_point T>
    _Mat4x4<T> _Mat4x4<T>::Transposed() const noexcept
    {
        return _Mat4x4(
            m[0][0], m[1][0], m[2][0], m[3][0],
            m[0][1], m[1][1], m[2][1], m[3][1],
            m[0][2], m[1][2], m[2][2], m[3][2],
            m[0][3], m[1][3], m[2][3], m[3][3]
        );
    }

    template <std::floating_point T>
    void _Mat4x4<T>::Transpose() noexcept
    {
        *this = Transposed();
    }

    template <std::floating_point T>
    T _Mat4x4<T>::Determinant() const noexcept
    {
        const T s0 = m[0][0] * m[1][1] - m[1][0] * m[0][1];
        const T s1 = m[0][0] * m[1][2] - m[1][0] * m[0][2];
        const T s2 = m[0][0] * m[1][3] - m[1][0] * m[0][3];
        const T s3 = m[0][1] * m[1][2] - m[1][1] * m[0][2];
        const T s4 = m[0][1] * m[1][3] - m[1][1] * m[0][3];
        const T s5 = m[0][2] * m[1][3] - m[1][2] * m[0][3];

        const T c5 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
        const T c4 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
        const T c3 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
        const T c2 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
        const T c1 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
        const T c0 = m[2][0] * m[3][1] - m[3][0] * m[2][1];

        return s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
    }

    template <std::floating_point T>
    _Mat4x4<T> _Mat4x4<T>::Inverted() const noexcept
    {
        const T s0 = m[0][0] * m[1][1] - m[1][0] * m[0][1];
        const T s1 = m[0][0] * m[1][2] - m[1][0] * m[0][2];
        const T s2 = m[0][0] * m[1][3] - m[1][0] * m[0][3];
        const T s3 = m[0][1] * m[1][2] - m[1][1] * m[0][2];
        const T s4 = m[0][1] * m[1][3] - m[1][1] * m[0][3];
        const T s5 = m[0][2] * m[1][3] - m[1][2] * m[0][3];

        const T c5 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
        const T c4 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
        const T c3 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
        const T c2 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
        const T c1 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
        const T c0 = m[2][0] * m[3][1] - m[3][0] * m[2][1];

        const T det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
        if (std::abs(det) <= DefaultEpsilon<T>)
        {
            return Zero();
        }

        const T invDet = static_cast<T>(1) / det;

        return _Mat4x4(
            ( m[1][1] * c5 - m[1][2] * c4 + m[1][3] * c3) * invDet,
            (-m[0][1] * c5 + m[0][2] * c4 - m[0][3] * c3) * invDet,
            ( m[3][1] * s5 - m[3][2] * s4 + m[3][3] * s3) * invDet,
            (-m[2][1] * s5 + m[2][2] * s4 - m[2][3] * s3) * invDet,

            (-m[1][0] * c5 + m[1][2] * c2 - m[1][3] * c1) * invDet,
            ( m[0][0] * c5 - m[0][2] * c2 + m[0][3] * c1) * invDet,
            (-m[3][0] * s5 + m[3][2] * s2 - m[3][3] * s1) * invDet,
            ( m[2][0] * s5 - m[2][2] * s2 + m[2][3] * s1) * invDet,

            ( m[1][0] * c4 - m[1][1] * c2 + m[1][3] * c0) * invDet,
            (-m[0][0] * c4 + m[0][1] * c2 - m[0][3] * c0) * invDet,
            ( m[3][0] * s4 - m[3][1] * s2 + m[3][3] * s0) * invDet,
            (-m[2][0] * s4 + m[2][1] * s2 - m[2][3] * s0) * invDet,

            (-m[1][0] * c3 + m[1][1] * c1 - m[1][2] * c0) * invDet,
            ( m[0][0] * c3 - m[0][1] * c1 + m[0][2] * c0) * invDet,
            (-m[3][0] * s3 + m[3][1] * s1 - m[3][2] * s0) * invDet,
            ( m[2][0] * s3 - m[2][1] * s1 + m[2][2] * s0) * invDet
        );
    }

    template <std::floating_point T>
    bool _Mat4x4<T>::Invert() noexcept
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
    _Vec3<T> _Mat4x4<T>::GetTranslation() const noexcept
    {
        return _Vec3<T>(m[3][0], m[3][1], m[3][2]);
    }

    template <std::floating_point T>
    void _Mat4x4<T>::SetTranslation(const _Vec3<T>& translation) noexcept
    {
        m[3][0] = translation.x;
        m[3][1] = translation.y;
        m[3][2] = translation.z;
    }

    template <std::floating_point T>
    _Mat3x3<T> _Mat4x4<T>::GetRotationMatrix() const noexcept
    {
        return _Mat3x3<T>(
            m[0][0], m[0][1], m[0][2],
            m[1][0], m[1][1], m[1][2],
            m[2][0], m[2][1], m[2][2]
        );
    }

    template <std::floating_point T>
    _Vec3<T> _Mat4x4<T>::GetEulerAngles() const noexcept
    {
        const T scaleX = _Vec3<T>(m[0][0], m[0][1], m[0][2]).Length();
        const T scaleY = _Vec3<T>(m[1][0], m[1][1], m[1][2]).Length();
        const T scaleZ = _Vec3<T>(m[2][0], m[2][1], m[2][2]).Length();

        if (scaleX <= DefaultEpsilon<T> || scaleY <= DefaultEpsilon<T> || scaleZ <= DefaultEpsilon<T>)
        {
            return _Vec3<T>::Zero();
        }

        const T invX = static_cast<T>(1) / scaleX;
        const T invY = static_cast<T>(1) / scaleY;
        T invZ = static_cast<T>(1) / scaleZ;

        // Check for reflection (negative determinant)
        const _Vec3<T> row0(m[0][0] * invX, m[0][1] * invX, m[0][2] * invX);
        const _Vec3<T> row1(m[1][0] * invY, m[1][1] * invY, m[1][2] * invY);
        const _Vec3<T> row2(m[2][0] * invZ, m[2][1] * invZ, m[2][2] * invZ);

        if (row0.Cross(row1).Dot(row2) < static_cast<T>(0))
        {
            invZ = -invZ;
        }

        const T r00 = m[0][0] * invX;
        const T r01 = m[0][1] * invX;
        const T r02 = m[0][2] * invX;

        const T r10 = m[1][0] * invY;
        const T r11 = m[1][1] * invY;
        const T r12 = m[1][2] * invY;

        const T r20 = m[2][0] * invZ;
        const T r21 = m[2][1] * invZ;
        const T r22 = m[2][2] * invZ;

        T pitch = static_cast<T>(0);
        T yaw   = static_cast<T>(0);
        T roll  = static_cast<T>(0);

        // In Left-Handed Roll(Z) * Pitch(X) * Yaw(Y):
        // r21 is -sin(pitch)
        const T sinPitch = -Clamp(r21, static_cast<T>(-1), static_cast<T>(1));
        pitch = std::asin(sinPitch);

        // Test for gimbal lock where cos(pitch) is near zero
        if (std::abs(r21) < static_cast<T>(1) - DefaultEpsilon<T>)
        {
            yaw = std::atan2(r20, r22);
            roll = std::atan2(r01, r11);
        }
        else
        {
            // Gimbal lock: pitch is +/- pi/2
            // Set yaw to 0 and solve for roll
            yaw = static_cast<T>(0);
            if (r21 < static_cast<T>(0))
            {
                // pitch = +pi/2
                roll = std::atan2(r02, r00);
            }
            else
            {
                // pitch = -pi/2
                roll = std::atan2(-r02, r00);
            }
        }

        return _Vec3<T>(pitch, yaw, roll);
    }

    template <std::floating_point T>
    _Mat4x4<T> _Mat4x4<T>::Translation(T x, T y, T z) noexcept
    {
        return _Mat4x4(
            static_cast<T>(1), static_cast<T>(0), static_cast<T>(0), static_cast<T>(0),
            static_cast<T>(0), static_cast<T>(1), static_cast<T>(0), static_cast<T>(0),
            static_cast<T>(0), static_cast<T>(0), static_cast<T>(1), static_cast<T>(0),
            x,                 y,                 z,                 static_cast<T>(1)
        );
    }

    template <std::floating_point T>
    _Mat4x4<T> _Mat4x4<T>::Translation(const _Vec3<T>& translation) noexcept
    {
        return Translation(translation.x, translation.y, translation.z);
    }

    template <std::floating_point T>
    _Mat4x4<T> _Mat4x4<T>::RotationAroundX(T radians) noexcept
    {
        const T c = std::cos(radians);
        const T s = std::sin(radians);

        return _Mat4x4(
            static_cast<T>(1), static_cast<T>(0), static_cast<T>(0), static_cast<T>(0),
            static_cast<T>(0), c,                 s,                 static_cast<T>(0),
            static_cast<T>(0), -s,                c,                 static_cast<T>(0),
            static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), static_cast<T>(1)
        );
    }

    template <std::floating_point T>
    _Mat4x4<T> _Mat4x4<T>::RotationAroundY(T radians) noexcept
    {
        const T c = std::cos(radians);
        const T s = std::sin(radians);

        return _Mat4x4(
            c,                 static_cast<T>(0), -s,                static_cast<T>(0),
            static_cast<T>(0), static_cast<T>(1), static_cast<T>(0), static_cast<T>(0),
            s,                 static_cast<T>(0), c,                 static_cast<T>(0),
            static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), static_cast<T>(1)
        );
    }

    template <std::floating_point T>
    _Mat4x4<T> _Mat4x4<T>::RotationAroundZ(T radians) noexcept
    {
        const T c = std::cos(radians);
        const T s = std::sin(radians);

        return _Mat4x4(
            c,                 s,                 static_cast<T>(0), static_cast<T>(0),
            -s,                c,                 static_cast<T>(0), static_cast<T>(0),
            static_cast<T>(0), static_cast<T>(0), static_cast<T>(1), static_cast<T>(0),
            static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), static_cast<T>(1)
        );
    }

    template <std::floating_point T>
    _Mat4x4<T> _Mat4x4<T>::RotationYawPitchRoll(T yaw, T pitch, T roll) noexcept
    {
        // Left-handed compound rotation: Roll (Z) * Pitch (X) * Yaw (Y)
        return RotationAroundZ(roll) * RotationAroundX(pitch) * RotationAroundY(yaw);
    }

    template <std::floating_point T>
    _Mat4x4<T> _Mat4x4<T>::RotationAroundAxis(const _Vec3<T>& axis, T radians) noexcept
    {
        const _Vec3<T> a = axis.Normalised();
        const T c = std::cos(radians);
        const T s = std::sin(radians);
        const T t = static_cast<T>(1) - c;

        return _Mat4x4(
            t * a.x * a.x + c,       t * a.x * a.y + s * a.z, t * a.x * a.z - s * a.y, static_cast<T>(0),
            t * a.x * a.y - s * a.z, t * a.y * a.y + c,       t * a.y * a.z + s * a.x, static_cast<T>(0),
            t * a.x * a.z + s * a.y, t * a.y * a.z - s * a.x, t * a.z * a.z + c,       static_cast<T>(0),
            static_cast<T>(0),       static_cast<T>(0),       static_cast<T>(0),       static_cast<T>(1)
        );
    }

    template <std::floating_point T>
    _Mat4x4<T> _Mat4x4<T>::Scale(T sx, T sy, T sz) noexcept
    {
        return _Mat4x4(
            sx,                static_cast<T>(0), static_cast<T>(0), static_cast<T>(0),
            static_cast<T>(0), sy,                static_cast<T>(0), static_cast<T>(0),
            static_cast<T>(0), static_cast<T>(0), sz,                static_cast<T>(0),
            static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), static_cast<T>(1)
        );
    }

    template <std::floating_point T>
    _Mat4x4<T> _Mat4x4<T>::Scale(const _Vec3<T>& scale) noexcept
    {
        return Scale(scale.x, scale.y, scale.z);
    }

    template <std::floating_point T>
    _Mat4x4<T> _Mat4x4<T>::World(const _Vec3<T>& position) noexcept
    {
        return Translation(position);
    }

    template <std::floating_point T>
    _Mat4x4<T> _Mat4x4<T>::World(const _Vec3<T>& position, const _Vec3<T>& eulerAngles) noexcept
    {
        _Mat4x4 result = RotationYawPitchRoll(eulerAngles.y, eulerAngles.x, eulerAngles.z);
        result.SetTranslation(position);
        return result;
    }

    template <std::floating_point T>
    _Mat4x4<T> _Mat4x4<T>::World(
        const _Vec3<T>& position,
        const _Vec3<T>& forwardVector,
        const _Vec3<T>& upVector
    ) noexcept
    {
        _Vec3<T> zAxis = forwardVector.Normalised();
        if (zAxis.IsZero())
        {
            zAxis = _Vec3<T>::Forward();
        }

        _Vec3<T> xAxis = upVector.Cross(zAxis).Normalised();
        if (xAxis.IsZero())
        {
            const _Vec3<T> fallbackUp = (std::abs(zAxis.y) < static_cast<T>(1) - DefaultEpsilon<T>)
                ? _Vec3<T>::Up()
                : _Vec3<T>::Forward();
            xAxis = fallbackUp.Cross(zAxis).Normalised();
        }
        const _Vec3<T> yAxis = zAxis.Cross(xAxis);

        return _Mat4x4(
            xAxis.x,    xAxis.y,    xAxis.z,    static_cast<T>(0),
            yAxis.x,    yAxis.y,    yAxis.z,    static_cast<T>(0),
            zAxis.x,    zAxis.y,    zAxis.z,    static_cast<T>(0),
            position.x, position.y, position.z, static_cast<T>(1)
        );
    }

    template <std::floating_point T>
    _Mat4x4<T> _Mat4x4<T>::LookAt(
        const _Vec3<T>& eyePosition,
        const _Vec3<T>& targetPosition,
        const _Vec3<T>& upVector
    ) noexcept
    {
        const _Vec3<T> zAxis = (targetPosition - eyePosition).Normalised();
        const _Vec3<T> xAxis = upVector.Cross(zAxis).Normalised();
        const _Vec3<T> yAxis = zAxis.Cross(xAxis);

        return _Mat4x4(
            xAxis.x,               yAxis.x,               zAxis.x,               static_cast<T>(0),
            xAxis.y,               yAxis.y,               zAxis.y,               static_cast<T>(0),
            xAxis.z,               yAxis.z,               zAxis.z,               static_cast<T>(0),
            -xAxis.Dot(eyePosition), -yAxis.Dot(eyePosition), -zAxis.Dot(eyePosition), static_cast<T>(1)
        );
    }

    template <std::floating_point T>
    _Mat4x4<T> _Mat4x4<T>::Perspective(
        T fovYRadians,
        T aspectRatio,
        T nearZ,
        T farZ
    ) noexcept
    {
        const T halfFov = fovYRadians * static_cast<T>(0.5);
        const T h = static_cast<T>(1) / std::tan(halfFov);
        const T w = h / aspectRatio;
        const T q = farZ / (farZ - nearZ);

        return _Mat4x4(
            w,                 static_cast<T>(0), static_cast<T>(0),  static_cast<T>(0),
            static_cast<T>(0), h,                 static_cast<T>(0),  static_cast<T>(0),
            static_cast<T>(0), static_cast<T>(0), q,                  static_cast<T>(1),
            static_cast<T>(0), static_cast<T>(0), -q * nearZ,         static_cast<T>(0)
        );
    }

    template <std::floating_point T>
    _Mat4x4<T> _Mat4x4<T>::Perspective(
        T left,
        T right,
        T bottom,
        T top,
        T nearZ,
        T farZ
    ) noexcept
    {
        const T invWidth  = static_cast<T>(1) / (right - left);
        const T invHeight = static_cast<T>(1) / (top - bottom);
        const T invRange  = static_cast<T>(1) / (farZ - nearZ);
        const T twoNear   = static_cast<T>(2) * nearZ;

        return _Mat4x4(
            twoNear * invWidth,         static_cast<T>(0),           static_cast<T>(0),         static_cast<T>(0),
            static_cast<T>(0),          twoNear * invHeight,         static_cast<T>(0),         static_cast<T>(0),
            -(right + left) * invWidth, -(top + bottom) * invHeight, farZ * invRange,           static_cast<T>(1),
            static_cast<T>(0),          static_cast<T>(0),           -nearZ * farZ * invRange,  static_cast<T>(0)
        );
    }

    template <std::floating_point T>
    _Mat4x4<T> _Mat4x4<T>::Orthographic(
        T width,
        T height,
        T nearZ,
        T farZ
    ) noexcept
    {
        const T invRange = static_cast<T>(1) / (farZ - nearZ);

        return _Mat4x4(
            static_cast<T>(2) / width, static_cast<T>(0),         static_cast<T>(0),    static_cast<T>(0),
            static_cast<T>(0),         static_cast<T>(2) / height,static_cast<T>(0),    static_cast<T>(0),
            static_cast<T>(0),         static_cast<T>(0),         invRange,             static_cast<T>(0),
            static_cast<T>(0),         static_cast<T>(0),         -nearZ * invRange,    static_cast<T>(1)
        );
    }

    template <std::floating_point T>
    _Mat4x4<float> _Mat4x4<T>::CreateCameraRelativeModelView(
        const _Mat4x4<double>& worldMatrix,
        const _Vec3<double>& cameraWorldPosition,
        const _Mat4x4<double>& viewMatrix
    ) noexcept
    {
        // 1. Evaluate world-space translation relative to the camera in 64-bit double precision
        const _Vec3<double> worldTranslation = worldMatrix.GetTranslation();
        const _Vec3<double> relativeTranslation = worldTranslation - cameraWorldPosition;

        // 2. Clone world matrix with camera-relative translation
        _Mat4x4<double> relativeWorld = worldMatrix;
        relativeWorld.SetTranslation(relativeTranslation);

        // 3. Extract purely rotational view matrix (camera placed at relative origin)
        _Mat4x4<double> rotationView = viewMatrix;
        rotationView.SetTranslation(_Vec3<double>::Zero());

        // 4. Combine into Model-View matrix in 64-bit precision
        const _Mat4x4<double> relativeModelView = relativeWorld * rotationView;

        // 5. Down-convert to 32-bit single precision for GPU submission without vertex jitter
        return _Mat4x4<float>(relativeModelView);
    }

    template <std::floating_point T>
    _Vec3<T> _Mat4x4<T>::TransformPoint(const _Vec3<T>& point) const noexcept
    {
        const T x = point.x * m[0][0] + point.y * m[1][0] + point.z * m[2][0] + m[3][0];
        const T y = point.x * m[0][1] + point.y * m[1][1] + point.z * m[2][1] + m[3][1];
        const T z = point.x * m[0][2] + point.y * m[1][2] + point.z * m[2][2] + m[3][2];
        const T w = point.x * m[0][3] + point.y * m[1][3] + point.z * m[2][3] + m[3][3];

        if (std::abs(w - static_cast<T>(1)) > DefaultEpsilon<T> && std::abs(w) > DefaultEpsilon<T>)
        {
            const T invW = static_cast<T>(1) / w;
            return _Vec3<T>(x * invW, y * invW, z * invW);
        }

        return _Vec3<T>(x, y, z);
    }

    template <std::floating_point T>
    _Vec3<T> _Mat4x4<T>::TransformDirection(const _Vec3<T>& dir) const noexcept
    {
        return _Vec3<T>(
            dir.x * m[0][0] + dir.y * m[1][0] + dir.z * m[2][0],
            dir.x * m[0][1] + dir.y * m[1][1] + dir.z * m[2][1],
            dir.x * m[0][2] + dir.y * m[1][2] + dir.z * m[2][2]
        );
    }

    template <std::floating_point T>
    _Mat4x4<T> _Mat4x4<T>::operator*(const _Mat4x4& rhs) const noexcept
    {
        _Mat4x4 res = Zero();
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                res.m[i][j] = m[i][0] * rhs.m[0][j]
                            + m[i][1] * rhs.m[1][j]
                            + m[i][2] * rhs.m[2][j]
                            + m[i][3] * rhs.m[3][j];
            }
        }
        return res;
    }

    template <std::floating_point T>
    bool _Mat4x4<T>::operator==(const _Mat4x4& rhs) const noexcept
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                if (!ApproximatelyEqual(m[i][j], rhs.m[i][j]))
                {
                    return false;
                }
            }
        }
        return true;
    }

    template struct _Mat4x4<float>;
    template struct _Mat4x4<double>;
}

