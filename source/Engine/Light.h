// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Base.h"
#include "Maths/Maths.h"

#include <string_view>

namespace Sandbox3D::Engine
{
    using Maths::Vec3;
    using Maths::Vec4;

    // Encapsulates scene directional and ambient illumination as a Base scene entity
    class Light : public Base
    {
    public:
        explicit Light(std::string_view name = "DirectionalLight");
        Light(
            const Vec3& direction,
            const Vec4& color = Vec4(0.9f, 0.9f, 0.95f, 1.0f),
            const Vec4& ambient = Vec4(0.2f, 0.2f, 0.25f, 1.0f),
            std::string_view name = "DirectionalLight"
        );
        ~Light() override = default;

        // Core polymorphic update loop implementation
        void Update(float deltaTime) override;

        // Directional illumination configuration
        void SetDirection(const Vec3& direction) noexcept;
        [[nodiscard]] const Vec4& GetDirection() const noexcept { return m_direction; }
        [[nodiscard]] Vec3 GetDirection3() const noexcept { return Vec3(m_direction.x, m_direction.y, m_direction.z); }

        // Light color & ambient configuration
        void SetColor(const Vec4& color) noexcept { m_color = color; }
        [[nodiscard]] const Vec4& GetColor() const noexcept { return m_color; }

        void SetAmbient(const Vec4& ambient) noexcept { m_ambient = ambient; }
        [[nodiscard]] const Vec4& GetAmbient() const noexcept { return m_ambient; }

        void SetDirectionalLight(
            const Vec3& direction,
            const Vec4& color = Vec4(0.9f, 0.9f, 0.95f, 1.0f),
            const Vec4& ambient = Vec4(0.2f, 0.2f, 0.25f, 1.0f)
        ) noexcept;

        // Non-renderable: emits 0 draw calls
        [[nodiscard]] bool IsRenderable() const noexcept override { return false; }

    private:
        Vec4 m_direction{ -0.577f, -0.707f, -0.408f, 0.0f };
        Vec4 m_color{ 0.9f, 0.9f, 0.95f, 1.0f };
        Vec4 m_ambient{ 0.2f, 0.2f, 0.25f, 1.0f };
    };
}

