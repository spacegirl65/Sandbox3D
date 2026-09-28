// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Body.h"
#include "Camera.h"
#include "Maths/MathsCommon.h"
#include <memory>
#include <numbers>
#include <string_view>

namespace Sandbox3D::Engine
{
    // Character simulation entity representing the player in the 3D world
    class Character : public Body
    {
    public:
        explicit Character(std::string_view name = "Character");
        Character(std::shared_ptr<Renderer::Mesh> mesh, std::string_view name = "Character");
        ~Character() override = default;

        Character(const Character&) = delete;
        Character& operator=(const Character&) = delete;
        Character(Character&&) noexcept = default;
        Character& operator=(Character&&) noexcept = default;

        // Core polymorphic update loop
        void Update(float deltaTime) override;

        // Spatial transform overrides for eye camera synchronisation
        void SetPosition(const Maths::Vec3D& position) override;
        void SetWorldMatrix(const Maths::Mat4x4D& worldMatrix) override;

        [[nodiscard]] Camera* GetEyeCamera() noexcept { return m_camera.get(); }
        [[nodiscard]] const Camera* GetEyeCamera() const noexcept { return m_camera.get(); }

        void SetHeadPivotHeight(double height) noexcept;
        [[nodiscard]] double GetHeadPivotHeight() const noexcept { return m_headPivotHeight; }

        void SetEyeDistance(double distance) noexcept;
        [[nodiscard]] double GetEyeDistance() const noexcept { return m_eyeDistance; }

        void SetEyeOffset(const Maths::Vec3D& offset) noexcept;
        [[nodiscard]] const Maths::Vec3D& GetEyeOffset() const noexcept { return m_eyeOffset; }

        [[nodiscard]] double GetYaw() const noexcept { return m_yaw; }
        [[nodiscard]] double GetPitch() const noexcept { return m_pitch; }
        void SetYaw(double yaw) noexcept;
        void SetPitch(double pitch) noexcept;
        void SetOrientation(double yaw, double pitch) noexcept;
        void Rotate(double deltaYaw, double deltaPitch) noexcept;

        // Locomotion and horizontal speed
        void SetHorizontalSpeed(const Maths::Vec3D& horizontalSpeed) noexcept;
        [[nodiscard]] Maths::Vec3D GetHorizontalSpeed() const noexcept;
        [[nodiscard]] Maths::Vec3D GetWalkForward() const noexcept;
        [[nodiscard]] Maths::Vec3D GetWalkRight() const noexcept;

        // Transform accessors for body, head, and eyes
        [[nodiscard]] const Maths::Mat4x4D& GetBodyTransform() const noexcept { return m_worldMatrix; }
        [[nodiscard]] const Maths::Mat4x4D& GetHeadTransform() const noexcept { return m_headTransform; }
        [[nodiscard]] Maths::Mat4x4D GetEyeTransform() const noexcept;
        [[nodiscard]] Maths::Vec3D GetHeadPosition() const noexcept;
        [[nodiscard]] Maths::Vec3D GetEyePosition() const noexcept;

    private:
        void SynchroniseTransforms() noexcept;
        void SynchroniseCamera() noexcept;

    private:
        std::unique_ptr<Camera> m_camera;
        Maths::Mat4x4D          m_headTransform{ Maths::Mat4x4D::Identity() };
        Maths::Vec3D            m_eyeOffset{ 0.0, 1.58, -0.25 };
        double                  m_headPivotHeight{ 1.58 };
        double                  m_eyeDistance{ 0.25 };
        double                  m_yaw{ std::numbers::pi };
        double                  m_pitch{ 0.0 };
    };
}

