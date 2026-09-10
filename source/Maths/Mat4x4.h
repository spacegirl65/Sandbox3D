// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Mat3x3.h"

namespace Sandbox3D::Maths
{
    template <std::floating_point T>
    struct _Vec4;
    // 4x4 Matrix struct for 3D affine transformations, Left-Handed view/projection, and dual-tier camera-relative rendering
    // Stored row-major in memory; vector transformations adhere to row-vector convention (v * M)
    template <std::floating_point T>
    struct _Mat4x4
    {
        // Elements stored row-major: m[row][column]
        T m[4][4]{
            { 1, 0, 0, 0 },
            { 0, 1, 0, 0 },
            { 0, 0, 1, 0 },
            { 0, 0, 0, 1 }
        };

        constexpr _Mat4x4() noexcept = default;

        constexpr _Mat4x4(
            T m00, T m01, T m02, T m03,
            T m10, T m11, T m12, T m13,
            T m20, T m21, T m22, T m23,
            T m30, T m31, T m32, T m33
        ) noexcept : m{
            { m00, m01, m02, m03 },
            { m10, m11, m12, m13 },
            { m20, m21, m22, m23 },
            { m30, m31, m32, m33 }
        } {}

        // Multi-precision conversion constructor
        template <std::floating_point U>
        constexpr explicit _Mat4x4(const _Mat4x4<U>& other) noexcept
            : m{
                { static_cast<T>(other.m[0][0]), static_cast<T>(other.m[0][1]), static_cast<T>(other.m[0][2]), static_cast<T>(other.m[0][3]) },
                { static_cast<T>(other.m[1][0]), static_cast<T>(other.m[1][1]), static_cast<T>(other.m[1][2]), static_cast<T>(other.m[1][3]) },
                { static_cast<T>(other.m[2][0]), static_cast<T>(other.m[2][1]), static_cast<T>(other.m[2][2]), static_cast<T>(other.m[2][3]) },
                { static_cast<T>(other.m[3][0]), static_cast<T>(other.m[3][1]), static_cast<T>(other.m[3][2]), static_cast<T>(other.m[3][3]) }
            } {}

        // Algebraic properties and transformations
        [[nodiscard]] _Mat4x4 Transposed() const noexcept;
        void Transpose() noexcept;
        [[nodiscard]] T Determinant() const noexcept;
        [[nodiscard]] _Mat4x4 Inverted() const noexcept;
        bool Invert() noexcept;

        // Matrix decomposition and translation accessors
        [[nodiscard]] _Vec3<T> GetTranslation() const noexcept;
        void SetTranslation(const _Vec3<T>& translation) noexcept;
        [[nodiscard]] _Mat3x3<T> GetRotationMatrix() const noexcept;
        [[nodiscard]] _Vec3<T> GetEulerAngles() const noexcept;
        [[nodiscard]] _Vec3<T> eulerAngles() const noexcept { return GetEulerAngles(); }

        // Static factory affine transformations (Left-Handed)
        [[nodiscard]] static constexpr _Mat4x4 Identity() noexcept { return _Mat4x4(); }
        [[nodiscard]] static constexpr _Mat4x4 Zero() noexcept;
        [[nodiscard]] static _Mat4x4 Translation(T x, T y, T z) noexcept;
        [[nodiscard]] static _Mat4x4 Translation(const _Vec3<T>& translation) noexcept;
        [[nodiscard]] static _Mat4x4 RotationAroundX(T radians) noexcept;
        [[nodiscard]] static _Mat4x4 RotationAroundY(T radians) noexcept;
        [[nodiscard]] static _Mat4x4 RotationAroundZ(T radians) noexcept;
        [[nodiscard]] static _Mat4x4 RotationYawPitchRoll(T yaw, T pitch, T roll) noexcept;
        [[nodiscard]] static _Mat4x4 RotationAroundAxis(const _Vec3<T>& axis, T radians) noexcept;
        [[nodiscard]] static _Mat4x4 Scale(T sx, T sy, T sz) noexcept;
        [[nodiscard]] static _Mat4x4 Scale(const _Vec3<T>& scale) noexcept;

        // World matrix transformations
        [[nodiscard]] static _Mat4x4 World(const _Vec3<T>& position) noexcept;
        [[nodiscard]] static _Mat4x4 World(const _Vec3<T>& position, const _Vec3<T>& eulerAngles) noexcept;
        [[nodiscard]] static _Mat4x4 World(
            const _Vec3<T>& position,
            const _Vec3<T>& forwardVector,
            const _Vec3<T>& upVector
        ) noexcept;

        // Camera & Projection systems (DirectX Left-Handed [0, 1] Clip Depth)
        [[nodiscard]] static _Mat4x4 LookAt(
            const _Vec3<T>& eyePosition,
            const _Vec3<T>& targetPosition,
            const _Vec3<T>& upVector
        ) noexcept;

        [[nodiscard]] static _Mat4x4 Perspective(
            T fovYRadians,
            T aspectRatio,
            T nearZ,
            T farZ
        ) noexcept;

        [[nodiscard]] static _Mat4x4 Perspective(
            T left,
            T right,
            T bottom,
            T top,
            T nearZ,
            T farZ
        ) noexcept;

        [[nodiscard]] static _Mat4x4 Orthographic(
            T width,
            T height,
            T nearZ,
            T farZ
        ) noexcept;

        // Dual-tier camera-relative model-view generator
        // Evaluates relative world translation in high precision (double)
        // before converting to single-precision (float) for GPU constant buffers
        [[nodiscard]] static _Mat4x4<float> CreateCameraRelativeModelView(
            const _Mat4x4<double>& worldMatrix,
            const _Vec3<double>& cameraWorldPosition,
            const _Mat4x4<double>& viewMatrix
        ) noexcept;

        // Vector transformations
        [[nodiscard]] _Vec3<T> TransformPoint(const _Vec3<T>& point) const noexcept;    // affine w=1
        [[nodiscard]] _Vec3<T> TransformDirection(const _Vec3<T>& dir) const noexcept; // linear w=0
        [[nodiscard]] _Vec4<T> TransformVector(const _Vec4<T>& v) const noexcept;

        // Operators
        [[nodiscard]] _Mat4x4 operator*(const _Mat4x4& rhs) const noexcept;
        [[nodiscard]] _Vec3<T> operator*(const _Vec3<T>& rhs) const noexcept { return TransformPoint(rhs); }
        [[nodiscard]] _Vec4<T> operator*(const _Vec4<T>& rhs) const noexcept { return TransformVector(rhs); }

        bool operator==(const _Mat4x4& rhs) const noexcept;
        bool operator!=(const _Mat4x4& rhs) const noexcept { return !(*this == rhs); }
    };

    using Mat4x4  = _Mat4x4<float>;
    using Mat4x4D = _Mat4x4<double>;
}

