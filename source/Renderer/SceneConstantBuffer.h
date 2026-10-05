// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Maths/Maths.h"

#include "Engine/LightChannel.h"

namespace Sandbox3D::Renderer
{
    using Engine::LightChannel;
    using Engine::PackLightTypeAndChannels;
    using Engine::UnpackLightType;
    using Engine::UnpackLightChannels;

    static constexpr uint32_t MaxLights = 16;

    // Direct3D 12 GPU light parameter structure (64 bytes, 16-byte aligned)
    struct GpuLight
    {
        Maths::Vec4 position;     // Position in xyz, and range in w (point/spot)
        Maths::Vec4 direction;    // Direction vector in xyz, and packed type/channels in w
        Maths::Vec4 color;        // Colour in rgb, and intensity in w
        Maths::Vec4 attenuation;  // Attenuation factors (constant, linear, quadratic) in xyz, and spot cosine in w
    };

    // Direct3D 12 Scene Constant Buffer containing transformation matrices, multi-light array, ambient, atmospheric fog, cascaded shadow, and physical atmosphere data
    static constexpr uint32_t MaxCascades = 4;

    struct SceneConstantBuffer
    {
        Maths::Mat4x4 mvp;                                      // Combined model-view-projection matrix (64 bytes)
        Maths::Mat4x4 world;                                    // World transformation matrix (64 bytes)
        Maths::Vec4   ambientColor;                             // Ambient colour in rgb, and alpha in a (16 bytes)
        Maths::Vec4   fogColor;                                 // Atmospheric fog colour in rgb, and fog strength in a (16 bytes)
        Maths::Vec4   fogParams;                                // Fog parameters (fogStart, fogEnd, fogDensity, reserved) (16 bytes)
        uint32_t      lightCount{0};                            // Number of active scene lights (4 bytes)
        uint32_t      isInstanced{0};                           // Flag indicating 1 if instance buffer is active, or 0 for direct draw (4 bytes)
        uint32_t      objectLightChannels{LightChannel::All};   // Active lighting channel mask for rendered object (4 bytes)
        uint32_t      padding{0};                               // Alignment padding for HLSL constant buffer (4 bytes)
        GpuLight      lights[MaxLights]{};                      // Active lights array (16 * 64 = 1024 bytes)
        Maths::Mat4x4 shadowViewProj[MaxCascades]{};            // Cascade light view-projection matrices (4 * 64 = 256 bytes)
        Maths::Vec4   cascadeSplits{};                          // Cascade split distances (16 bytes)
        Maths::Vec4   shadowParams{};                           // Shadow parameters (mapSize, invMapSize, biasScale, cascadeCount) (16 bytes)
        Maths::Vec4   rayleighParams{};                         // Rayleigh scattering parameters: beta_R in rgb, and H_R in w (16 bytes)
        Maths::Vec4   mieParams{};                              // Mie scattering parameters: beta_M in rgb, and H_M in w (16 bytes)
        Maths::Vec4   ozoneParams{};                            // Ozone absorption parameters: beta_ozone in rgb, and g in w (16 bytes)
        Maths::Vec4   atmosphereParams{};                       // Atmosphere geometry: R_planet, R_atm, sunLux, ozoneCenter (16 bytes)
    };

    static_assert(sizeof(GpuLight) == 64, "GpuLight must be exactly 64 bytes");
    static_assert(sizeof(SceneConstantBuffer) == 1568, "SceneConstantBuffer must be exactly 1568 bytes");

    using ModelViewProjectionBuffer = SceneConstantBuffer;
}

