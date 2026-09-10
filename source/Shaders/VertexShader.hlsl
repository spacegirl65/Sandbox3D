// Copyright © 2026 spacegirl65. All Rights Reserved.

cbuffer SceneConstantBuffer : register(b0)
{
    row_major float4x4 g_mvp;
    row_major float4x4 g_world;
    float4 g_lightDirection;
    float4 g_lightColor;
    float4 g_ambientColor;
};

struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
};

struct VertexOutput
{
    float4 position    : SV_POSITION;
    float3 worldNormal : NORMAL;
    float4 color       : COLOR;
};

VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;

    // Row-vector multiplication convention (v * M) adhering to engine standards
    output.position = mul(float4(input.position, 1.0f), g_mvp);
    output.worldNormal = normalize(mul(float4(input.normal, 0.0f), g_world).xyz);
    output.color = input.color;

    return output;
}

