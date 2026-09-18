// Copyright © 2026 spacegirl65. All Rights Reserved.

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

struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
};

struct VertexOutput
{
    float4 position      : SV_POSITION;
    float3 worldNormal   : NORMAL;
    float4 color         : COLOR;
    float3 worldPosition : TEXCOORD0;
};

VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;

    // Row-vector multiplication convention (v * M) adhering to engine standards
    output.position = mul(float4(input.position, 1.0f), g_mvp);
    output.worldNormal = normalize(mul(float4(input.normal, 0.0f), g_world).xyz);
    output.worldPosition = mul(float4(input.position, 1.0f), g_world).xyz;
    output.color = input.color;

    return output;
}

