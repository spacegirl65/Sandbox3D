// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include <cstdint>

namespace Sandbox3D::Engine
{
    // Lighting channel bitmask flags for selective object illumination
    enum LightChannel : uint32_t
    {
        None        = 0,
        Default     = 1 << 0, // General scene meshes and default objects
        Terrain     = 1 << 1, // Landscape and terrain geometry
        Character   = 1 << 2, // Player character and dynamic entities
        All         = 0x0FFFFFFF
    };

    inline constexpr uint32_t PackLightTypeAndChannels(uint32_t lightType, uint32_t channels) noexcept
    {
        return (lightType & 0xFu) | ((channels & 0x0FFFFFFFu) << 4u);
    }

    inline constexpr uint32_t UnpackLightType(uint32_t packed) noexcept
    {
        return packed & 0xFu;
    }

    inline constexpr uint32_t UnpackLightChannels(uint32_t packed) noexcept
    {
        return (packed >> 4u) & 0x0FFFFFFFu;
    }
}
