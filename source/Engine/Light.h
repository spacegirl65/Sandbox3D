// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Base.h"
#include "Maths/Maths.h"
#include "Renderer/SceneConstantBuffer.h"

#include <memory>
#include <string_view>

namespace Sandbox3D::Engine
{
    using Maths::Vec3;
    using Maths::Vec3D;
    using Maths::Vec4;

    // Supported scene illumination light classification
    enum class LightType : uint32_t
    {
        Directional = 0,
        Point       = 1,
        Spot        = 2
    };

    // Encapsulates scene illumination (directional, point, or spot) as a Base scene entity
    class Light : public Base
    {
    public:
        // Default constructor
        explicit Light(std::string_view name = "DirectionalLight");

        // Directional light constructor
        Light(
            const Vec3& direction,
            const Vec4& color = Vec4(0.9f, 0.9f, 0.95f, 1.0f),
            const Vec4& ambient = Vec4(0.2f, 0.2f, 0.25f, 1.0f),
            std::string_view name = "DirectionalLight"
        );

        // Point light constructor
        Light(
            const Vec3D& position,
            float range,
            const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f),
            std::string_view name = "PointLight"
        );

        // Colour temperature directional light constructor (Kelvin)
        Light(
            const Vec3& direction,
            float kelvin,
            float intensity = 1.0f,
            std::string_view name = "DirectionalLight"
        );

        ~Light() override = default;

        // Static factory helpers
        static std::shared_ptr<Light> CreateDirectional(
            const Vec3& direction,
            const Vec4& color = Vec4(0.9f, 0.9f, 0.95f, 1.0f),
            std::string_view name = "DirectionalLight"
        );

        static std::shared_ptr<Light> CreateDirectionalWithTemperature(
            const Vec3& direction,
            float kelvin,
            float intensity = 1.0f,
            std::string_view name = "DirectionalLight"
        );

        static std::shared_ptr<Light> CreatePoint(
            const Vec3D& position,
            float range = 100.0f,
            const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f),
            std::string_view name = "PointLight"
        );

        static std::shared_ptr<Light> CreateSpot(
            const Vec3D& position,
            const Vec3& direction,
            float range = 100.0f,
            float innerAngleRad = 0.5f,
            float outerAngleRad = 0.7f,
            const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f),
            std::string_view name = "SpotLight"
        );

        // Core polymorphic update loop implementation
        void Update(float deltaTime) override;

        // Light type classification
        [[nodiscard]] LightType GetLightType() const noexcept { return m_type; }
        void SetLightType(LightType type) noexcept { m_type = type; }

        // Directional illumination configuration
        void SetDirection(const Vec3& direction) noexcept;
        [[nodiscard]] const Vec4& GetDirection() const noexcept { return m_direction; }
        [[nodiscard]] Vec3 GetDirection3() const noexcept { return Vec3(m_direction.x, m_direction.y, m_direction.z); }

        // Colour and intensity configuration
        void SetColor(const Vec4& color) noexcept { m_color = color; }
        [[nodiscard]] const Vec4& GetColor() const noexcept { return m_color; }
        void SetIntensity(float intensity) noexcept { m_intensity = intensity; }
        [[nodiscard]] float GetIntensity() const noexcept { return m_intensity; }

        // Colour temperature lighting (Kelvin)
        [[nodiscard]] static Vec4 ColourFromTemperature(float kelvin) noexcept;
        void SetColourTemperature(float kelvin) noexcept;
        [[nodiscard]] float GetColourTemperature() const noexcept { return m_colourTemperature; }

        // US English compatibility aliases
        [[nodiscard]] static Vec4 ColorFromTemperature(float kelvin) noexcept { return ColourFromTemperature(kelvin); }
        void SetColorTemperature(float kelvin) noexcept { SetColourTemperature(kelvin); }
        [[nodiscard]] float GetColorTemperature() const noexcept { return GetColourTemperature(); }

        // Range (point and spot lights)
        void SetRange(float range) noexcept { m_range = range; }
        [[nodiscard]] float GetRange() const noexcept { return m_range; }

        // Attenuation coefficients (constant, linear, quadratic)
        void SetAttenuation(float constant, float linear, float quadratic) noexcept
        {
            m_attenuation = Vec4(constant, linear, quadratic, m_attenuation.w);
        }
        [[nodiscard]] const Vec4& GetAttenuation() const noexcept { return m_attenuation; }

        // Spot cone angles (in radians)
        void SetSpotAngles(float innerAngleRad, float outerAngleRad) noexcept;
        [[nodiscard]] float GetInnerSpotAngle() const noexcept { return m_innerSpotAngle; }
        [[nodiscard]] float GetOuterSpotAngle() const noexcept { return m_outerSpotAngle; }

        // Ambient configuration
        void SetAmbient(const Vec4& ambient) noexcept { m_ambient = ambient; }
        [[nodiscard]] const Vec4& GetAmbient() const noexcept { return m_ambient; }

        void SetDirectionalLight(
            const Vec3& direction,
            const Vec4& color = Vec4(0.9f, 0.9f, 0.95f, 1.0f),
            const Vec4& ambient = Vec4(0.2f, 0.2f, 0.25f, 1.0f)
        ) noexcept;

        // Convert to Direct3D 12 GPU constant buffer light structure with camera-relative position
        [[nodiscard]] Renderer::GpuLight ToGpuLight(const Vec3D& cameraPosition = Vec3D::Zero()) const noexcept;

        // Non-renderable: emits 0 draw calls
        [[nodiscard]] bool IsRenderable() const noexcept override { return false; }

    private:
        LightType m_type{ LightType::Directional };
        Vec4      m_direction{ -0.577f, -0.707f, -0.408f, 0.0f };
        Vec4      m_color{ 0.9f, 0.9f, 0.95f, 1.0f };
        Vec4      m_ambient{ 0.2f, 0.2f, 0.25f, 1.0f };
        float     m_colourTemperature{ 6500.0f };
        float     m_intensity{ 1.0f };
        float     m_range{ 100.0f };
        Vec4      m_attenuation{ 1.0f, 0.09f, 0.032f, 0.0f };
        float     m_innerSpotAngle{ 0.5f };
        float     m_outerSpotAngle{ 0.7f };
    };
}

