// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "SceneBuffers.hlsli"

Texture2D g_hdrSceneTexture : register(t33);
Texture2D g_bloomTexture    : register(t34);
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

// Evaluates Rec.709 relative luminance for Karis firefly suppression and bright-pass thresholding
float EvaluatePostLuminance(float3 color)
{
    const float3 lumaWeights = float3(0.2126f, 0.7152f, 0.0722f);
    return dot(color, lumaWeights);
}

// Karis anti-firefly inverse-luminance weight preventing sub-pixel specular aliasing without crushing specular energy
float EvaluateKarisWeight(float3 color)
{
    const float karisDamping = 0.25f;
    const float luma = EvaluatePostLuminance(color);
    return 1.0f / (1.0f + max(luma, 0.0f) * karisDamping);
}

// Soft-knee quadratic bright-pass curve isolating HDR energy above threshold without hard clipping seams
float3 ApplyBloomSoftKnee(float3 color, float threshold, float softKnee)
{
    const float minBrightnessFloor = 0.00001f;
    const float brightness = max(color.r, max(color.g, color.b));
    const float knee = threshold * softKnee;
    const float soft = clamp(brightness - threshold + knee, 0.0f, 2.0f * knee);
    const float softContrib = (soft * soft) / max(4.0f * knee, minBrightnessFloor);
    const float contribution = max(softContrib, brightness - threshold) / max(brightness, minBrightnessFloor);
    return color * max(contribution, 0.0f);
}

// First bloom mip stage: Karis-weighted 13-tap downsample with soft-knee bright-pass extraction
float4 BloomPrefilterPS(PostProcessVertexOutput input) : SV_TARGET
{
    uint texWidth = 1u;
    uint texHeight = 1u;
    g_hdrSceneTexture.GetDimensions(texWidth, texHeight);
    const float2 texelSize = float2(1.0f / max(float(texWidth), 1.0f), 1.0f / max(float(texHeight), 1.0f));
    const float2 uv = input.uv;

    // 13-tap Jimenez downsample pattern covering five overlapping 2x2 boxes
    const float3 a = max(g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2(-2.0f,  2.0f) * texelSize).rgb, 0.0f);
    const float3 b = max(g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2( 0.0f,  2.0f) * texelSize).rgb, 0.0f);
    const float3 c = max(g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2( 2.0f,  2.0f) * texelSize).rgb, 0.0f);
    const float3 d = max(g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2(-2.0f,  0.0f) * texelSize).rgb, 0.0f);
    const float3 e = max(g_hdrSceneTexture.Sample(g_samplerLinear, uv).rgb, 0.0f);
    const float3 f = max(g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2( 2.0f,  0.0f) * texelSize).rgb, 0.0f);
    const float3 g = max(g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2(-2.0f, -2.0f) * texelSize).rgb, 0.0f);
    const float3 h = max(g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2( 0.0f, -2.0f) * texelSize).rgb, 0.0f);
    const float3 i = max(g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2( 2.0f, -2.0f) * texelSize).rgb, 0.0f);

    const float3 j = max(g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2(-1.0f,  1.0f) * texelSize).rgb, 0.0f);
    const float3 k = max(g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2( 1.0f,  1.0f) * texelSize).rgb, 0.0f);
    const float3 l = max(g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2(-1.0f, -1.0f) * texelSize).rgb, 0.0f);
    const float3 m = max(g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2( 1.0f, -1.0f) * texelSize).rgb, 0.0f);

    // Extract HDR radiance per 2x2 group before Karis weighting so dark space neighbours do not crush specular peaks
    const float bloomThreshold = 0.85f;
    const float bloomSoftKnee  = 0.50f;

    const float3 group0 = ApplyBloomSoftKnee((a + b + d + e) * 0.25f, bloomThreshold, bloomSoftKnee);
    const float3 group1 = ApplyBloomSoftKnee((b + c + e + f) * 0.25f, bloomThreshold, bloomSoftKnee);
    const float3 group2 = ApplyBloomSoftKnee((d + e + g + h) * 0.25f, bloomThreshold, bloomSoftKnee);
    const float3 group3 = ApplyBloomSoftKnee((e + f + h + i) * 0.25f, bloomThreshold, bloomSoftKnee);
    const float3 group4 = ApplyBloomSoftKnee((j + k + l + m) * 0.25f, bloomThreshold, bloomSoftKnee);

    const float w0 = EvaluateKarisWeight(group0) * 0.125f;
    const float w1 = EvaluateKarisWeight(group1) * 0.125f;
    const float w2 = EvaluateKarisWeight(group2) * 0.125f;
    const float w3 = EvaluateKarisWeight(group3) * 0.125f;
    const float w4 = EvaluateKarisWeight(group4) * 0.500f;

    const float weightSum = max(w0 + w1 + w2 + w3 + w4, 0.00001f);
    const float3 extracted = (group0 * w0 + group1 * w1 + group2 * w2 + group3 * w3 + group4 * w4) / weightSum;

    return float4(extracted, 1.0f);
}

