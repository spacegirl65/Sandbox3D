// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Camera.h"

namespace Sandbox3D::Renderer
{
    Camera::Camera()
    {
        RecalculateViewMatrix();
        SetPerspective(m_fovY, m_aspectRatio, m_nearZ, m_farZ);
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
        // 1. Evaluate world-space translation relative to the camera in 64-bit double precision
        // 2. Down-convert to 32-bit single precision for GPU constant buffer submission (Rule 19 & 20)
        const Mat4x4 modelView = Mat4x4D::CreateCameraRelativeModelView(worldMatrix, m_position, m_viewMatrix);

        // 3. Compose with projection matrix (row-vector convention: v * (MV * P))
        return modelView * m_projectionMatrix;
    }

    void Camera::RecalculateViewMatrix() noexcept
    {
        m_viewMatrix = Mat4x4D::LookAt(m_position, m_target, m_up);
    }
}

