// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "SceneBuffers.hlsli"

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

