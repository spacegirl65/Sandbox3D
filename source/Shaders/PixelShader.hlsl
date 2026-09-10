// Copyright © 2026 spacegirl65. All Rights Reserved.

cbuffer SceneConstantBuffer : register(b0)
{
    row_major float4x4 g_mvp;
    row_major float4x4 g_world;
    float4 g_lightDirection;
    float4 g_lightColor;
    float4 g_ambientColor;
};

struct PixelInput
{
    float4 position    : SV_POSITION;
    float3 worldNormal : NORMAL;
    float4 color       : COLOR;
};

float4 PSMain(PixelInput input) : SV_TARGET
{
    float3 N = normalize(input.worldNormal);
    float3 L = normalize(-g_lightDirection.xyz);
    float nDotL = max(dot(N, L), 0.0f);

    float3 diffuse = g_lightColor.rgb * nDotL;
    float3 ambient = g_ambientColor.rgb;
    float3 shadedColor = input.color.rgb * (ambient + diffuse);

    return float4(shadedColor, input.color.a);
}

