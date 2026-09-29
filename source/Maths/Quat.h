// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Mat3x3.h"
#include "Mat4x4.h"
#include "Vec3.h"

namespace Sandbox3D::Maths
{
    // Four-dimensional quaternion struct adhering to the Value Object design pattern
    // All directional orientations conform strictly to the Left-Handed (LH) coordinate standard
    template <std::floating_point T>
    struct _Quat
    {
        T x{ static_cast<T>(0) };
        T y{ static_cast<T>(0) };
        T z{ static_cast<T>(0) };
        T w{ static_cast<T>(1) };

        constexpr _Quat() noexcept = default;
        constexpr _Quat(T inX, T inY, T inZ, T inW) noexcept : x(inX), y(inY), z(inZ), w(inW) {}
        constexpr _Quat(const _Vec3<T>& vectorPart, T scalarPart) noexcept
            : x(vectorPart.x), y(vectorPart.y), z(vectorPart.z), w(scalarPart) {}

        // Multi-precision conversion constructor
        template <std::floating_point U>
        constexpr explicit _Quat(const _Quat<U>& other) noexcept
            : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)), z(static_cast<T>(other.z)), w(static_cast<T>(other.w)) {}

        // Magnitude and normalisation
        [[nodiscard]] T Length() const noexcept;
        [[nodiscard]] constexpr T LengthSquared() const noexcept { return x * x + y * y + z * z + w * w; }
        [[nodiscard]] _Quat Normalised() const noexcept;
        void Normalise() noexcept;

        // Geometric operations
        [[nodiscard]] constexpr _Quat Conjugate() const noexcept { return _Quat(-x, -y, -z, w); }
        [[nodiscard]] _Quat Inverted() const noexcept;
        void Invert() noexcept;

        [[nodiscard]] constexpr T Dot(const _Quat& other) const noexcept
        {
            return x * other.x + y * other.y + z * other.z + w * other.w;
        }

        // Spatial vector rotation
        [[nodiscard]] _Vec3<T> Rotate(const _Vec3<T>& vec) const noexcept;

        // Matrix and Euler conversions
        [[nodiscard]] _Mat3x3<T> ToRotationMatrix3x3() const noexcept;
        [[nodiscard]] _Mat4x4<T> ToRotationMatrix4x4() const noexcept;
        [[nodiscard]] _Vec3<T> ToEulerAngles() const noexcept;
        void ToAxisAngle(_Vec3<T>& outAxis, T& outAngleRadians) const noexcept;

        // Kinematic integration from angular velocity vector
        [[nodiscard]] _Quat Integrate(const _Vec3<T>& angularVelocity, T deltaTime) const noexcept;

        // Interpolation operations
        [[nodiscard]] static _Quat Slerp(const _Quat& a, const _Quat& b, T t) noexcept;
        [[nodiscard]] static _Quat Lerp(const _Quat& a, const _Quat& b, T t) noexcept;

        // Static factory methods
        [[nodiscard]] static constexpr _Quat Identity() noexcept { return _Quat(0, 0, 0, 1); }
        [[nodiscard]] static _Quat FromAxisAngle(const _Vec3<T>& axis, T angleRadians) noexcept;
        [[nodiscard]] static _Quat FromEulerAngles(T pitch, T yaw, T roll) noexcept;
        [[nodiscard]] static _Quat FromEuler(const _Vec3<T>& eulerRadians) noexcept;
        [[nodiscard]] static _Quat FromRotationMatrix(const _Mat3x3<T>& mat) noexcept;
        [[nodiscard]] static _Quat FromRotationMatrix(const _Mat4x4<T>& mat) noexcept;
        [[nodiscard]] static _Quat FromToRotation(const _Vec3<T>& from, const _Vec3<T>& to) noexcept;

        // Operators
        constexpr _Quat operator+(const _Quat& rhs) const noexcept { return _Quat(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w); }
        constexpr _Quat operator-(const _Quat& rhs) const noexcept { return _Quat(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w); }
        constexpr _Quat operator*(T scalar) const noexcept { return _Quat(x * scalar, y * scalar, z * scalar, w * scalar); }
        constexpr _Quat operator/(T scalar) const noexcept { return _Quat(x / scalar, y / scalar, z / scalar, w / scalar); }
        constexpr _Quat operator-() const noexcept { return _Quat(-x, -y, -z, -w); }

        _Quat operator*(const _Quat& rhs) const noexcept;
        _Quat& operator*=(const _Quat& rhs) noexcept;

        _Vec3<T> operator*(const _Vec3<T>& vec) const noexcept { return Rotate(vec); }

        [[nodiscard]] bool operator==(const _Quat& rhs) const noexcept;
        [[nodiscard]] bool operator!=(const _Quat& rhs) const noexcept { return !(*this == rhs); }
    };

    template <std::floating_point T>
    constexpr _Quat<T> operator*(T scalar, const _Quat<T>& q) noexcept
    {
        return q * scalar;
    }

    template <std::floating_point T>
    using Quat = _Quat<T>;

    using QuatF        = _Quat<float>;
    using QuatD        = _Quat<double>;
    using Quaternion   = _Quat<float>;
    using QuaternionD  = _Quat<double>;
}

