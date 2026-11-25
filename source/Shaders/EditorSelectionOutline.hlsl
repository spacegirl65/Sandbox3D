// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "SceneBuffers.hlsli"

Texture2D g_selectionMaskTexture : register(t41);
SamplerState g_samplerLinear : register(s1);

struct SelectionOutlineVertexOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

// Generates a full-screen triangle covering [-1, 1] clip space from SV_VertexID
SelectionOutlineVertexOutput VSMain(uint vertexId : SV_VertexID)
{
    SelectionOutlineVertexOutput output;

    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.uv = uv;
    output.position = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);

    return output;
}

// Writes solid silhouette mask (1.0) for the currently selected editor mesh
float4 SelectionMaskPS() : SV_TARGET
{
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}

// Composites a smooth, anti-aliased silhouette outline around the selected mesh pixels
float4 SelectionOutlinePS(SelectionOutlineVertexOutput input) : SV_TARGET
{
    uint texWidth = 1u;
    uint texHeight = 1u;
    g_selectionMaskTexture.GetDimensions(texWidth, texHeight);
    const float2 texelSize = float2(1.0f / max(float(texWidth), 1.0f), 1.0f / max(float(texHeight), 1.0f));
    const float2 uv = input.uv;

    const float center = g_selectionMaskTexture.SampleLevel(g_samplerLinear, uv, 0.0f).r;
    if (center >= 0.99f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // Smooth circular 16-tap neighbourhood search across 2 rings for sub-pixel anti-aliased edges
    float edgeSignal = 0.0f;
    [unroll]
    for (int ring = 1; ring <= 2; ++ring)
    {
        const float r = float(ring) * 1.15f;
        const float ringWeight = (ring == 1) ? 1.0f : 0.75f;
        [unroll]
        for (int i = 0; i < 8; ++i)
        {
            const float angle = float(i) * (6.2831853f / 8.0f) + float(ring) * 0.392699f;
            const float2 offset = float2(cos(angle), sin(angle)) * texelSize * r;
            const float s = g_selectionMaskTexture.SampleLevel(g_samplerLinear, uv + offset, 0.0f).r;
            edgeSignal = max(edgeSignal, s * ringWeight);
        }
    }

    const float alpha = saturate(edgeSignal * (1.0f - center) * 1.25f);
    if (alpha <= 0.01f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    return float4(0.105f, 0.310f, 0.630f, alpha);
}
