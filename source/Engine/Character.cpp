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

        SynchroniseCamera();
    }

    Character::Character(std::shared_ptr<Renderer::Mesh> mesh, std::string_view name)
        : Body(std::move(mesh), name)
        , m_camera(std::make_unique<Camera>(std::string(name) + "EyeCamera"))
    {
        constexpr double colliderRadius    = 0.35;
        constexpr double colliderCylHeight = 1.07;
        const Vec3D colliderOffset(0.0, 0.885, 0.0);
        SetCapsuleCollider(colliderRadius, colliderCylHeight, colliderOffset);

        SynchroniseCamera();
    }

    void Character::Update([[maybe_unused]] float deltaTime)
    {
        // Future player locomotion and physics will execute here
        SynchroniseCamera();
    }

    void Character::SetPosition(const Vec3D& position)
    {
        Body::SetPosition(position);
        SynchroniseCamera();
    }

    void Character::SetWorldMatrix(const Mat4x4D& worldMatrix)
    {
        Body::SetWorldMatrix(worldMatrix);
        SynchroniseCamera();
    }

    void Character::SetEyeOffset(const Vec3D& offset) noexcept
    {
        m_eyeOffset = offset;
        SynchroniseCamera();
    }

    void Character::SetYaw(double yaw) noexcept
    {
        m_yaw = yaw;
        SynchroniseCamera();
    }

    void Character::SetPitch(double pitch) noexcept
    {
        constexpr double maxPitch = 1.55; // ~89 degrees
        m_pitch = std::clamp(pitch, -maxPitch, maxPitch);
        SynchroniseCamera();
    }

    void Character::SynchroniseCamera() noexcept
    {
        if (!m_camera)
        {
            return;
        }

        const Vec3D eyePosition = m_position + m_eyeOffset;

        const double cosPitch = std::cos(m_pitch);
        const double sinPitch = std::sin(m_pitch);
        const double cosYaw   = std::cos(m_yaw);
        const double sinYaw   = std::sin(m_yaw);

        const Vec3D forward(
            cosPitch * sinYaw,
            sinPitch,
            cosPitch * cosYaw
        );

        const Vec3D cameraRight = Vec3D::Up().Cross(forward).Normalised();
        const Vec3D cameraUp    = forward.Cross(cameraRight).Normalised();
        const Vec3D target      = eyePosition + forward;

        m_camera->SetLookAt(eyePosition, target, cameraUp);
    }
}

