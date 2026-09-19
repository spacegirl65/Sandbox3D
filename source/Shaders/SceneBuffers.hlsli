// Copyright © 2026 spacegirl65. All Rights Reserved.

#ifndef SCENE_BUFFERS_HLSLI
#define SCENE_BUFFERS_HLSLI

struct LightData
{
    float4 position;    // xyz = world position, w = range
    float4 direction;   // xyz = normalized direction, w = type (0=Dir, 1=Point, 2=Spot)
    float4 color;       // rgb = light color, w = intensity
    float4 attenuation; // x = constant, y = linear, z = quadratic, w = inner/outer spot cosine
};

cbuffer SceneConstantBuffer : register(b0)
{
    row_major float4x4 g_mvp;
    row_major float4x4 g_world;
    float4             g_ambientColor;
    uint               g_lightCount;
    uint3              g_lightPadding;
    LightData          g_lights[16];
};

#endif // SCENE_BUFFERS_HLSLI

