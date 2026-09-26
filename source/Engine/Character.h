// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Body.h"
#include "Camera.h"

#include <d3d12.h>
#include <memory>
#include <string_view>

namespace Sandbox3D::Engine
{
    // Character simulation entity representing the player in the 3D world
    class Character : public Body
    {
    public:
        explicit Character(std::string_view name = "Character");
        Character(ID3D12Device* device, std::string_view name = "Character");
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

        // Eye offset access and configuration
        void SetEyeOffset(const Maths::Vec3D& offset) noexcept;
        [[nodiscard]] const Maths::Vec3D& GetEyeOffset() const noexcept { return m_eyeOffset; }

        // Orientation access and configuration
        [[nodiscard]] double GetYaw() const noexcept { return m_yaw; }
        [[nodiscard]] double GetPitch() const noexcept { return m_pitch; }
        void SetYaw(double yaw) noexcept;
        void SetPitch(double pitch) noexcept;

        // Procedural player character mesh builder combining capsule body and spherical head
        [[nodiscard]] static std::shared_ptr<Renderer::Mesh> CreateCharacterMesh(
            ID3D12Device* device,
            float capsuleRadius = 0.35f,
            float capsuleCylinderHeight = 0.70f,
            float headRadius = 0.22f,
            const Maths::Vec4& bodyColor = Maths::Vec4(0.20f, 0.42f, 0.68f, 1.0f),
            const Maths::Vec4& headColor = Maths::Vec4(0.85f, 0.72f, 0.58f, 1.0f)
        );

    private:
        void SynchroniseCamera() noexcept;

    private:
        std::unique_ptr<Camera> m_camera;
        Maths::Vec3D            m_eyeOffset{ 0.0, 1.55, 0.0 };
        double                  m_yaw{ 0.0 };
        double                  m_pitch{ 0.0 };
    };
}