// Progressive 13-tap Jimenez downsample for subsequent bloom pyramid mips
float4 BloomDownsamplePS(PostProcessVertexOutput input) : SV_TARGET
{
    uint texWidth = 1u;
    uint texHeight = 1u;
    g_hdrSceneTexture.GetDimensions(texWidth, texHeight);
    const float2 texelSize = float2(1.0f / max(float(texWidth), 1.0f), 1.0f / max(float(texHeight), 1.0f));
    const float2 uv = input.uv;

    const float3 a = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2(-2.0f,  2.0f) * texelSize).rgb;
    const float3 b = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2( 0.0f,  2.0f) * texelSize).rgb;
    const float3 c = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2( 2.0f,  2.0f) * texelSize).rgb;
    const float3 d = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2(-2.0f,  0.0f) * texelSize).rgb;
    const float3 e = g_hdrSceneTexture.Sample(g_samplerLinear, uv).rgb;
    const float3 f = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2( 2.0f,  0.0f) * texelSize).rgb;
    const float3 g = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2(-2.0f, -2.0f) * texelSize).rgb;
    const float3 h = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2( 0.0f, -2.0f) * texelSize).rgb;
    const float3 i = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2( 2.0f, -2.0f) * texelSize).rgb;

    const float3 j = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2(-1.0f,  1.0f) * texelSize).rgb;
    const float3 k = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2( 1.0f,  1.0f) * texelSize).rgb;
    const float3 l = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2(-1.0f, -1.0f) * texelSize).rgb;
    const float3 m = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2( 1.0f, -1.0f) * texelSize).rgb;

    const float3 downsampled =
        e * 0.125f +
        (a + c + g + i) * 0.03125f +
        (b + d + f + h) * 0.0625f +
        (j + k + l + m) * 0.125f;

    return float4(max(downsampled, 0.0f), 1.0f);
}

// 9-tap tent-filter upsample stage additively blended back up the bloom mip pyramid
float4 BloomUpsamplePS(PostProcessVertexOutput input) : SV_TARGET
{
    uint texWidth = 1u;
    uint texHeight = 1u;
    g_hdrSceneTexture.GetDimensions(texWidth, texHeight);
    const float filterRadius = 1.50f;
    const float2 step = float2(filterRadius / max(float(texWidth), 1.0f), filterRadius / max(float(texHeight), 1.0f));
    const float2 uv = input.uv;

    const float3 a = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2(-step.x,  step.y)).rgb;
    const float3 b = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2( 0.0f,    step.y)).rgb;
    const float3 c = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2( step.x,  step.y)).rgb;
    const float3 d = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2(-step.x,  0.0f)).rgb;
    const float3 e = g_hdrSceneTexture.Sample(g_samplerLinear, uv).rgb;
    const float3 f = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2( step.x,  0.0f)).rgb;
    const float3 g = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2(-step.x, -step.y)).rgb;
    const float3 h = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2( 0.0f,   -step.y)).rgb;
    const float3 i = g_hdrSceneTexture.Sample(g_samplerLinear, uv + float2( step.x, -step.y)).rgb;

    const float invTentWeightSum = 1.0f / 16.0f;
    const float3 upsampled = (e * 4.0f + (b + d + f + h) * 2.0f + (a + c + g + i)) * invTentWeightSum;

    return float4(max(upsampled, 0.0f), 1.0f);
}

// Tone mapping, HDR bloom compositing, and dithering post-processing stage matching Sky.hlsl / Standard.hlsl output
float4 PSMain(PostProcessVertexOutput input) : SV_TARGET
{
    // Sample resolved floating-point HDR scene radiance
    const float4 hdrSample = g_hdrSceneTexture.Sample(g_samplerLinear, input.uv);
    float3 hdrColor = max(hdrSample.rgb, 0.0f);

    // Composite multi-scale optical HDR bloom when enabled via g_adaptationParams.w
    const float bloomIntensity = max(g_adaptationParams.w, 0.0f);
    if (bloomIntensity > 0.0f)
    {
        const float3 bloomRadiance = max(g_bloomTexture.Sample(g_samplerLinear, input.uv).rgb, 0.0f);
        hdrColor += bloomRadiance * bloomIntensity;
    }

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
