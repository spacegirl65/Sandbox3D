// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Character.h"

#include <algorithm>
#include <cmath>

namespace Sandbox3D::Engine
{
    using Maths::Vec3D;
    using Maths::Mat4x4D;

    Character::Character(std::string_view name)
        : Body(name)
        , m_camera(std::make_unique<Camera>(std::string(name) + "EyeCamera"))
    {
        constexpr double colliderRadius    = 0.35;
        constexpr double colliderCylHeight = 1.07;
        const Vec3D colliderOffset(0.0, 0.885, 0.0);
        SetCapsuleCollider(colliderRadius, colliderCylHeight, colliderOffset);

        SynchroniseTransforms();
    }

    Character::Character(std::shared_ptr<Renderer::Mesh> mesh, std::string_view name)
        : Body(std::move(mesh), name)
        , m_camera(std::make_unique<Camera>(std::string(name) + "EyeCamera"))
    {
        constexpr double colliderRadius    = 0.35;
        constexpr double colliderCylHeight = 1.07;
        const Vec3D colliderOffset(0.0, 0.885, 0.0);
        SetCapsuleCollider(colliderRadius, colliderCylHeight, colliderOffset);

        SynchroniseTransforms();
    }

    void Character::Update(float deltaTime)
    {
        Body::Update(deltaTime);
        SynchroniseTransforms();
    }

    void Character::SetPosition(const Vec3D& position)
    {
        m_position = position;
        SynchroniseTransforms();
    }

    void Character::SetWorldMatrix(const Mat4x4D& worldMatrix)
    {
        Body::SetWorldMatrix(worldMatrix);
        m_yaw = std::atan2(worldMatrix.m[2][0], worldMatrix.m[2][2]);
        SynchroniseTransforms();
    }

    void Character::SetHeadPivotHeight(double height) noexcept
    {
        m_headPivotHeight = height;
        SynchroniseTransforms();
    }

    void Character::SetEyeDistance(double distance) noexcept
    {
        m_eyeDistance = distance;
        SynchroniseTransforms();
    }

    void Character::SetEyeOffset(const Vec3D& offset) noexcept
    {
        m_eyeOffset = offset;
        m_headPivotHeight = offset.y;
        m_eyeDistance = std::abs(offset.z);
        SynchroniseTransforms();
    }

    void Character::SetYaw(double yaw) noexcept
    {
        m_yaw = std::fmod(yaw, Maths::TwoPi<double>);
        if (m_yaw < 0.0)
        {
            m_yaw += Maths::TwoPi<double>;
        }
        SynchroniseTransforms();
    }

    void Character::SetPitch(double pitch) noexcept
    {
        m_pitch = pitch;
        SynchroniseTransforms();
    }

    void Character::SetOrientation(double yaw, double pitch) noexcept
    {
        m_yaw = std::fmod(yaw, Maths::TwoPi<double>);
        if (m_yaw < 0.0)
        {
            m_yaw += Maths::TwoPi<double>;
        }
        m_pitch = pitch;
        SynchroniseTransforms();
    }

    void Character::Rotate(double deltaYaw, double deltaPitch) noexcept
    {
        SetOrientation(m_yaw + deltaYaw, m_pitch + deltaPitch);
    }

    Maths::Mat4x4D Character::GetEyeTransform() const noexcept
    {
        if (m_camera)
        {
            return m_camera->GetViewMatrix().Inverted();
        }
        return m_headTransform;
    }

    Maths::Vec3D Character::GetHeadPosition() const noexcept
    {
        return m_position + Maths::Vec3D(0.0, m_headPivotHeight, 0.0);
    }

    Maths::Vec3D Character::GetEyePosition() const noexcept
    {
        if (m_camera)
        {
            return m_camera->GetPosition();
        }
        return GetHeadPosition();
    }

    void Character::SynchroniseTransforms() noexcept
    {
        // 1. Whole mesh / body transform: rotate whole mesh about Y axis at character position
        const Mat4x4D bodyRotation    = Mat4x4D::RotationAroundY(m_yaw);
        const Mat4x4D bodyTranslation = Mat4x4D::Translation(m_position.x, m_position.y, m_position.z);
        m_worldMatrix = bodyRotation * bodyTranslation;
        SynchroniseRenderItem();

        // 2. Head transform: located on top of body, rotated by yaw and pitch
        const Vec3D headPivotWorld = m_position + Vec3D(0.0, m_headPivotHeight, 0.0);
        const Mat4x4D headRotation = Mat4x4D::RotationAroundX(m_pitch) * bodyRotation;
        m_headTransform = headRotation * Mat4x4D::Translation(headPivotWorld.x, headPivotWorld.y, headPivotWorld.z);

        // 3. Eye camera transform
        SynchroniseCamera();
    }

    void Character::SynchroniseCamera() noexcept
    {
        if (!m_camera)
        {
            return;
        }

        const double cosPitch = std::cos(m_pitch);
        const double sinPitch = std::sin(m_pitch);
        const double cosYaw   = std::cos(m_yaw);
        const double sinYaw   = std::sin(m_yaw);

        const Vec3D forward(
            cosPitch * sinYaw,
            sinPitch,
            cosPitch * cosYaw
        );

        const Vec3D headPivotWorld = m_position + Vec3D(0.0, m_headPivotHeight, 0.0);
        const Vec3D eyePosition    = headPivotWorld + forward * m_eyeDistance;

        const Vec3D cameraRight = Vec3D::Up().Cross(forward).Normalised();
        const Vec3D cameraUp    = forward.Cross(cameraRight).Normalised();
        const Vec3D target      = eyePosition + forward;

        m_camera->SetLookAt(eyePosition, target, cameraUp);
    }
}

