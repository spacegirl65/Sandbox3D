// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "SceneBuffers.hlsli"

Texture2D g_hdrSceneTexture : register(t33);
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

// Tone mapping and dithering post-processing stage matching Sky.hlsl / Standard.hlsl output
float4 PSMain(PostProcessVertexOutput input) : SV_TARGET
{
    // Sample resolved floating-point HDR scene radiance
    const float4 hdrSample = g_hdrSceneTexture.Sample(g_samplerLinear, input.uv);
    const float3 hdrColor = max(hdrSample.rgb, 0.0f);

    // Apply exposure and ACES filmic tone reproduction
    const float exposure = max(g_exposureParams.x, 0.0001f);
    const float3 exposedRadiance = hdrColor * exposure;
    const float3 tonemappedRadiance = AcesFilmicToneMapping(exposedRadiance);

    // Subtle +/-0.5 LSB triangular dither to eliminate 8-bit banding in dark space and solar glare gradients
    const float ditherSeedA = frac(sin(dot(input.position.xy, float2(12.9898f, 78.233f))) * 43758.5453f);
    const float ditherSeedB = frac(sin(dot(input.position.xy + float2(1.0f, 1.0f), float2(12.9898f, 78.233f))) * 43758.5453f);
    const float dither = (ditherSeedA + ditherSeedB - 1.0f) * (0.5f / 255.0f);
    const float hasSignal = step(0.0001f, dot(tonemappedRadiance, float3(1.0f, 1.0f, 1.0f)));

    return float4(saturate(tonemappedRadiance + dither * hasSignal), hdrSample.a);
}
