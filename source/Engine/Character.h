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
        // Orientation limits: looking no more than 90 degrees down or 60 degrees up
        static constexpr double MinPitch = -Maths::DegToRad<double> * 89.9; // ~90 degrees down
        static constexpr double MaxPitch =  Maths::DegToRad<double> * 60.0; // 60 degrees up

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

        // Spatial transform overrides ensuring eye camera synchronisation
        void SetPosition(const Maths::Vec3D& position) override;
        void SetWorldMatrix(const Maths::Mat4x4D& worldMatrix) override;

        // Eye camera access
        [[nodiscard]] Camera* GetCamera() noexcept { return m_camera.get(); }
        [[nodiscard]] const Camera* GetCamera() const noexcept { return m_camera.get(); }

        // Head and eye geometry configuration
        void SetHeadPivotHeight(double height) noexcept;
        [[nodiscard]] double GetHeadPivotHeight() const noexcept { return m_headPivotHeight; }

        void SetEyeDistance(double distance) noexcept;
        [[nodiscard]] double GetEyeDistance() const noexcept { return m_eyeDistance; }

        // Eye offset access and configuration (for compatibility)
        void SetEyeOffset(const Maths::Vec3D& offset) noexcept;
        [[nodiscard]] const Maths::Vec3D& GetEyeOffset() const noexcept { return m_eyeOffset; }

        // Orientation access and configuration
        [[nodiscard]] double GetYaw() const noexcept { return m_yaw; }
        [[nodiscard]] double GetPitch() const noexcept { return m_pitch; }
        void SetYaw(double yaw) noexcept;
        void SetPitch(double pitch) noexcept;
        void SetOrientation(double yaw, double pitch) noexcept;
        void Rotate(double deltaYaw, double deltaPitch) noexcept;

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

