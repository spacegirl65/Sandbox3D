// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "SceneBuffers.hlsli"

Texture2D g_hdrSceneTexture : register(t0);
SamplerState g_samplerLinear : register(s1);

struct PostProcessVertexOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

// Generates a procedural full-screen triangle covering [-1, 1] clip space from SV_VertexID
PostProcessVertexOutput VSMain(uint vertexId : SV_VertexID)
{
    PostProcessVertexOutput output;

    // Vertex 0: (0, 0) -> (-1, 1)
    // Vertex 1: (2, 0) -> (3, 1)
    // Vertex 2: (0, 2) -> (-1, -3)
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.uv = uv;
    output.position = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);

    return output;
}

// Narkowicz ACES filmic tone reproduction curve fitting ACEScg to sRGB target
float3 AcesFilmicToneMapping(float3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;

    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// Tone mapping and colour grading post-processing stage
float4 PSMain(PostProcessVertexOutput input) : SV_TARGET
{
    // Sample resolved floating-point HDR scene radiance
    float4 hdrSample = g_hdrSceneTexture.Sample(g_samplerLinear, input.uv);
    float3 hdrColor = max(hdrSample.rgb, 0.0f);

    // Apply manual exposure multiplier and EV100 compensation
    float exposure = max(g_exposureParams.x, 0.0001f);
    if (g_exposureParams.y != 0.0f)
    {
        // Photometric EV100 to luminance scale conversion: 1.0 / (1.2 * 2^EV100)
        exposure *= 1.0f / (1.2f * exp2(g_exposureParams.y));
    }

    float3 exposedColor = hdrColor * exposure;

    // ACES filmic tone reproduction
    float3 ldrColor = AcesFilmicToneMapping(exposedColor);

    // Precise sRGB electro-optical transfer function (EOTF) conversion
    float3 srgbColor;
    srgbColor.r = (ldrColor.r <= 0.0031308f) ? (ldrColor.r * 12.92f) : (1.055f * pow(ldrColor.r, 1.0f / 2.4f) - 0.055f);
    srgbColor.g = (ldrColor.g <= 0.0031308f) ? (ldrColor.g * 12.92f) : (1.055f * pow(ldrColor.g, 1.0f / 2.4f) - 0.055f);
    srgbColor.b = (ldrColor.b <= 0.0031308f) ? (ldrColor.b * 12.92f) : (1.055f * pow(ldrColor.b, 1.0f / 2.4f) - 0.055f);

    return float4(srgbColor, hdrSample.a);
}
