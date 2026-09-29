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

// Vertex shader stage
VertexOutput VSMain(VertexInput input, uint instanceId : SV_InstanceID)
{
    VertexOutput output;

    // Row-vector multiplication convention (v * M) adhering to engine standards
    if (g_isInstanced != 0)
    {
        InstanceData inst    = g_instances[instanceId];
        output.position      = mul(float4(input.position, 1.0f), inst.mvp);
        output.worldNormal   = normalize(mul(float4(input.normal, 0.0f), inst.world).xyz);
        output.worldPosition = mul(float4(input.position, 1.0f), inst.world).xyz;
        output.color         = input.color * inst.colorTint;
    }
    else
    {
        output.position      = mul(float4(input.position, 1.0f), g_mvp);
        output.worldNormal   = normalize(mul(float4(input.normal, 0.0f), g_world).xyz);
        output.worldPosition = mul(float4(input.position, 1.0f), g_world).xyz;
        output.color         = input.color;
    }

    return output;
}

// Pixel shader stage
float4 PSMain(VertexOutput input) : SV_TARGET
{
    return input.color;
}

