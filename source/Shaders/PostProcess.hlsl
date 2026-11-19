// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "SceneBuffers.hlsli"

Texture2D g_hdrSceneTexture : register(t33);
Texture2D g_bloomTexture    : register(t34);
Texture2D g_bloomMip1       : register(t35);
Texture2D g_bloomMip2       : register(t36);
Texture2D g_bloomMip3       : register(t37);
Texture2D g_bloomMip4       : register(t38);
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

// Evaluates physical specular optical glare: wide veiling scatter, anamorphic streak, 6-blade aperture diffraction, and internal lens ghosts
float3 EvaluateOpticalGlare(float2 uv, float aspectRatio)
{
    const float glareThreshold = 0.55f;
    const float glareSoftKnee  = 0.40f;

    // Wide optical veiling scatter from the coarsest bloom pyramid mip (1/32 resolution)
    const float3 wideBloom = max(g_bloomMip4.Sample(g_samplerLinear, uv).rgb, 0.0f);
    const float3 veilingGlare = ApplyBloomSoftKnee(wideBloom, 0.20f, 0.50f) * 0.35f;

    // Horizontal anamorphic optical streak sampled from pre-blurred mip 3 (1/16 res) with overlapping taps to avoid beading
    float3 anamorphicStreak = float3(0.0f, 0.0f, 0.0f);
    const float streakStepX = 0.0065f;
    const float streakDecay = 0.72f;
    float streakWeight = 0.36f;

    [unroll]
    for (int tap = 1; tap <= 10; ++tap)
    {
        const float offsetX = float(tap) * streakStepX;
        const float3 samplePos = max(g_bloomMip3.Sample(g_samplerLinear, saturate(uv + float2( offsetX, 0.0f))).rgb, 0.0f);
        const float3 sampleNeg = max(g_bloomMip3.Sample(g_samplerLinear, saturate(uv + float2(-offsetX, 0.0f))).rgb, 0.0f);
        const float3 gated = ApplyBloomSoftKnee(samplePos, glareThreshold, glareSoftKnee)
                           + ApplyBloomSoftKnee(sampleNeg, glareThreshold, glareSoftKnee);

        // Subtle multi-coated optical tint shifting gently toward cool cyan-blue on outer taps
        const float tintFactor = saturate(float(tap) / 10.0f);
        const float3 coatingTint = lerp(float3(1.0f, 1.0f, 1.0f), float3(0.76f, 0.90f, 1.08f), tintFactor);
        anamorphicStreak += gated * coatingTint * streakWeight;
        streakWeight *= streakDecay;
    }

    // 6-blade hexagonal aperture diffraction starburst (three symmetric axes at 0, 60, and 120 degrees)
    float3 diffractionStar = float3(0.0f, 0.0f, 0.0f);
    const float2 invAspect = float2(1.0f / max(aspectRatio, 0.1f), 1.0f);
    const float2 rayAxis0 = float2(1.0f, 0.0f) * invAspect;
    const float2 rayAxis1 = float2(0.5f, 0.8660254f) * invAspect;
    const float2 rayAxis2 = float2(-0.5f, 0.8660254f) * invAspect;

    const float starStep  = 0.0045f;
    const float starDecay = 0.64f;
    const float starThreshold = 0.70f;
    float starWeight = 0.24f;

    [unroll]
    for (int stepIdx = 1; stepIdx <= 6; ++stepIdx)
    {
        const float dist = float(stepIdx) * starStep;
        const float3 s0 = ApplyBloomSoftKnee(max(g_bloomMip2.Sample(g_samplerLinear, saturate(uv + rayAxis0 * dist)).rgb, 0.0f), starThreshold, glareSoftKnee)
                        + ApplyBloomSoftKnee(max(g_bloomMip2.Sample(g_samplerLinear, saturate(uv - rayAxis0 * dist)).rgb, 0.0f), starThreshold, glareSoftKnee);
        const float3 s1 = ApplyBloomSoftKnee(max(g_bloomMip2.Sample(g_samplerLinear, saturate(uv + rayAxis1 * dist)).rgb, 0.0f), starThreshold, glareSoftKnee)
                        + ApplyBloomSoftKnee(max(g_bloomMip2.Sample(g_samplerLinear, saturate(uv - rayAxis1 * dist)).rgb, 0.0f), starThreshold, glareSoftKnee);
        const float3 s2 = ApplyBloomSoftKnee(max(g_bloomMip2.Sample(g_samplerLinear, saturate(uv + rayAxis2 * dist)).rgb, 0.0f), starThreshold, glareSoftKnee)
                        + ApplyBloomSoftKnee(max(g_bloomMip2.Sample(g_samplerLinear, saturate(uv - rayAxis2 * dist)).rgb, 0.0f), starThreshold, glareSoftKnee);

        diffractionStar += (s0 + s1 + s2) * starWeight;
        starWeight *= starDecay;
    }

    // Restrained internal lens reflection ghosts mirrored across the optical centre with chromatic dispersion
    const float2 flippedUv = float2(1.0f, 1.0f) - uv;
    const float2 toCentre = float2(0.5f, 0.5f) - flippedUv;
    const float2 dispersionDir = toCentre * 0.012f;

    float3 lensGhost = float3(0.0f, 0.0f, 0.0f);
    const float ghostScales[2] = { 0.45f, 0.95f };
    const float ghostWeights[2] = { 0.08f, 0.045f };

    [unroll]
    for (int gIdx = 0; gIdx < 2; ++gIdx)
    {
        const float2 ghostUv = flippedUv + toCentre * ghostScales[gIdx];
        const float radialDist = length(ghostUv - float2(0.5f, 0.5f));
        const float edgeMask = pow(saturate(1.0f - radialDist * 1.41421356f), 2.0f);

        const float r = max(g_bloomMip3.Sample(g_samplerLinear, saturate(ghostUv + dispersionDir)).r, 0.0f);
        const float g = max(g_bloomMip3.Sample(g_samplerLinear, saturate(ghostUv)).g, 0.0f);
        const float b = max(g_bloomMip3.Sample(g_samplerLinear, saturate(ghostUv - dispersionDir)).b, 0.0f);
        const float3 ghostSample = ApplyBloomSoftKnee(float3(r, g, b), 0.75f, 0.35f);

        lensGhost += ghostSample * edgeMask * ghostWeights[gIdx];
    }

    return veilingGlare + anamorphicStreak * 0.22f + diffractionStar * 0.24f + lensGhost;
}

// Tone mapping, HDR bloom, optical glare, and dithering post-processing stage matching Sky.hlsl / Standard.hlsl output
float4 PSMain(PostProcessVertexOutput input) : SV_TARGET
{
    uint sceneWidth = 1u;
    uint sceneHeight = 1u;
    g_hdrSceneTexture.GetDimensions(sceneWidth, sceneHeight);
    const float aspectRatio = max(float(sceneWidth), 1.0f) / max(float(sceneHeight), 1.0f);

    // Sample resolved floating-point HDR scene radiance
    const float4 hdrSample = g_hdrSceneTexture.Sample(g_samplerLinear, input.uv);
    float3 hdrColor = max(hdrSample.rgb, 0.0f);

    // Composite multi-scale optical HDR bloom and lens diffraction glare when enabled via g_adaptationParams.w
    const float bloomIntensity = max(g_adaptationParams.w, 0.0f);
    if (bloomIntensity > 0.0f)
    {
        const float3 bloomRadiance = max(g_bloomTexture.Sample(g_samplerLinear, input.uv).rgb, 0.0f);
        const float3 glareRadiance = EvaluateOpticalGlare(input.uv, aspectRatio);
        hdrColor += (bloomRadiance + glareRadiance) * bloomIntensity;
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
