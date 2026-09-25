// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Maths/Maths.h"

namespace Sandbox3D::Renderer
{
    static constexpr uint32_t MaxLights = 16;

    // Direct3D 12 GPU light parameter structure (64 bytes, 16-byte aligned)
    struct GpuLight
    {
        Maths::Vec4 position;     // xyz = world position, w = range (point/spot)
        Maths::Vec4 direction;    // xyz = normalized direction, w = light type (0=Dir, 1=Point, 2=Spot)
        Maths::Vec4 color;        // rgb = light color, w = intensity
        Maths::Vec4 attenuation;  // x = constant, y = linear, z = quadratic, w = inner/outer spot cosine
    };

    // Direct3D 12 Scene Constant Buffer containing transformation matrices, multi-light array, ambient, and atmospheric fog data
    struct SceneConstantBuffer
    {
        Maths::Mat4x4 mvp;                    // combined model-view-projection matrix (64 bytes)
        Maths::Mat4x4 world;                  // world transformation matrix (64 bytes)
        Maths::Vec4   ambientColor;           // rgb = global ambient color, a = 1.0f (16 bytes)
        Maths::Vec4   fogColor;               // rgb = atmospheric fog color, a = fog strength (16 bytes)
        Maths::Vec4   fogParams;              // x = fogStart, y = fogEnd, z = fogDensity, w = reserved (16 bytes)
        uint32_t      lightCount{ 0 };        // number of active scene lights (4 bytes)
        uint32_t      padding[3]{ 0, 0, 0 };  // 16-byte alignment padding for HLSL cbuffer (12 bytes)
        GpuLight      lights[MaxLights]{};    // active lights array (16 * 64 = 1024 bytes)
    };

    static_assert(sizeof(GpuLight) == 64, "GpuLight must be exactly 64 bytes");
    static_assert(sizeof(SceneConstantBuffer) == 1216, "SceneConstantBuffer must be exactly 1216 bytes");

    using ModelViewProjectionBuffer = SceneConstantBuffer;
}

