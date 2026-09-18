// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Light.h"

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
        , m_color(color)
        , m_ambient(ambient)
    {
        SetDirection(direction);
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

    void Light::SetDirectionalLight(
        const Vec3& direction,
        const Vec4& color,
        const Vec4& ambient
    ) noexcept
    {
        SetDirection(direction);
        m_color   = color;
        m_ambient = ambient;
    }
}

