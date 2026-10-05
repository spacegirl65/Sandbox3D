// Copyright © 2026 spacegirl65. All Rights Reserved.

#ifndef SCENE_BUFFERS_HLSLI
#define SCENE_BUFFERS_HLSLI

struct LightData
{
    float4 position;    // xyz = world position, w = range
    float4 direction;   // xyz = normalized direction, w = type (0=Dir, 1=Point, 2=Spot)
    float4 color;       // rgb = light color, w = intensity
    float4 attenuation; // x = constant, y = linear, z = quadratic, w = inner/outer spot cosine
};

struct InstanceData
{
    row_major float4x4 mvp;
    row_major float4x4 world;
    float4             colorTint;
};

cbuffer SceneConstantBuffer : register(b0)
{
    row_major float4x4 g_mvp;
    row_major float4x4 g_world;
    float4             g_ambientColor;
    float4             g_fogColor;
    float4             g_fogParams;
    uint               g_lightCount;
    uint               g_isInstanced;
    uint               g_objectLightChannels;
    uint               g_padding;
    LightData          g_lights[16];
    row_major float4x4 g_shadowViewProj[4];
    float4             g_cascadeSplits;
    float4             g_shadowParams;
    float4             g_rayleighParams;
    float4             g_mieParams;
    float4             g_ozoneParams;
    float4             g_atmosphereParams;
};

StructuredBuffer<InstanceData> g_instances : register(t0);

// Cascaded Directional Shadows depth array resource and comparison sampler
Texture2DArray g_shadowMapArray : register(t44);
SamplerComparisonState g_samplerShadow : register(s2);

// Percentage-Closer Filter (PCF) evaluation across cascaded directional solar shadow map
float EvaluateCascadeSliceShadow(float3 worldPosition, float3 worldNormal, float3 lightDir, int cascadeIndex)
{
    // Evaluate light clip coordinates (M_LightViewProj[i] with row-vector convention v * M)
    float4 lightClip = mul(float4(worldPosition, 1.0f), g_shadowViewProj[cascadeIndex]);

    // Homogeneous clip space to shadow map UV coordinates
    float3 shadowCoord = float3(
        lightClip.x * 0.5f + 0.5f,
        -lightClip.y * 0.5f + 0.5f,
        lightClip.z
    );

    // Depth bounds verification
    if (shadowCoord.z < 0.0f || shadowCoord.z > 1.0f)
    {
        return 1.0f;
    }

    // Boundary check within cascade projection
    if (shadowCoord.x < 0.001f || shadowCoord.x > 0.999f || shadowCoord.y < 0.001f || shadowCoord.y > 0.999f)
    {
        return 1.0f;
    }

    // Slope-scaled depth bias evaluating incident surface angle
    float nDotL = max(dot(worldNormal, lightDir), 0.0f);
    float slopeFactor = sqrt(saturate(1.0f - nDotL * nDotL)) / max(nDotL, 0.001f);
    slopeFactor = min(slopeFactor, 4.0f);

    // Per-cascade scaled bias compensating for larger world-space footprint in distant cascades
    float cascadeScale = float(cascadeIndex + 1);
    float bias = (0.00005f + 0.00020f * slopeFactor) * cascadeScale;

    // 4-tap rotated bilinear Percentage-Closer Filter (PCF) with hardware comparison sampling
    const float texelSize = 1.0f / 2048.0f;
    const float2 offsets[4] = {
        float2(-0.7071f, -0.7071f) * texelSize,
        float2( 0.7071f, -0.7071f) * texelSize,
        float2(-0.7071f,  0.7071f) * texelSize,
        float2( 0.7071f,  0.7071f) * texelSize
    };

    float shadow = 0.0f;
    [unroll]
    for (int tap = 0; tap < 4; ++tap)
    {
        shadow += g_shadowMapArray.SampleCmpLevelZero(
            g_samplerShadow,
            float3(shadowCoord.xy + offsets[tap], float(cascadeIndex)),
            shadowCoord.z - bias
        );
    }

    return shadow * 0.25f;
}

float CalculateCascadedShadow(float3 worldPosition, float3 worldNormal, float3 lightDir)
{
    // Radial camera distance corresponding to logarithmic view frustum partition splits
    const float viewDepth = length(worldPosition);

    // Select cascade slice according to logarithmic depth split boundaries
    int cascadeIndex = 3;
    if (viewDepth < g_cascadeSplits.x)
    {
        cascadeIndex = 0;
    }
    else if (viewDepth < g_cascadeSplits.y)
    {
        cascadeIndex = 1;
    }
    else if (viewDepth < g_cascadeSplits.z)
    {
        cascadeIndex = 2;
    }
    else if (viewDepth >= g_cascadeSplits.w)
    {
        return 1.0f; // Beyond maximum macroscopic shadow distance (1,600m)
    }

    float shadow = EvaluateCascadeSliceShadow(worldPosition, worldNormal, lightDir, cascadeIndex);

    // Smooth cascade partition transition blending
    if (cascadeIndex < 3)
    {
        float splitDistance = g_cascadeSplits[cascadeIndex];
        float blendStart = splitDistance * 0.92f;
        if (viewDepth > blendStart)
        {
            float blendFactor = saturate((viewDepth - blendStart) / (splitDistance - blendStart));
            float nextShadow = EvaluateCascadeSliceShadow(worldPosition, worldNormal, lightDir, cascadeIndex + 1);
            shadow = lerp(shadow, nextShadow, blendFactor);
        }
    }
    else
    {
        // Smooth distant fade towards unshadowed illumination at the macroscopic boundary
        float maxDistance = g_cascadeSplits.w;
        float fadeStart = maxDistance - 200.0f;
        if (viewDepth > fadeStart)
        {
            float fadeFactor = saturate((viewDepth - fadeStart) / 200.0f);
            shadow = lerp(shadow, 1.0f, fadeFactor);
        }
    }

    return shadow;
}

#endif // SCENE_BUFFERS_HLSLI

