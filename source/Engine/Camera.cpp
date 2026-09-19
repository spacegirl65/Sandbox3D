// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Camera.h"

namespace Sandbox3D::Engine
{
    Camera::Camera(std::string_view name)
        : Base(name)
    {
        m_position = Vec3D(0.0, 0.0, -2.5);
        RecalculateViewMatrix();
        SetPerspective(m_fovY, m_aspectRatio, m_nearZ, m_farZ);
    }

    void Camera::Update([[maybe_unused]] float deltaTime)
    {
        RecalculateViewMatrix();
    }

    void Camera::SetPosition(const Vec3D& position)
    {
        m_position = position;
        RecalculateViewMatrix();
    }

    void Camera::SetLookAt(const Vec3D& position, const Vec3D& target, const Vec3D& up) noexcept
    {
        m_position = position;
        m_target   = target;
        m_up       = up;
        RecalculateViewMatrix();
    }

    void Camera::SetPerspective(float fovYRadians, float aspectRatio, float nearZ, float farZ) noexcept
    {
        m_fovY          = fovYRadians;
        m_aspectRatio   = aspectRatio;
        m_nearZ         = nearZ;
        m_farZ          = farZ;
        m_isPerspective = true;

        m_projectionMatrix = Mat4x4::Perspective(m_fovY, m_aspectRatio, m_nearZ, m_farZ);
    }

    void Camera::SetOrthographic(float width, float height, float nearZ, float farZ) noexcept
    {
        m_nearZ         = nearZ;
        m_farZ          = farZ;
        m_isPerspective = false;

        m_projectionMatrix = Mat4x4::Orthographic(width, height, m_nearZ, m_farZ);
    }

    void Camera::UpdateAspectRatio(float aspectRatio) noexcept
    {
        if (aspectRatio > 0.0f)
        {
            m_aspectRatio = aspectRatio;
            if (m_isPerspective)
            {
                m_projectionMatrix = Mat4x4::Perspective(m_fovY, m_aspectRatio, m_nearZ, m_farZ);
            }
        }
    }

    Mat4x4 Camera::CalculateCameraRelativeMVP(const Mat4x4D& worldMatrix) const noexcept
    {
        // Evaluate world-space translation relative to the camera in 64-bit double precision,
        // down-converting to 32-bit single precision for GPU constant buffer submission
        const Mat4x4 modelView = Mat4x4D::CreateCameraRelativeModelView(worldMatrix, m_position, m_viewMatrix);

        // Compose with projection matrix (row-vector convention: v * (MV * P))
        return modelView * m_projectionMatrix;
    }

    Mat4x4 Camera::CalculateCameraRelativeWorld(const Mat4x4D& worldMatrix) const noexcept
    {
        // Evaluate world-space translation relative to the camera in 64-bit double precision,
        // down-converting to 32-bit single precision for GPU constant buffer submission
        return Mat4x4D::CreateCameraRelativeWorld(worldMatrix, m_position);
    }

    void Camera::RecalculateViewMatrix() noexcept
    {
        m_viewMatrix = Mat4x4D::LookAt(m_position, m_target, m_up);
    }

    Maths::BoundingFrustum Camera::GetFrustum() const noexcept
    {
        const Mat4x4 view32(m_viewMatrix);
        const Mat4x4 viewProj = view32 * m_projectionMatrix;
        return Maths::BoundingFrustum(viewProj);
    }

    Maths::BoundingFrustumD Camera::GetFrustumD() const noexcept
    {
        const Mat4x4D proj64(m_projectionMatrix);
        const Mat4x4D viewProj = m_viewMatrix * proj64;
        return Maths::BoundingFrustumD(viewProj);
    }
}

