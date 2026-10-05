// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Character.h"
#include "Maths/Quat.h"

#include <algorithm>
#include <cmath>

namespace Sandbox3D::Engine
{
    using Maths::Vec3D;
    using Maths::Mat4x4D;
    using Maths::QuatD;

    Character::Character(std::string_view name)
        : Body(name)
        , m_camera(std::make_unique<Camera>(std::string(name) + "EyeCamera"))
    {
        constexpr double colliderRadius    = 0.35;
        constexpr double colliderCylHeight = 1.07;
        const Vec3D colliderOffset(0.0, 0.885, 0.0);
        SetCapsuleCollider(colliderRadius, colliderCylHeight, colliderOffset);
        SetLightChannels(LightChannel::Character);

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
        SetLightChannels(LightChannel::Character);

        SynchroniseTransforms();
    }

    void Character::Update(float deltaTime)
    {
        Body::Update(deltaTime);
        SynchroniseTransforms();
    }

    void Character::SetPosition(const Vec3D& position)
    {
        Body::SetPosition(position);
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

    void Character::SetHorizontalSpeed(const Vec3D& horizontalSpeed) noexcept
    {
        m_worldVelocity.x = horizontalSpeed.x;
        m_worldVelocity.z = horizontalSpeed.z;
    }

    Maths::Vec3D Character::GetHorizontalSpeed() const noexcept
    {
        return Vec3D(m_worldVelocity.x, 0.0, m_worldVelocity.z);
    }

    Maths::Vec3D Character::GetWalkForward() const noexcept
    {
        const QuatD bodyQuat = QuatD::FromAxisAngle(Vec3D::Up(), m_yaw);
        return bodyQuat.Rotate(Vec3D::Forward());
    }

    Maths::Vec3D Character::GetWalkRight() const noexcept
    {
        const QuatD bodyQuat = QuatD::FromAxisAngle(Vec3D::Up(), m_yaw);
        return bodyQuat.Rotate(Vec3D::Right());
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
        return GetPosition() + Maths::Vec3D(0.0, m_headPivotHeight, 0.0);
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
        const Vec3D pos = GetPosition();

        // Evaluate body rotation from yaw via quaternion and compose into world matrix for whole mesh / body
        const QuatD bodyQuat = QuatD::FromAxisAngle(Vec3D::Up(), m_yaw);
        m_worldMatrix = bodyQuat.ToRotationMatrix4x4();
        m_worldMatrix.SetTranslation(pos);
        SynchroniseRenderItem();

        // Compound local head pitch and body yaw rotations via quaternion product for head transform
        const Vec3D headPivotWorld = pos + Vec3D(0.0, m_headPivotHeight, 0.0);
        const QuatD headQuat       = QuatD::FromEulerAngles(m_pitch, m_yaw, 0.0);
        m_headTransform = headQuat.ToRotationMatrix4x4();
        m_headTransform.SetTranslation(headPivotWorld);

        // Synchronise eye camera transform
        SynchroniseCamera();
    }

    void Character::SynchroniseCamera() noexcept
    {
        if (!m_camera)
        {
            return;
        }

        const QuatD cameraQuat = QuatD::FromEulerAngles(m_pitch, m_yaw, 0.0);
        const Vec3D forward    = cameraQuat.Rotate(Vec3D::Forward());
        const Vec3D cameraUp   = cameraQuat.Rotate(Vec3D::Up());

        const Vec3D headPivotWorld = GetPosition() + Vec3D(0.0, m_headPivotHeight, 0.0);
        const Vec3D eyePosition    = headPivotWorld + forward * m_eyeDistance;
        const Vec3D target         = eyePosition + forward;

        m_camera->SetLookAt(eyePosition, target, cameraUp);
    }
}

