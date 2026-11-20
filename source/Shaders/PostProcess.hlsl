// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "SceneBuffers.hlsli"

Texture2D g_hdrSceneTexture       : register(t33);
Texture2D g_bloomTexture          : register(t34);
Texture2D g_bloomMip1             : register(t35);
Texture2D g_bloomMip2             : register(t36);
Texture2D g_bloomMip3             : register(t37);
Texture2D g_bloomMip4             : register(t38);
Texture2D g_luminanceMeterTexture : register(t39);
Texture2D g_exposureTexture       : register(t40);
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

// Physical cos^4(theta) natural optical illumination falloff applied in linear radiance space
float EvaluateOpticalVignette(float2 uv, float aspectRatio)
{
    const float vignetteStrength = max(g_opticsParams.y, 0.0f);
    if (vignetteStrength <= 0.00001f)
    {
        return 1.0f;
    }

    const float2 aspectNorm = float2(aspectRatio, 1.0f) / max(aspectRatio, 1.0f);
    const float2 radial     = (uv - float2(0.5f, 0.5f)) * aspectNorm;
    const float tanThetaSq  = dot(radial, radial) * vignetteStrength * 2.0f;
    const float cosThetaSq  = 1.0f / (1.0f + tanThetaSq);

    return cosThetaSq * cosThetaSq;
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
    const float aspectRatio = max(float(texWidth), 1.0f) / max(float(texHeight), 1.0f);

    return float4(extracted * EvaluateOpticalVignette(uv, aspectRatio), 1.0f);
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

// Centre-weighted log2 luminance metering pass writing weighted log-luminance sums into a 16x16 grid
float4 LuminanceMeterPS(PostProcessVertexOutput input) : SV_TARGET
{
    const float gridDimension  = 16.0f;
    const float subGridCount   = 4.0f;
    const float minMeterLuma   = 0.10f;
    const float maxMeterLuma   = 64.0f;
    const float centreFalloff  = 3.0f;

    const float2 cellOrigin = floor(input.position.xy) / gridDimension;
    const float2 subStep    = float2(1.0f, 1.0f) / (gridDimension * subGridCount);

    float sumWeightedLogLuma = 0.0f;
    float sumWeight          = 0.0f;

    [loop]
    for (int y = 0; y < 4; ++y)
    {
        [loop]
        for (int x = 0; x < 4; ++x)
        {
            const float2 sampleUv = cellOrigin + (float2(float(x), float(y)) + float2(0.5f, 0.5f)) * subStep;
            const float3 hdrSample = max(g_hdrSceneTexture.SampleLevel(g_samplerLinear, sampleUv, 0.0f).rgb, 0.0f);
            const float luma = clamp(EvaluatePostLuminance(hdrSample), minMeterLuma, maxMeterLuma);

            const float2 offsetFromCentre = sampleUv - float2(0.5f, 0.5f);
            const float radialWeight = exp(-dot(offsetFromCentre, offsetFromCentre) * centreFalloff);

            sumWeightedLogLuma += log2(luma) * radialWeight;
            sumWeight += radialWeight;
        }
    }

    return float4(sumWeightedLogLuma, sumWeight, 0.0f, 1.0f);
}

// Temporal eye-adaptation pass reducing the 16x16 luminance meter into a smoothly adapted 1x1 exposure scalar
float4 ExposureAdaptPS(PostProcessVertexOutput input) : SV_TARGET
{
    float totalWeightedLogLuma = 0.0f;
    float totalWeight          = 0.0f;

    [loop]
    for (int y = 0; y < 16; ++y)
    {
        [loop]
        for (int x = 0; x < 16; ++x)
        {
            const float2 cell = g_luminanceMeterTexture.Load(int3(x, y, 0)).rg;
            totalWeightedLogLuma += cell.r;
            totalWeight += cell.g;
        }
    }

    const float avgLogLuma     = totalWeightedLogLuma / max(totalWeight, 0.0001f);
    const float avgLuma        = exp2(avgLogLuma);
    const float middleGrey     = max(g_adaptationParams.x, 0.01f);
    const float minExposure    = max(g_exposureParams.z, 0.01f);
    const float maxExposure    = max(g_exposureParams.w, minExposure);
    const float targetExposure = clamp(middleGrey / max(avgLuma, 0.0001f), minExposure, maxExposure);

    const float prevExposure = g_exposureTexture.Load(int3(0, 0, 0)).r;
    if (prevExposure <= 0.0001f)
    {
        return float4(targetExposure, 0.0f, 0.0f, 1.0f);
    }

    // Asymmetric optical iris adaptation: stops down faster into bright sunlight than it opens in shadow
    const float baseSpeed   = max(g_adaptationParams.y, 0.01f);
    const float brightBoost = 2.2f;
    const float adaptSpeed  = (targetExposure < prevExposure) ? (baseSpeed * brightBoost) : baseSpeed;
    const float deltaTime   = clamp(g_exposureParams.y, 0.0f, 0.25f);
    const float blendFactor = 1.0f - exp(-deltaTime * adaptSpeed);
    const float adapted     = clamp(prevExposure + (targetExposure - prevExposure) * blendFactor, minExposure, maxExposure);

    return float4(adapted, 0.0f, 0.0f, 1.0f);
}

// High-precision angular separation in radians via chord length, avoiding acos() cancellation near 0 degrees
float EvaluatePostAngularSeparation(float3 dirA, float3 dirB)
{
    return 2.0f * asin(saturate(0.5f * length(dirA - dirB)));
}

// Evaluates physical specular optical glare: occlusion-aware veiling scatter, anamorphic streak, 6-blade diffraction, and lens ghosts
float3 EvaluateOpticalGlare(float2 uv, float aspectRatio)
{
    const float glareThreshold = 0.55f;
    const float glareSoftKnee  = 0.40f;

    // Wide optical veiling scatter from the coarsest bloom pyramid mip (1/32 resolution)
    const float3 wideBloom = max(g_bloomMip4.Sample(g_samplerLinear, uv).rgb, 0.0f);

    // Reconstruct exact 3D eye-centre ray and per-pixel view ray for true angular distance to the Sun
    const float  invProjX   = 1.0f / max(g_mvp._m00, 0.0001f);
    const float  invProjY   = 1.0f / max(g_mvp._m11, 0.0001f);
    const float2 clipPos    = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    const float3 viewDir    = normalize(mul(float4(clipPos.x * invProjX, clipPos.y * invProjY, 1.0f, 0.0f), g_world).xyz);
    const float3 camRight   = normalize(g_world[0].xyz);
    const float3 camUp      = normalize(g_world[1].xyz);
    const float3 eyeForward = normalize(g_world[2].xyz);

    float3 sunDir = normalize(float3(0.35f, 0.92f, 0.18f));
    for (uint lightIdx = 0u; lightIdx < min(g_lightCount, 16u); ++lightIdx)
    {
        const uint lightType = asuint(g_lights[lightIdx].direction.w) & 0xFu;
        if (lightType == 0u && g_lights[lightIdx].direction.y < 0.0f)
        {
            sunDir = -normalize(g_lights[lightIdx].direction.xyz);
            break;
        }
    }

    // True 3D angular separation from the direct centre ray of the camera eye to the Sun
    const float eyeAngleToSun   = EvaluatePostAngularSeparation(eyeForward, sunDir);
    const float pixelAngleToSun = EvaluatePostAngularSeparation(viewDir, sunDir);

    // Project sunDir into screen UV to probe g_bloomMip4 directly at the Sun for occlusion and distance-flux gating
    const float3 sunCam = float3(
        dot(sunDir, camRight),
        dot(sunDir, camUp),
        dot(sunDir, eyeForward)
    );
    const float2 sunNdc = (sunCam.z > 0.01f) ? float2((sunCam.x / sunCam.z) * g_mvp._m00, (sunCam.y / sunCam.z) * g_mvp._m11) : float2(0.0f, 0.0f);
    const float2 sunUv  = saturate(float2(sunNdc.x * 0.5f + 0.5f, 0.5f - sunNdc.y * 0.5f));
    const float2 probeStep = float2(0.025f, 0.045f);
    const float3 sunBloomSample = (sunCam.z > 0.01f)
        ? (  max(g_bloomMip4.SampleLevel(g_samplerLinear, sunUv, 0.0f).rgb, 0.0f) * 0.40f
           + max(g_bloomMip4.SampleLevel(g_samplerLinear, saturate(sunUv + float2( probeStep.x, 0.0f)), 0.0f).rgb, 0.0f) * 0.15f
           + max(g_bloomMip4.SampleLevel(g_samplerLinear, saturate(sunUv - float2( probeStep.x, 0.0f)), 0.0f).rgb, 0.0f) * 0.15f
           + max(g_bloomMip4.SampleLevel(g_samplerLinear, saturate(sunUv + float2(0.0f,  probeStep.y)), 0.0f).rgb, 0.0f) * 0.15f
           + max(g_bloomMip4.SampleLevel(g_samplerLinear, saturate(sunUv - float2(0.0f,  probeStep.y)), 0.0f).rgb, 0.0f) * 0.15f)
        : float3(0.0f, 0.0f, 0.0f);

    const float solarBloomLuma = EvaluatePostLuminance(sunBloomSample);
    const float fluxGate       = smoothstep(0.01f, 0.09f, solarBloomLuma);
    const float maxGlareAngle  = 1.00f; // ~57 deg off-axis onset building smoothly to dead-on peak
    const float titration      = pow(saturate(1.0f - eyeAngleToSun / maxGlareAngle), 1.30f);
    const float blindingFactor = fluxGate * titration;

    // Tight, localized angular optical scatter around the Sun while keeping the rest of deep space crisp
    const float  veilProfile     = exp(-pixelAngleToSun * 4.10f) + 0.12f * exp(-pixelAngleToSun * 1.80f);
    const float  diskBoostCore   = exp(-pow(pixelAngleToSun / 0.0125f, 1.75f));
    const float3 veilTint        = max(sunBloomSample / max(solarBloomLuma, 0.0001f), 0.0f);
    const float3 blindingVeil    = veilTint * (blindingFactor * (veilProfile * 0.42f + diskBoostCore * 15.0f));

    const float3 veilingGlare = ApplyBloomSoftKnee(wideBloom, 0.20f, 0.50f) * 0.35f + blindingVeil;

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
    const float2 rayAxes[3] =
    {
        float2(1.0f, 0.0f) * invAspect,
        float2(0.5f, 0.8660254f) * invAspect,
        float2(-0.5f, 0.8660254f) * invAspect
    };

    const float starStep  = 0.0045f;
    const float starDecay = 0.64f;
    const float starThreshold = 0.70f;
    float starWeight = 0.24f;

    [unroll]
    for (int stepIdx = 1; stepIdx <= 6; ++stepIdx)
    {
        const float dist = float(stepIdx) * starStep;
        float3 stepSum = float3(0.0f, 0.0f, 0.0f);

        [unroll]
        for (int axisIdx = 0; axisIdx < 3; ++axisIdx)
        {
            const float2 offset = rayAxes[axisIdx] * dist;
            stepSum += ApplyBloomSoftKnee(max(g_bloomMip2.Sample(g_samplerLinear, saturate(uv + offset)).rgb, 0.0f), starThreshold, glareSoftKnee)
                     + ApplyBloomSoftKnee(max(g_bloomMip2.Sample(g_samplerLinear, saturate(uv - offset)).rgb, 0.0f), starThreshold, glareSoftKnee);
        }

        diffractionStar += stepSum * starWeight;
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

// Bradford LMS cone-space chromatic adaptation for physically grounded white balance (temperature and tint)
float3 ApplyWhiteBalance(float3 linearRgb, float temperature, float tint)
{
    if (abs(temperature) < 0.0001f && abs(tint) < 0.0001f)
    {
        return linearRgb;
    }

    const float t1 = temperature * 0.10f;
    const float t2 = tint * 0.10f;

    // Reference D65 illuminant white point shifted in CIE xy chromaticity space
    const float x = 0.31271f - t1 * (t1 < 0.0f ? 0.10f : 0.05f);
    const float y = (2.87f * x - 3.0f * x * x - 0.27509507f) + t2 * 0.05f;
    const float safeY = max(y, 0.0001f);

    const float3 w1 = float3(0.949237f, 1.03542f, 1.08728f); // D65 LMS reference
    const float3 xyz = float3(x / safeY, 1.0f, (1.0f - x - y) / safeY);
    const float3 w2 = float3(
        0.7328f * xyz.x + 0.4296f * xyz.y - 0.1624f * xyz.z,
       -0.7036f * xyz.x + 1.6975f * xyz.y + 0.0061f * xyz.z,
        0.0030f * xyz.x + 0.0136f * xyz.y + 0.9834f * xyz.z
    );

    const float3 balance = w1 / max(w2, float3(0.0001f, 0.0001f, 0.0001f));

    // Linear Rec.709 -> LMS -> balanced LMS -> Linear Rec.709
    const float3x3 linToLms = float3x3(
        3.90405e-1f, 5.49941e-1f, 8.92632e-3f,
        7.08416e-2f, 9.63172e-1f, 1.35775e-3f,
        2.31082e-2f, 1.28021e-1f, 9.36245e-1f
    );
    const float3x3 lmsToLin = float3x3(
        2.85847e+0f, -1.62879e+0f, -2.48910e-2f,
       -2.10182e-1f,  1.15820e+0f,  3.24281e-4f,
       -4.18120e-2f, -1.18169e-1f,  1.06867e+0f
    );

    const float3 lms = mul(linToLms, linearRgb) * balance;
    return max(mul(lmsToLin, lms), 0.0f);
}

// Applies linear-space filmic colour grading (white balance, star-safe log contrast, saturation, and shadow toe) before ACES
float3 ApplyFilmicColorGrading(float3 exposedColor)
{
    const float temperature = g_colorGradeParams.x;
    const float tint        = g_colorGradeParams.y;
    const float contrast    = (g_colorGradeParams.z > 0.001f) ? g_colorGradeParams.z : 1.0f;
    const float saturation  = (g_colorGradeParams.w >= 0.0f)  ? g_colorGradeParams.w : 1.0f;
    const float shadowToe   = max(g_opticsParams.x, 0.0f);

    float3 graded = ApplyWhiteBalance(max(exposedColor, 0.0f), temperature, tint);

    // Log-space contrast pivoted around 18% middle grey, faded out below 0.04 luma so faint stars remain untouched
    const float lumaBeforeContrast = EvaluatePostLuminance(graded);
    if (abs(contrast - 1.0f) > 0.0001f && lumaBeforeContrast > 0.002f)
    {
        const float midGrey      = 0.18f;
        const float contrastMask = smoothstep(0.002f, 0.04f, lumaBeforeContrast);
        const float3 logInput    = max(graded, float3(0.00001f, 0.00001f, 0.00001f));
        const float3 contrasted  = exp2(log2(logInput / midGrey) * contrast) * midGrey;
        graded = lerp(graded, max(contrasted, 0.0f), contrastMask);
    }

    // Luminance-preserving chromatic saturation adjustment
    const float lumaAfterContrast = EvaluatePostLuminance(graded);
    graded = max(lerp(float3(lumaAfterContrast, lumaAfterContrast, lumaAfterContrast), graded, saturation), 0.0f);

    // Optional filmic shadow toe anchoring deep atmospheric shadows without crushing near-black starlight
    if (shadowToe > 0.0001f)
    {
        const float toeMask = smoothstep(0.005f, 0.05f, lumaAfterContrast);
        const float3 toed = (graded * graded) / max(graded + shadowToe, float3(0.00001f, 0.00001f, 0.00001f));
        graded = lerp(graded, toed, toeMask);
    }

    return graded;
}

// Samples the HDR scene texture with restrained edge-only lateral chromatic dispersion
float4 SampleHdrSceneWithChromaticDispersion(float2 uv)
{
    const float chromaticStrength = max(g_opticsParams.z, 0.0f);
    const float2 centered = uv - float2(0.5f, 0.5f);
    const float radialSq  = dot(centered, centered);

    const float4 centerSample = g_hdrSceneTexture.Sample(g_samplerLinear, uv);
    if (chromaticStrength <= 0.00001f || radialSq <= 0.04f)
    {
        return centerSample;
    }

    // Cubic radial dispersion: zero at optical centre, subtle sub-pixel R/B separation near corners
    const float2 shift = centered * (radialSq * chromaticStrength);
    const float r = g_hdrSceneTexture.Sample(g_samplerLinear, saturate(uv + shift)).r;
    const float b = g_hdrSceneTexture.Sample(g_samplerLinear, saturate(uv - shift)).b;

    return float4(r, centerSample.g, b, centerSample.a);
}

// Fast 32-bit PCG integer hash avoiding trigonometric GPU precision patterns in sensor grain and dithering
uint PostProcessPcgHash(uint seed)
{
    uint state = seed * 747796405u + 2891336453u;
    uint word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// Evaluates a uniform [0, 1) pseudo-random scalar from integer pixel coordinates, channel, and frame index
float EvaluatePostNoise(uint2 pixelCoord, uint frameIndex, uint salt)
{
    const uint combined = pixelCoord.x * 1973u ^ pixelCoord.y * 9277u ^ (frameIndex + salt) * 26699u;
    return float(PostProcessPcgHash(combined) & 0x00FFFFFFu) * (1.0f / 16777216.0f);
}

// Low-light CMOS sensor micro-grain weighted to shaded midtones, suppressed in pure black vacuum and clipped highlights
float3 EvaluateCmosSensorGrain(float3 tonemappedColor, uint2 pixelCoord, uint frameIndex, float autoExposure)
{
    const float grainStrength = max(g_opticsParams.w, 0.0f);
    if (grainStrength <= 0.00001f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const float luma = EvaluatePostLuminance(tonemappedColor);
    // Keep pure black vacuum noise-free and roll off cleanly on bright specular peaks
    const float shadowGate    = smoothstep(0.004f, 0.045f, luma);
    const float highlightRoll = 1.0f - smoothstep(0.55f, 0.95f, luma);
    const float isoGain       = clamp(sqrt(max(autoExposure, 0.25f)), 0.70f, 1.35f);
    const float envelope      = shadowGate * highlightRoll * isoGain * grainStrength;

    // Triangular-PDF luminance micro-grain with a subtle 20% chroma read-noise component
    const float lumaNoise = (EvaluatePostNoise(pixelCoord, frameIndex, 11u) + EvaluatePostNoise(pixelCoord, frameIndex, 29u) - 1.0f);
    const float rChroma   = (EvaluatePostNoise(pixelCoord, frameIndex, 47u) - 0.5f) * 0.40f;
    const float gChroma   = (EvaluatePostNoise(pixelCoord, frameIndex, 71u) - 0.5f) * 0.25f;
    const float bChroma   = (EvaluatePostNoise(pixelCoord, frameIndex, 97u) - 0.5f) * 0.45f;

    return (float3(lumaNoise, lumaNoise, lumaNoise) + float3(rChroma, gChroma, bChroma)) * envelope;
}

// Tone mapping, HDR bloom, optical glare, filmic grading, lens optics, CMOS micro-grain, and TPDF dithering
float4 PSMain(PostProcessVertexOutput input) : SV_TARGET
{
    uint sceneWidth = 1u;
    uint sceneHeight = 1u;
    g_hdrSceneTexture.GetDimensions(sceneWidth, sceneHeight);
    const float aspectRatio = max(float(sceneWidth), 1.0f) / max(float(sceneHeight), 1.0f);

    // Sample resolved floating-point HDR scene radiance with optional edge-only lateral chromatic dispersion
    const float4 hdrSample = SampleHdrSceneWithChromaticDispersion(input.uv);
    float3 hdrColor = max(hdrSample.rgb, 0.0f);

    // Composite multi-scale optical HDR bloom and lens diffraction glare when enabled via g_adaptationParams.w
    const float bloomIntensity = max(g_adaptationParams.w, 0.0f);
    if (bloomIntensity > 0.0f)
    {
        const float3 bloomRadiance = max(g_bloomTexture.Sample(g_samplerLinear, input.uv).rgb, 0.0f);
        const float3 glareRadiance = EvaluateOpticalGlare(input.uv, aspectRatio);
        hdrColor += (bloomRadiance + glareRadiance) * bloomIntensity;
    }

    // Apply physical cos^4(theta) lens vignetting in linear optical radiance space
    hdrColor *= EvaluateOpticalVignette(input.uv, aspectRatio);

    // Apply manual exposure compensation modulated by temporally adapted auto-exposure
    const float manualExposure = max(g_exposureParams.x, 0.0001f);
    const float autoExposure   = (g_exposureParams.z > 0.0f) ? max(g_exposureTexture.Load(int3(0, 0, 0)).r, 0.0001f) : 1.0f;
    const float exposure       = manualExposure * autoExposure;
    const float3 exposedRadiance = hdrColor * exposure;
    const float3 gradedRadiance  = ApplyFilmicColorGrading(exposedRadiance);
    const float3 tonemappedRadiance = AcesFilmicToneMapping(gradedRadiance);

    // Low-light CMOS sensor micro-grain + +/-0.5 LSB PCG triangular-PDF dithering
    const uint2 pixelCoord = uint2(input.position.xy);
    const uint  frameIndex = uint(max(g_exposureParams.y, 0.0f));
    const float3 sensorGrain = EvaluateCmosSensorGrain(tonemappedRadiance, pixelCoord, frameIndex, autoExposure);

    const float ditherA   = EvaluatePostNoise(pixelCoord, frameIndex, 131u);
    const float ditherB   = EvaluatePostNoise(pixelCoord, frameIndex, 193u);
    const float dither    = (ditherA + ditherB - 1.0f) * (0.5f / 255.0f);
    const float hasSignal = step(0.0001f, dot(tonemappedRadiance, float3(1.0f, 1.0f, 1.0f)));

    return float4(saturate(tonemappedRadiance + sensorGrain + dither * hasSignal), hdrSample.a);
}
