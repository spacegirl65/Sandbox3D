// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "../Maths/Maths.h"

namespace Sandbox3D::Renderer
{
    using Maths::Vec3D;
    using Maths::Mat4x4;
    using Maths::Mat4x4D;

    // High-precision camera system implementing dual-tier coordinates and camera-relative MVP evaluation
    class Camera final
    {
    public:
        Camera();
        ~Camera() = default;

        // View configuration
        void SetLookAt(const Vec3D& position, const Vec3D& target, const Vec3D& up = Vec3D::Up()) noexcept;
        
        // Projection configuration
        void SetPerspective(float fovYRadians, float aspectRatio, float nearZ = 0.1f, float farZ = 1000.0f) noexcept;
        void SetOrthographic(float width, float height, float nearZ = 0.1f, float farZ = 1000.0f) noexcept;
        void UpdateAspectRatio(float aspectRatio) noexcept;

        // Accessors
        [[nodiscard]] const Vec3D& GetPosition() const noexcept { return m_position; }
        [[nodiscard]] const Vec3D& GetTarget() const noexcept { return m_target; }
        [[nodiscard]] const Vec3D& GetUp() const noexcept { return m_up; }
        [[nodiscard]] const Mat4x4D& GetViewMatrix() const noexcept { return m_viewMatrix; }
        [[nodiscard]] const Mat4x4& GetProjectionMatrix() const noexcept { return m_projectionMatrix; }

        // Evaluates camera-relative Model-View-Projection matrix adhering to Rules 19 & 20
        [[nodiscard]] Mat4x4 CalculateCameraRelativeMVP(const Mat4x4D& worldMatrix) const noexcept;

        // Bounding frustum extraction for spatial culling
        [[nodiscard]] Maths::BoundingFrustum GetFrustum() const noexcept;
        [[nodiscard]] Maths::BoundingFrustumD GetFrustumD() const noexcept;

    private:
        void RecalculateViewMatrix() noexcept;

    private:
        Vec3D    m_position{ 0.0, 0.0, -2.5 };
        Vec3D    m_target{ 0.0, 0.0, 0.0 };
        Vec3D    m_up{ 0.0, 1.0, 0.0 };

        Mat4x4D  m_viewMatrix{ Mat4x4D::Identity() };
        Mat4x4   m_projectionMatrix{ Mat4x4::Identity() };

        float    m_fovY{ Maths::ToRadians(60.0f) };
        float    m_aspectRatio{ 16.0f / 9.0f };
        float    m_nearZ{ 0.1f };
        float    m_farZ{ 1000.0f };
        bool     m_isPerspective{ true };
    };
}

