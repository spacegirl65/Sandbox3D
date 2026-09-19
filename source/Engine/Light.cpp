// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Light.h"

#include <algorithm>
#include <cmath>

namespace Sandbox3D::Engine
{
    Light::Light(std::string_view name)
        : Base(name)
    {
    }

    Light::Light(
        const Vec3& direction,
        const Vec4& color,
        const Vec4& ambient,
        std::string_view name
    )
        : Base(name)
        , m_type(LightType::Directional)
        , m_color(color)
        , m_ambient(ambient)
    {
        SetDirection(direction);
    }

    Light::Light(
        const Vec3D& position,
        float range,
        const Vec4& color,
        std::string_view name
    )
        : Base(name)
        , m_type(LightType::Point)
        , m_color(color)
        , m_range(range)
    {
        SetPosition(position);
    }

    Light::Light(
        const Vec3& direction,
        float kelvin,
        float intensity,
        std::string_view name
    )
        : Base(name)
        , m_type(LightType::Directional)
        , m_intensity(intensity)
    {
        SetDirection(direction);
        SetColourTemperature(kelvin);
    }

    std::shared_ptr<Light> Light::CreateDirectional(
        const Vec3& direction,
        const Vec4& color,
        std::string_view name
    )
    {
        auto light = std::make_shared<Light>(name);
        light->SetLightType(LightType::Directional);
        light->SetDirection(direction);
        light->SetColor(color);
        return light;
    }

    std::shared_ptr<Light> Light::CreateDirectionalWithTemperature(
        const Vec3& direction,
        float kelvin,
        float intensity,
        std::string_view name
    )
    {
        auto light = std::make_shared<Light>(name);
        light->SetLightType(LightType::Directional);
        light->SetDirection(direction);
        light->SetIntensity(intensity);
        light->SetColourTemperature(kelvin);
        return light;
    }

    std::shared_ptr<Light> Light::CreatePoint(
        const Vec3D& position,
        float range,
        const Vec4& color,
        std::string_view name
    )
    {
        auto light = std::make_shared<Light>(name);
        light->SetLightType(LightType::Point);
        light->SetPosition(position);
        light->SetRange(range);
        light->SetColor(color);
        return light;
    }

    std::shared_ptr<Light> Light::CreateSpot(
        const Vec3D& position,
        const Vec3& direction,
        float range,
        float innerAngleRad,
        float outerAngleRad,
        const Vec4& color,
        std::string_view name
    )
    {
        auto light = std::make_shared<Light>(name);
        light->SetLightType(LightType::Spot);
        light->SetPosition(position);
        light->SetDirection(direction);
        light->SetRange(range);
        light->SetSpotAngles(innerAngleRad, outerAngleRad);
        light->SetColor(color);
        return light;
    }

    void Light::Update([[maybe_unused]] float deltaTime)
    {
        // Light simulation update (e.g. animated sun orbit or day-night transitions)
    }

    void Light::SetDirection(const Vec3& direction) noexcept
    {
        const Vec3 normDir = direction.Normalised();
        m_direction = Vec4(normDir.x, normDir.y, normDir.z, 0.0f);
    }

    void Light::SetSpotAngles(float innerAngleRad, float outerAngleRad) noexcept
    {
        m_innerSpotAngle = innerAngleRad;
        m_outerSpotAngle = outerAngleRad;
    }

    Vec4 Light::ColourFromTemperature(float kelvin) noexcept
    {
        // Clamp colour temperature to Planckian blackbody radiation spectrum [1000K, 40000K]
        const float temp = std::clamp(kelvin, 1000.0f, 40000.0f) / 100.0f;

        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;

        // Evaluate red chromaticity component
        if (temp <= 66.0f)
        {
            red = 255.0f;
        }
        else
        {
            const float t = temp - 60.0f;
            red = 329.698727446f * std::pow(t, -0.1332047592f);
            red = std::clamp(red, 0.0f, 255.0f);
        }

        // Evaluate green chromaticity component
        if (temp <= 66.0f)
        {
            green = 99.4708025861f * std::log(temp) - 161.1195681661f;
            green = std::clamp(green, 0.0f, 255.0f);
        }
        else
        {
            const float t = temp - 60.0f;
            green = 288.1221695283f * std::pow(t, -0.0755148492f);
            green = std::clamp(green, 0.0f, 255.0f);
        }

        // Evaluate blue chromaticity component
        if (temp >= 66.0f)
        {
            blue = 255.0f;
        }
        else if (temp <= 19.0f)
        {
            blue = 0.0f;
        }
        else
        {
            const float t = temp - 10.0f;
            blue = 138.5177312231f * std::log(t) - 305.0447927307f;
            blue = std::clamp(blue, 0.0f, 255.0f);
        }

        return Vec4(red / 255.0f, green / 255.0f, blue / 255.0f, 1.0f);
    }

    void Light::SetColourTemperature(float kelvin) noexcept
    {
        m_colourTemperature = kelvin;
        m_color = ColourFromTemperature(kelvin);
    }

    void Light::SetDirectionalLight(
        const Vec3& direction,
        const Vec4& color,
        const Vec4& ambient
    ) noexcept
    {
        m_type = LightType::Directional;
        SetDirection(direction);
        m_color   = color;
        m_ambient = ambient;
    }

    Renderer::GpuLight Light::ToGpuLight(const Vec3D& cameraPosition) const noexcept
    {
        Renderer::GpuLight gpu{};

        switch (m_type)
        {
        case LightType::Directional:
        {
            gpu.position    = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            gpu.direction   = Vec4(m_direction.x, m_direction.y, m_direction.z, 0.0f); // w = 0 (Directional)
            gpu.color       = Vec4(m_color.x, m_color.y, m_color.z, m_intensity);
            gpu.attenuation = Vec4(1.0f, 0.0f, 0.0f, 0.0f);
            break;
        }
        case LightType::Point:
        {
            const Vec3D relPos = m_position - cameraPosition;
            gpu.position    = Vec4(
                static_cast<float>(relPos.x),
                static_cast<float>(relPos.y),
                static_cast<float>(relPos.z),
                m_range
            );
            gpu.direction   = Vec4(0.0f, 0.0f, 0.0f, 1.0f); // w = 1 (Point)
            gpu.color       = Vec4(m_color.x, m_color.y, m_color.z, m_intensity);
            gpu.attenuation = m_attenuation;
            break;
        }
        case LightType::Spot:
        {
            const Vec3D relPos = m_position - cameraPosition;
            gpu.position    = Vec4(
                static_cast<float>(relPos.x),
                static_cast<float>(relPos.y),
                static_cast<float>(relPos.z),
                m_range
            );
            gpu.direction   = Vec4(m_direction.x, m_direction.y, m_direction.z, 2.0f); // w = 2 (Spot)
            gpu.color       = Vec4(m_color.x, m_color.y, m_color.z, m_intensity);
            gpu.attenuation = Vec4(
                m_attenuation.x,
                m_attenuation.y,
                std::cos(m_innerSpotAngle),
                std::cos(m_outerSpotAngle)
            );
            break;
        }
        }

        return gpu;
    }
}

