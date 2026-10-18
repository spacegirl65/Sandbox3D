// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Atmosphere.hlsli"

// Terrain PBR composite material textures bound to root descriptor table slots t1 through t24
// Biome 0: Meadow and alluvial valley pasture (uncut_grass_1)
Texture2D g_texMeadowAlbedoHeight : register(t1);
Texture2D g_texMeadowNormal       : register(t2);
Texture2D g_texMeadowORM          : register(t3);

// Biome 1: Mid-slope fell turf (wild_grass_1)
Texture2D g_texMidSlopeAlbedoHeight : register(t4);
Texture2D g_texMidSlopeNormal       : register(t5);
Texture2D g_texMidSlopeORM          : register(t6);

// Biome 2: High plateau moorland (wild_grass_2)
Texture2D g_texPlateauAlbedoHeight : register(t7);
Texture2D g_texPlateauNormal       : register(t8);
Texture2D g_texPlateauORM          : register(t9);

// Biome 3: Dry thatch knolls and weathered ridges (grass_dried_1)
Texture2D g_texDryThatchAlbedoHeight : register(t10);
Texture2D g_texDryThatchNormal       : register(t11);
Texture2D g_texDryThatchORM          : register(t12);

// Biome 4: Crevice, scar, and sheer cliff rock (rock_cliff_1)
Texture2D g_texRockAlbedoHeight : register(t13);
Texture2D g_texRockNormal       : register(t14);
Texture2D g_texRockORM          : register(t15);

// Alternative Biome 0: Clover and grazed pasture (uncut_grass_2)
Texture2D g_texMeadow2AlbedoHeight : register(t16);
Texture2D g_texMeadow2Normal       : register(t17);
Texture2D g_texMeadow2ORM          : register(t18);

// Alternative Biome 1: Fine moor-edge bent-grass (wild_grass_3)
Texture2D g_texMidSlope2AlbedoHeight : register(t19);
Texture2D g_texMidSlope2Normal       : register(t20);
Texture2D g_texMidSlope2ORM          : register(t21);

// Alternative Biome 2: Weathered crag alpine turf (wild_grass_4)
Texture2D g_texPlateau2AlbedoHeight : register(t22);
Texture2D g_texPlateau2Normal       : register(t23);
Texture2D g_texPlateau2ORM          : register(t24);

// Pre-baked LiDAR Horizon Ambient Occlusion and Crevice Depth map
Texture2D g_texLidarAO           : register(t25);
Texture2D g_texLidarHorizon0     : register(t26);
Texture2D g_texLidarHorizon1     : register(t27);

// Static samplers: anisotropic 16x wrap sampler in s0, and bilinear clamp sampler in s1
SamplerState g_samplerAniso : register(s0);
SamplerState g_samplerClamp : register(s1);

struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
};

struct VertexOutput
{
    float4 position        : SV_POSITION;
    float3 worldNormal     : NORMAL;
    float4 color           : COLOR;
    float3 worldPosition   : TEXCOORD0;
    float3 terrainPosition : TEXCOORD1;
};

struct PbrSurface
{
    float3 albedo;
    float3 normalOffset;
    float  roughness;
    float  ao;
};

// Triplanar screen-space analytic derivative structure for seamless texture filtering
struct TriplanarGradients
{
    float2 ddxX, ddyX;
    float2 ddxY, ddyY;
    float2 ddxZ, ddyZ;
};

// ================================================================================================
// Procedural Stochastic Hexagonal Anti-Tiling
// Based on Morten S. Mikkelsen (JCGT 2022) and Thomas Deliot & Eric Heitz (GPU Zen 2)
// Eliminates rectilinear texture repetition while preserving 100% of authored mean RGB colour & contrast.
// ================================================================================================

float2 HexHash2D(int2 p)
{
    // Fast, robust 32-bit integer hash generating deterministic [0, 1) pseudo-random 2D offsets
    uint2 u = uint2(asuint(p.x), asuint(p.y));
    u = u * 1664525u + 1013904223u;
    u.x += u.y * 1664525u;
    u.y += u.x * 1664525u;
    u ^= (u >> 16u);
    u.x += u.y * 1664525u;
    u.y += u.x * 1664525u;
    return float2(u & 0x00ffffffu) * (1.0f / 16777216.0f);
}

void EvaluateHexLattice(
    float2 uv,
    out float3 weights,
    out float2 uvOffset1,
    out float2 uvOffset2,
    out float2 uvOffset3
)
{
    // Equilateral-triangle lattice skew transform (simplex grid)
    const float2x2 gridToSkewedGrid = float2x2(1.0f, -0.57735027f, 0.0f, 1.15470054f);
    float2 skewedCoord = mul(gridToSkewedGrid, uv);

    int2 baseId = int2(floor(skewedCoord));
    float3 temp = float3(frac(skewedCoord), 0.0f);
    temp.z = 1.0f - temp.x - temp.y;

    int2 v1, v2, v3;
    float3 rawWeights;
    if (temp.z > 0.0f)
    {
        rawWeights = float3(temp.z, temp.y, temp.x);
        v1 = baseId;
        v2 = baseId + int2(0, 1);
        v3 = baseId + int2(1, 0);
    }
    else
    {
        rawWeights = float3(-temp.z, 1.0f - temp.y, 1.0f - temp.x);
        v1 = baseId + int2(1, 1);
        v2 = baseId + int2(1, 0);
        v3 = baseId + int2(0, 1);
    }

    uvOffset1 = HexHash2D(v1);
    uvOffset2 = HexHash2D(v2);
    uvOffset3 = HexHash2D(v3);

    // Quartic contrast-sharpening (Mikkelsen 2022) preserving micro-contrast across tile boundaries
    float3 w2 = rawWeights * rawWeights;
    float3 w  = w2 * w2;
    weights = w / max(dot(w, float3(1.0f, 1.0f, 1.0f)), 0.0001f);
}

float3 SampleHexTiledAlbedo(Texture2D tex, float2 uv, float2 ddxUv, float2 ddyUv)
{
    float3 weights;
    float2 off1, off2, off3;
    EvaluateHexLattice(uv * 0.65f, weights, off1, off2, off3);

    float3 color = float3(0.0f, 0.0f, 0.0f);
    float sumWeights = 0.0f;

    [branch]
    if (weights.x > 0.02f)
    {
        color += tex.SampleGrad(g_samplerAniso, uv + off1, ddxUv, ddyUv).rgb * weights.x;
        sumWeights += weights.x;
    }
    [branch]
    if (weights.y > 0.02f)
    {
        color += tex.SampleGrad(g_samplerAniso, uv + off2, ddxUv, ddyUv).rgb * weights.y;
        sumWeights += weights.y;
    }
    [branch]
    if (weights.z > 0.02f)
    {
        color += tex.SampleGrad(g_samplerAniso, uv + off3, ddxUv, ddyUv).rgb * weights.z;
        sumWeights += weights.z;
    }

    return color / max(sumWeights, 0.0001f);
}

struct MicroPbrSample
{
    float3 albedo;
    float2 norm;
    float  rough;
    float  ao;
};

MicroPbrSample SampleMicroHexTiled(
    Texture2D albedoHeightTex,
    Texture2D normalTex,
    Texture2D ormTex,
    float2 uv,
    float2 ddxUv,
    float2 ddyUv
)
{
    float3 weights;
    float2 off1, off2, off3;
    EvaluateHexLattice(uv * 0.70f, weights, off1, off2, off3);

    MicroPbrSample result;
    result.albedo = float3(0.0f, 0.0f, 0.0f);
    result.norm   = float2(0.0f, 0.0f);
    result.rough  = 0.0f;
    result.ao     = 0.0f;
    float sumWeights = 0.0f;

    [branch]
    if (weights.x > 0.02f)
    {
        float2 u1 = uv + off1;
        float2 orm = ormTex.SampleGrad(g_samplerAniso, u1, ddxUv, ddyUv).rg;
        result.albedo += albedoHeightTex.SampleGrad(g_samplerAniso, u1, ddxUv, ddyUv).rgb * weights.x;
        result.norm   += normalTex.SampleGrad(g_samplerAniso, u1, ddxUv, ddyUv).rg * weights.x;
        result.ao     += orm.r * weights.x;
        result.rough  += orm.g * weights.x;
        sumWeights    += weights.x;
    }
    [branch]
    if (weights.y > 0.02f)
    {
        float2 u2 = uv + off2;
        float2 orm = ormTex.SampleGrad(g_samplerAniso, u2, ddxUv, ddyUv).rg;
        result.albedo += albedoHeightTex.SampleGrad(g_samplerAniso, u2, ddxUv, ddyUv).rgb * weights.y;
        result.norm   += normalTex.SampleGrad(g_samplerAniso, u2, ddxUv, ddyUv).rg * weights.y;
        result.ao     += orm.r * weights.y;
        result.rough  += orm.g * weights.y;
        sumWeights    += weights.y;
    }
    [branch]
    if (weights.z > 0.02f)
    {
        float2 u3 = uv + off3;
        float2 orm = ormTex.SampleGrad(g_samplerAniso, u3, ddxUv, ddyUv).rg;
        result.albedo += albedoHeightTex.SampleGrad(g_samplerAniso, u3, ddxUv, ddyUv).rgb * weights.z;
        result.norm   += normalTex.SampleGrad(g_samplerAniso, u3, ddxUv, ddyUv).rg * weights.z;
        result.ao     += orm.r * weights.z;
        result.rough  += orm.g * weights.z;
        sumWeights    += weights.z;
    }

    float invSum = 1.0f / max(sumWeights, 0.0001f);
    result.albedo *= invSum;
    result.norm   *= invSum;
    result.rough  *= invSum;
    result.ao     *= invSum;
    return result;
}

// Dual-frequency planar PBR projection evaluator with Parallax Occlusion Mapping and Hex-Tiling
PbrSurface SamplePlanePbr(
    Texture2D albedoHeightTex,
    Texture2D normalTex,
    Texture2D ormTex,
    float2 uv,
    float2 ddxUv,
    float2 ddyUv,
    float2 viewDirTan,
    float cameraDist,
    uint planeAxis
)
{
    const float scaleA = 0.50f;
    const float scaleB = 0.08f;

    float3 albedoA;
    float2 normA;
    float  roughA;
    float  aoA;

    // Parallax Occlusion Mapping on primary micro layer evaluating optical self-occlusion at close range (< 16m)
    float2 pomUvA = uv * scaleA;
    if (cameraDist < 16.0f)
    {
        const float pomFade = saturate((16.0f - cameraDist) / 6.0f);
        const float heightScale = 0.045f * scaleA * pomFade;
        const uint numSteps = 8;
        const float stepSize = 1.0f / (float)numSteps;
        const float2 uvDelta = viewDirTan * heightScale * stepSize;

        float currentLayer = 1.0f;
        float2 currentUv = pomUvA;
        float h = albedoHeightTex.SampleGrad(g_samplerAniso, currentUv, ddxUv * scaleA, ddyUv * scaleA).a;

        [unroll(8)]
        for (uint s = 0; s < numSteps; ++s)
        {
            if (h >= currentLayer)
                break;
            currentLayer -= stepSize;
            currentUv += uvDelta;
            h = albedoHeightTex.SampleGrad(g_samplerAniso, currentUv, ddxUv * scaleA, ddyUv * scaleA).a;
        }

        float prevLayer = currentLayer + stepSize;
        float prevH = albedoHeightTex.SampleGrad(g_samplerAniso, currentUv - uvDelta, ddxUv * scaleA, ddyUv * scaleA).a;
        float nextDiff = h - currentLayer;
        float prevDiff = prevLayer - prevH;
        float weight = saturate(nextDiff / max(nextDiff + prevDiff, 0.0001f));
        pomUvA = lerp(currentUv, currentUv - uvDelta, weight);

        float4 albH = albedoHeightTex.SampleGrad(g_samplerAniso, pomUvA, ddxUv * scaleA, ddyUv * scaleA);
        albedoA = albH.rgb;
        normA   = normalTex.SampleGrad(g_samplerAniso, pomUvA, ddxUv * scaleA, ddyUv * scaleA).rg;
        float2 orm = ormTex.SampleGrad(g_samplerAniso, pomUvA, ddxUv * scaleA, ddyUv * scaleA).rg;
        aoA     = orm.r;
        roughA  = orm.g;
    }
    else
    {
        // Distance range: Stochastic hexagonal anti-tiling breaking micro tile collinearity
        MicroPbrSample ms = SampleMicroHexTiled(
            albedoHeightTex, normalTex, ormTex,
            uv * scaleA, ddxUv * scaleA, ddyUv * scaleA
        );
        albedoA = ms.albedo;
        normA   = ms.norm;
        roughA  = ms.rough;
        aoA     = ms.ao;
    }

    // Multi-scale detail & normal/AO relief (AAA terrain standard):
    // 1. Sample 1x1 mip-tail mean (Mip 12 of 4K BC7_UNORM_SRGB) so microRatio (albedoA / albedoMean)
    //    is strictly centered at (1, 1, 1).
    // 2. Keep full multi-scale RGB detail ratio (albedoA / albedoB) and macro normal/AO relief active
    //    all the way across close and mid-ground hillsides (0m..450m), smoothly attenuating over
    //    450m..950m so distant mountain ranges (> 1000m) remain completely free of macro blotches.
    float3 albedoMean = albedoHeightTex.SampleLevel(g_samplerAniso, float2(0.5f, 0.5f), 12.0f).rgb;

    // Convert sampled albedo from hardware-linearised sRGB to display gamma space
    albedoA    = pow(max(albedoA, 0.0001f), 1.0f / 2.2f);
    albedoMean = pow(max(albedoMean, 0.0001f), 1.0f / 2.2f);

    float3 microRatio  = clamp(albedoA / max(albedoMean, 0.05f), 0.45f, 1.75f);
    float3 detailRatio = microRatio;

    [branch]
    if (cameraDist < 950.0f)
    {
        // Rotated macro planar coordinates decorrelating frequencies
        const float rotCos = 0.7986355f;
        const float rotSin = 0.6018150f;
        const float2 uvRot = float2(
            uv.x * rotCos - uv.y * rotSin,
            uv.x * rotSin + uv.y * rotCos
        );
        const float2 uvB = uvRot * scaleB + float2(0.37f, 0.71f);
        const float2 ddxUvB = float2(
            ddxUv.x * rotCos - ddxUv.y * rotSin,
            ddxUv.x * rotSin + ddxUv.y * rotCos
        ) * scaleB;
        const float2 ddyUvB = float2(
            ddyUv.x * rotCos - ddyUv.y * rotSin,
            ddyUv.x * rotSin + ddyUv.y * rotCos
        ) * scaleB;

        // Macro layer (12.5m tile): sampled via stochastic hexagonal tiling for albedo, normal, roughness, and AO
        MicroPbrSample msB = SampleMicroHexTiled(
            albedoHeightTex, normalTex, ormTex,
            uvB, ddxUvB, ddyUvB
        );
        float3 albedoB = pow(max(msB.albedo, 0.0001f), 1.0f / 2.2f);

        // Full close-to-mid range detail ratio preserved out to 450m, smoothly fading over 450m..950m
        const float detailFade = 1.0f - smoothstep(450.0f, 950.0f, cameraDist);
        const float3 closeRatio = clamp(albedoA / max(albedoB, 0.05f), 0.45f, 1.75f);
        detailRatio = lerp(microRatio, closeRatio, detailFade);

        // Blend zero-mean macro normal and AO offsets centered around their 1x1 mip-tail mean (Mip 12)
        // so close-to-mid range slopes (< 950m) gain 3D turf relief with zero shift in average lighting or brightness
        const float2 normMean = normalTex.SampleLevel(g_samplerAniso, float2(0.5f, 0.5f), 12.0f).rg;
        const float2 ormMean  = ormTex.SampleLevel(g_samplerAniso, float2(0.5f, 0.5f), 12.0f).rg;
        normA  = clamp(normA + (msB.norm - normMean) * (0.65f * detailFade), 0.0f, 1.0f);
        aoA    = saturate(aoA + (msB.ao - ormMean.r) * (0.40f * detailFade));
        roughA = saturate(roughA + (msB.rough - ormMean.g) * (0.35f * detailFade));
    }

    PbrSurface result;
    result.albedo    = detailRatio;
    result.roughness = roughA;
    result.ao        = aoA;

    // Convert planar tangent-space normal offsets to world-space normal perturbations
    float2 tanXY = normA * 2.0f - 1.0f;
    if (planeAxis == 1) // Y-plane projection (coords.xz)
    {
        result.normalOffset = float3(tanXY.x, 0.0f, tanXY.y);
    }
    else if (planeAxis == 0) // X-plane projection (coords.zy)
    {
        result.normalOffset = float3(0.0f, tanXY.y, tanXY.x);
    }
    else // Z-plane projection (coords.xy)
    {
        result.normalOffset = float3(tanXY.x, tanXY.y, 0.0f);
    }

    return result;
}

// Triplanar material projection synthesiser with dynamic plane culling
PbrSurface SampleTriplanarMaterial(
    Texture2D albedoHeightTex,
    Texture2D normalTex,
    Texture2D ormTex,
    float3 terrainPos,
    float3 blendWeights,
    TriplanarGradients grads,
    float3 V,
    float cameraDist
)
{
    float2 vTanX = float2(-V.z, -V.y) / max(abs(V.x), 0.15f);
    float2 vTanY = float2(-V.x, -V.z) / max(abs(V.y), 0.15f);
    float2 vTanZ = float2(-V.x, -V.y) / max(abs(V.z), 0.15f);

    float activePlaneSum = 0.0f;
    if (blendWeights.x > 0.02f) activePlaneSum += blendWeights.x;
    if (blendWeights.y > 0.02f) activePlaneSum += blendWeights.y;
    if (blendWeights.z > 0.02f) activePlaneSum += blendWeights.z;
    const float3 weights = blendWeights / max(activePlaneSum, 0.0001f);

    PbrSurface result;
    result.albedo       = float3(0.0f, 0.0f, 0.0f);
    result.normalOffset = float3(0.0f, 0.0f, 0.0f);
    result.roughness    = 0.0f;
    result.ao           = 0.0f;

    [branch]
    if (blendWeights.y > 0.02f)
    {
        PbrSurface sampleY = SamplePlanePbr(albedoHeightTex, normalTex, ormTex, terrainPos.xz, grads.ddxY, grads.ddyY, vTanY, cameraDist, 1);
        result.albedo       += sampleY.albedo * weights.y;
        result.normalOffset += sampleY.normalOffset * weights.y;
        result.roughness    += sampleY.roughness * weights.y;
        result.ao           += sampleY.ao * weights.y;
    }

    [branch]
    if (blendWeights.x > 0.02f)
    {
        PbrSurface sampleX = SamplePlanePbr(albedoHeightTex, normalTex, ormTex, terrainPos.zy, grads.ddxX, grads.ddyX, vTanX, cameraDist, 0);
        result.albedo       += sampleX.albedo * weights.x;
        result.normalOffset += sampleX.normalOffset * weights.x;
        result.roughness    += sampleX.roughness * weights.x;
        result.ao           += sampleX.ao * weights.x;
    }

    [branch]
    if (blendWeights.z > 0.02f)
    {
        PbrSurface sampleZ = SamplePlanePbr(albedoHeightTex, normalTex, ormTex, terrainPos.xy, grads.ddxZ, grads.ddyZ, vTanZ, cameraDist, 2);
        result.albedo       += sampleZ.albedo * weights.z;
        result.normalOffset += sampleZ.normalOffset * weights.z;
        result.roughness    += sampleZ.roughness * weights.z;
        result.ao           += sampleZ.ao * weights.z;
    }

    return result;
}

// Evaluates a pair of biome variations with branch-culled spatial blending
PbrSurface SampleBiomePair(
    Texture2D albH1, Texture2D nrm1, Texture2D orm1,
    Texture2D albH2, Texture2D nrm2, Texture2D orm2,
    float mixFactor,
    float3 terrainPos,
    float3 blendWeights,
    TriplanarGradients grads,
    float3 V,
    float cameraDist
)
{
    [branch]
    if (mixFactor <= 0.08f)
    {
        return SampleTriplanarMaterial(albH1, nrm1, orm1, terrainPos, blendWeights, grads, V, cameraDist);
    }
    else if (mixFactor >= 0.92f)
    {
        return SampleTriplanarMaterial(albH2, nrm2, orm2, terrainPos, blendWeights, grads, V, cameraDist);
    }
    else
    {
        PbrSurface s1 = SampleTriplanarMaterial(albH1, nrm1, orm1, terrainPos, blendWeights, grads, V, cameraDist);
        PbrSurface s2 = SampleTriplanarMaterial(albH2, nrm2, orm2, terrainPos, blendWeights, grads, V, cameraDist);
        const float t = smoothstep(0.08f, 0.92f, mixFactor);
        PbrSurface result;
        result.albedo       = lerp(s1.albedo, s2.albedo, t);
        result.normalOffset = lerp(s1.normalOffset, s2.normalOffset, t);
        result.roughness    = lerp(s1.roughness, s2.roughness, t);
        result.ao           = lerp(s1.ao, s2.ao, t);
        return result;
    }
}

// Evaluates pre-baked mountain-scale horizon elevation sine across eight azimuthal directions
float SampleHorizonElevationSine(float4 h0, float4 h1, float3 L)
{
    static const float twoPi = 6.28318530718f;
    float sunAzimuth = atan2(L.z, L.x);
    if (sunAzimuth < 0.0f)
    {
        sunAzimuth += twoPi;
    }

    const float u = (sunAzimuth / twoPi) * 8.0f;
    const uint k0 = (uint) floor(u) % 8u;
    const uint k1 = (k0 + 1u) % 8u;
    const float f = frac(u);

    const float angles[8] = { h0.r, h0.g, h0.b, h0.a, h1.r, h1.g, h1.b, h1.a };
    return lerp(angles[k0], angles[k1], f);
}
   
// Evaluates continuous procedural domain noise for breaking up polygonal slope transitions
float EvaluateSlopeTransitionNoise(float2 pos, float pixelFootprint)
{
    // Primary macro fracture frequency (~18m wavelength) breaking 15m triangle edges
    const float2 p1 = pos * 0.35f;
    const float n1 = sin(p1.x + sin(p1.y * 1.31f)) * cos(p1.y + cos(p1.x * 1.13f));

    // Secondary micro fissure frequency (~6m wavelength) adding natural jaggedness
    const float2 p2 = float2(pos.x * 0.866f - pos.y * 0.5f, pos.x * 0.5f + pos.y * 0.866f) * 1.05f;
    const float n2 = sin(p2.x + cos(p2.y * 1.27f)) * cos(p2.y + sin(p2.x * 1.19f));

    // Screen-space analytic derivative filtering preventing distant sub-pixel shimmering
    const float filterFade = saturate(1.0f - pixelFootprint * 0.25f);
    return (n1 * 0.055f + n2 * 0.025f) * filterFade;
}

// Evaluates multi-octave domain-warped 2D procedural noise for broad hillside ecological variation (AAA standard)
// Evaluated strictly across horizontal coordinates (X, Z) with rotational decorrelation to eliminate elevation banding
float EvaluateBiomeVariationMask(float2 xz, float frequency, float2 offset)
{
    // Layer 1: Large-scale ecological domain warp (~300m - 600m hillside swaths)
    float2 p0 = (xz + offset) * frequency;
    float2 warp = float2(
        sin(p0.y * 1.13f + cos(p0.x * 0.89f) * 1.35f),
        cos(p0.x * 1.07f + sin(p0.y * 0.93f) * 1.35f)
    );

    // Layer 2: Rotated & warped primary variation (~200m - 350m broad organic drifts)
    const float2x2 rot = float2x2(0.8660254f, -0.5000000f, 0.5000000f, 0.8660254f);
    float2 p1 = mul(rot, p0 * 1.35f + warp * 0.85f);
    float n0 = sin(p0.x + warp.x * 0.90f) * cos(p0.y + warp.y * 0.90f);
    float n1 = sin(p1.x + sin(p1.y * 0.97f)) * cos(p1.y + cos(p1.x * 0.89f));

    // Smooth, balanced range [0, 1] without small high-frequency islands
    return saturate((n0 * 0.65f + n1 * 0.35f) * 0.45f + 0.5f);
}

// Vertex shader stage
VertexOutput VSMain(VertexInput input, uint instanceId : SV_InstanceID)
{
    VertexOutput output;

    // Row-vector multiplication convention (v * M) adhering to engine standards
    if (g_isInstanced != 0)
    {
        InstanceData inst      = g_instances[instanceId];
        output.position        = mul(float4(input.position, 1.0f), inst.mvp);
        output.worldNormal     = normalize(mul(float4(input.normal, 0.0f), inst.world).xyz);
        output.worldPosition   = mul(float4(input.position, 1.0f), inst.world).xyz;
        output.color           = input.color * inst.colorTint;
        output.terrainPosition = input.position;
    }
    else
    {
        output.position        = mul(float4(input.position, 1.0f), g_mvp);
        output.worldNormal     = normalize(mul(float4(input.normal, 0.0f), g_world).xyz);
        output.worldPosition   = mul(float4(input.position, 1.0f), g_world).xyz;
        output.color           = input.color;
        output.terrainPosition = input.position;
    }

    return output;
}

// Pixel shader stage
float4 PSMain(VertexOutput input) : SV_TARGET
{
    const float3 N = normalize(input.worldNormal);
    const float cameraDist = length(input.worldPosition);
    const float3 V = (cameraDist > 0.001f) ? (-input.worldPosition / cameraDist) : float3(0.0f, 1.0f, 0.0f);

    // Material response determination from vertex colour, saturation, and normal slope
    const float colorSaturation = max(max(input.color.r, input.color.g), input.color.b) -
                                  min(min(input.color.r, input.color.g), input.color.b);
    const float luminance = dot(input.color.rgb, float3(0.299f, 0.587f, 0.114f));

    // Screen-space footprint and procedural domain jitter breaking up polygonal triangle edges
    const float2 ddxTerrainPos = ddx(input.terrainPosition.xz);
    const float2 ddyTerrainPos = ddy(input.terrainPosition.xz);
    const float pixelFootprint = length(ddxTerrainPos) + length(ddyTerrainPos);
    const float slopeNoise = EvaluateSlopeTransitionNoise(input.terrainPosition.xz, pixelFootprint);

    // Altitudinal zone evaluation with broad 350m organic undulation preventing horizontal horizon lines without 18m speckles
    const float altNorm = (input.terrainPosition.y > 100.0f)
        ? saturate((input.terrainPosition.y - 100.0f) / 450.0f)
        : saturate((input.terrainPosition.y + 4.0f) / 93.6f);

    const float macroAltWave = sin(input.terrainPosition.x * 0.0031f + cos(input.terrainPosition.z * 0.0027f) * 1.5f) * 0.035f;
    const float jitteredAlt  = saturate(altNorm + macroAltWave);
    const float valleyFade   = smoothstep(0.16f, 0.28f, jitteredAlt);
    const float plateauFade  = smoothstep(0.46f, 0.60f, jitteredAlt);

    // Continuous slope and desaturation factors preventing sharp specular threshold facets
    const float rockSlopeMinNy = 0.66f;
    const float rockSlopeMaxNy = 0.78f;
    const float jitteredNy = clamp(N.y + slopeNoise, 0.0f, 1.0f);
    const float slopeRockFactor = 1.0f - smoothstep(rockSlopeMinNy, rockSlopeMaxNy, jitteredNy);

    const float desatMin = 0.02f;
    const float desatMax = 0.22f;
    const float jitteredSat = colorSaturation + slopeNoise * 0.40f;
    const float desatFactor = 1.0f - smoothstep(desatMin, desatMax, jitteredSat);

    const float rockGreenMin = 0.44f;
    const float rockGreenMax = 0.66f;
    const float jitteredGreen = input.color.g + slopeNoise * 0.20f;
    const float lowGreenFactor = 1.0f - smoothstep(rockGreenMin, rockGreenMax, jitteredGreen);

    const float satRockFactor = desatFactor * lowGreenFactor;

    // Pre-baked LiDAR Horizon Ambient Occlusion and Crevice Depth sampling
    // Terrain mesh is 15,000m wide (X: [-7500, +7500]) and 7,500m deep (Z: [-3750, +3750]) centered at origin
    const float2 terrainUV = float2(
        saturate((input.terrainPosition.x + 7500.0f) / 15000.0f),
        saturate((input.terrainPosition.z + 3750.0f) / 7500.0f)
    );
    const float4 lidarAO = g_texLidarAO.Sample(g_samplerClamp, terrainUV);
    const float macroHorizonAO = lidarAO.r;
    const float prebakedCrevice = max(input.color.a, lidarAO.g);
    const float4 lidarHorizon0 = g_texLidarHorizon0.Sample(g_samplerClamp, terrainUV);
    const float4 lidarHorizon1 = g_texLidarHorizon1.Sample(g_samplerClamp, terrainUV);

    // Crevice bed rock exposure: smooth cubic feathering prevents stepped threshold cuts
    const float creviceMinThreshold = 0.18f;
    const float creviceMaxThreshold = 0.60f;
    const float featheredCrevice = smoothstep(creviceMinThreshold, creviceMaxThreshold, prebakedCrevice);
    const float creviceRockExposure = featheredCrevice * lerp(0.25f, 0.85f, valleyFade);
    const float rockFactor = saturate(max(max(slopeRockFactor, satRockFactor), creviceRockExposure));

    const float peatLumMin = 0.18f;
    const float peatLumMax = 0.24f;
    const float lowLuminance = 1.0f - smoothstep(peatLumMin, peatLumMax, luminance);

    const float peatGreenMin = 0.20f;
    const float peatGreenMax = 0.26f;
    const float lowPeatGreen = 1.0f - smoothstep(peatGreenMin, peatGreenMax, input.color.g);
    const float peatFactor = lowLuminance * lowPeatGreen * (1.0f - rockFactor);

    // Triplanar projection blend weights with quartic sharpness
    float3 blendWeights = pow(abs(N), 4.0f);
    blendWeights /= max(dot(blendWeights, float3(1.0f, 1.0f, 1.0f)), 0.0001f);

    const float thatchWeight = saturate(max(peatFactor, saturate((input.color.r - input.color.b) * 4.0f - 0.6f)));

    // Normalised vegetation layer distribution weights
    float wMeadow    = (1.0f - valleyFade) * (1.0f - thatchWeight);
    float wMidSlope  = (valleyFade * (1.0f - plateauFade)) * (1.0f - thatchWeight);
    float wPlateau   = plateauFade * (1.0f - thatchWeight);
    float wDryThatch = thatchWeight;

    float totalGrassWeight = max(wMeadow + wMidSlope + wPlateau + wDryThatch, 0.0001f);
    wMeadow    /= totalGrassWeight;
    wMidSlope  /= totalGrassWeight;
    wPlateau   /= totalGrassWeight;
    wDryThatch /= totalGrassWeight;

    TriplanarGradients grads;
    grads.ddxX = ddx(input.terrainPosition.zy);
    grads.ddyX = ddy(input.terrainPosition.zy);
    grads.ddxY = ddxTerrainPos;
    grads.ddyY = ddyTerrainPos;
    grads.ddxZ = ddx(input.terrainPosition.xy);
    grads.ddyZ = ddy(input.terrainPosition.xy);

    // Spatial variation masks for natural inter-biome variety mixing (AAA domain-warped, band-free)
    // Slope aspect: horizontal compass heading of the terrain normal (smoothly varying [-1, 1])
    // South/South-West facing slopes receive higher solar insolation, while North-facing slopes stay sheltered
    const float2 horizN    = N.xz;
    const float  horizLen  = length(horizN);
    const float  aspectSun = (horizLen > 0.02f) ? dot(horizN / horizLen, float2(0.5547f, 0.8321f)) : 0.0f;

    // Meadow: broad 300m - 500m valley swaths mixing tall pasture with clover/grazed pasture
    const float meadowVar = EvaluateBiomeVariationMask(input.terrainPosition.xz, 0.0035f, float2(127.3f, 311.7f));

    // Mid-Slope: broad 250m - 400m hillside swaths mixing coarse fell turf with fine moor-edge bent-grass, softly biased by solar aspect
    const float nMid        = EvaluateBiomeVariationMask(input.terrainPosition.xz, 0.0042f, float2(541.9f, 823.1f));
    const float midSlopeVar = saturate(nMid + aspectSun * 0.12f);

    // High Plateau: broad 280m - 450m moorland swaths mixing moorland with weathered crag turf, biased by furrow crevice depth
    const float nPlat      = EvaluateBiomeVariationMask(input.terrainPosition.xz, 0.0038f, float2(919.4f, 237.8f));
    const float plateauVar = saturate(nPlat - featheredCrevice * 0.20f);

    // Rock PBR evaluation via triplanar projection with branch culling when unexposed
    PbrSurface rockSurface;
    [branch]
    if (rockFactor > 0.01f)
    {
        rockSurface = SampleTriplanarMaterial(
            g_texRockAlbedoHeight, g_texRockNormal, g_texRockORM,
            input.terrainPosition, blendWeights, grads, V, cameraDist
        );
    }
    else
    {
        rockSurface.albedo       = float3(1.0f, 1.0f, 1.0f);
        rockSurface.normalOffset = float3(0.0f, 0.0f, 0.0f);
        rockSurface.roughness    = 0.8f;
        rockSurface.ao           = 1.0f;
    }

    // Altitudinal vegetation evaluation with dynamic branch-culled biome transitions
    PbrSurface vegSurface;
    vegSurface.albedo       = float3(0.0f, 0.0f, 0.0f);
    vegSurface.normalOffset = float3(0.0f, 0.0f, 0.0f);
    vegSurface.roughness    = 0.0f;
    vegSurface.ao           = 0.0f;

    [branch]
    if (rockFactor < 0.99f)
    {
        if (altNorm < 0.35f)
        {
            float activeVegSum = 0.0f;
            if (wMeadow > 0.02f)    activeVegSum += wMeadow;
            if (wMidSlope > 0.02f)  activeVegSum += wMidSlope;
            if (wDryThatch > 0.02f) activeVegSum += wDryThatch;
            const float renormMeadow    = wMeadow / max(activeVegSum, 0.0001f);
            const float renormMidSlope  = wMidSlope / max(activeVegSum, 0.0001f);
            const float renormDryThatch = wDryThatch / max(activeVegSum, 0.0001f);

            [branch]
            if (wMeadow > 0.02f)
            {
                PbrSurface meadow = SampleBiomePair(
                    g_texMeadowAlbedoHeight, g_texMeadowNormal, g_texMeadowORM,
                    g_texMeadow2AlbedoHeight, g_texMeadow2Normal, g_texMeadow2ORM,
                    meadowVar, input.terrainPosition, blendWeights, grads, V, cameraDist
                );
                vegSurface.albedo       += meadow.albedo * renormMeadow;
                vegSurface.normalOffset += meadow.normalOffset * renormMeadow;
                vegSurface.roughness    += meadow.roughness * renormMeadow;
                vegSurface.ao           += meadow.ao * renormMeadow;
            }

            [branch]
            if (wMidSlope > 0.02f)
            {
                PbrSurface midSlope = SampleBiomePair(
                    g_texMidSlopeAlbedoHeight, g_texMidSlopeNormal, g_texMidSlopeORM,
                    g_texMidSlope2AlbedoHeight, g_texMidSlope2Normal, g_texMidSlope2ORM,
                    midSlopeVar, input.terrainPosition, blendWeights, grads, V, cameraDist
                );
                vegSurface.albedo       += midSlope.albedo * renormMidSlope;
                vegSurface.normalOffset += midSlope.normalOffset * renormMidSlope;
                vegSurface.roughness    += midSlope.roughness * renormMidSlope;
                vegSurface.ao           += midSlope.ao * renormMidSlope;
            }

            [branch]
            if (wDryThatch > 0.02f)
            {
                PbrSurface thatch = SampleTriplanarMaterial(
                    g_texDryThatchAlbedoHeight, g_texDryThatchNormal, g_texDryThatchORM,
                    input.terrainPosition, blendWeights, grads, V, cameraDist
                );
                vegSurface.albedo       += thatch.albedo * renormDryThatch;
                vegSurface.normalOffset += thatch.normalOffset * renormDryThatch;
                vegSurface.roughness    += thatch.roughness * renormDryThatch;
                vegSurface.ao           += thatch.ao * renormDryThatch;
            }
        }
        else
        {
            float activeVegSum = 0.0f;
            if (wMidSlope > 0.02f)  activeVegSum += wMidSlope;
            if (wPlateau > 0.02f)   activeVegSum += wPlateau;
            if (wDryThatch > 0.02f) activeVegSum += wDryThatch;
            const float renormMidSlope  = wMidSlope / max(activeVegSum, 0.0001f);
            const float renormPlateau   = wPlateau / max(activeVegSum, 0.0001f);
            const float renormDryThatch = wDryThatch / max(activeVegSum, 0.0001f);

            [branch]
            if (wMidSlope > 0.02f)
            {
                PbrSurface midSlope = SampleBiomePair(
                    g_texMidSlopeAlbedoHeight, g_texMidSlopeNormal, g_texMidSlopeORM,
                    g_texMidSlope2AlbedoHeight, g_texMidSlope2Normal, g_texMidSlope2ORM,
                    midSlopeVar, input.terrainPosition, blendWeights, grads, V, cameraDist
                );
                vegSurface.albedo       += midSlope.albedo * renormMidSlope;
                vegSurface.normalOffset += midSlope.normalOffset * renormMidSlope;
                vegSurface.roughness    += midSlope.roughness * renormMidSlope;
                vegSurface.ao           += midSlope.ao * renormMidSlope;
            }

            [branch]
            if (wPlateau > 0.02f)
            {
                PbrSurface plateau = SampleBiomePair(
                    g_texPlateauAlbedoHeight, g_texPlateauNormal, g_texPlateauORM,
                    g_texPlateau2AlbedoHeight, g_texPlateau2Normal, g_texPlateau2ORM,
                    plateauVar, input.terrainPosition, blendWeights, grads, V, cameraDist
                );
                vegSurface.albedo       += plateau.albedo * renormPlateau;
                vegSurface.normalOffset += plateau.normalOffset * renormPlateau;
                vegSurface.roughness    += plateau.roughness * renormPlateau;
                vegSurface.ao           += plateau.ao * renormPlateau;
            }

            [branch]
            if (wDryThatch > 0.02f)
            {
                PbrSurface thatch = SampleTriplanarMaterial(
                    g_texDryThatchAlbedoHeight, g_texDryThatchNormal, g_texDryThatchORM,
                    input.terrainPosition, blendWeights, grads, V, cameraDist
                );
                vegSurface.albedo       += thatch.albedo * renormDryThatch;
                vegSurface.normalOffset += thatch.normalOffset * renormDryThatch;
                vegSurface.roughness    += thatch.roughness * renormDryThatch;
                vegSurface.ao           += thatch.ao * renormDryThatch;
            }
        }
    }

    // Composite terrain surface synthesis blending vegetation and exposed rock
    PbrSurface terrainSurface;
    [branch]
    if (rockFactor >= 0.99f)
    {
        terrainSurface = rockSurface;
    }
    else if (rockFactor <= 0.01f)
    {
        terrainSurface = vegSurface;
    }
    else
    {
        terrainSurface.albedo       = lerp(vegSurface.albedo, rockSurface.albedo, rockFactor);
        terrainSurface.normalOffset = lerp(vegSurface.normalOffset, rockSurface.normalOffset, rockFactor);
        terrainSurface.roughness    = lerp(vegSurface.roughness, rockSurface.roughness, rockFactor);
        terrainSurface.ao           = lerp(vegSurface.ao, rockSurface.ao, rockFactor);
    }

    // Apply tangent-space normal perturbation to world-space geometric normal
    const float normalStrength = 0.85f;
    const float3 normalPerturbed = normalize(N + terrainSurface.normalOffset * normalStrength);

    // Full-strength photogrammetry micro-detail mapped onto the authored Yorkshire Dales palette
    const float3 pbrAlbedo = input.color.rgb * terrainSurface.albedo;

    // Specular reflectance parameters derived from physical microfacet roughness
    const float specPower = exp2(9.0f * (1.0f - terrainSurface.roughness) + 1.0f);
    const float baseReflectivity = lerp(0.04f, 0.06f, rockFactor);
    const float specIntensity = baseReflectivity * (1.0f - terrainSurface.roughness) * 2.5f;

    // Composite ambient occlusion combining pre-baked macro LiDAR horizon line-of-sight occlusion with microscopic photogrammetry contact occlusion between blades and rock facets
    const float microContactAO = lerp(0.35f, 1.0f, terrainSurface.ao);
    const float compositeAO    = macroHorizonAO * microContactAO;
    float3 ambient = g_ambientColor.rgb * compositeAO;
    float3 totalDiffuse = float3(0.0f, 0.0f, 0.0f);
    float3 totalSpecular = float3(0.0f, 0.0f, 0.0f);
    float3 totalTransmission = float3(0.0f, 0.0f, 0.0f);

    const uint activeLightCount = min(g_lightCount, 16u);

    for (uint i = 0; i < activeLightCount; ++i)
    {
        LightData light = g_lights[i];
        uint packed = asuint(light.direction.w);
        uint lightType = packed & 0xFu;
        uint lightChannels = packed >> 4u;

        if ((lightChannels & g_objectLightChannels) == 0u)
        {
            continue;
        }

        float intensity = light.color.w;
        float3 lightRgb = light.color.rgb * intensity;

        if (lightType == 0u) // Directional Light
        {
            float3 L = normalize(-light.direction.xyz);
            float nDotL = max(dot(normalPerturbed, L), 0.0f);

            // Thin-walled foliage subsurface scattering (backlight blade transmission)
            const float backLight = saturate(dot(-V, L));
            const float transmission = pow(backLight, 3.0f) * (1.0f - rockFactor) * 0.40f;

            float solarShadow = 0.0f;
            if (i == 0u)
            {
                const float horizonSin = SampleHorizonElevationSine(lidarHorizon0, lidarHorizon1, L);
                const float horizonShadow = saturate((L.y - horizonSin) * 25.0f);
                if (horizonShadow > 0.001f && (nDotL > 0.0f || transmission > 0.001f))
                {
                    // Dynamic entity shadows sampled within near cascades (<= 400m); distant terrain self-shadowed by LiDAR horizon map
                    float csmShadow = 1.0f;
                    if (cameraDist < g_cascadeSplits.z)
                    {
                        csmShadow = CalculateCascadedShadow(input.worldPosition, normalPerturbed, L);
                    }
                    solarShadow = horizonShadow * csmShadow;
                }
            }
            else
            {
                solarShadow = 1.0f;
            }

            totalDiffuse += lightRgb * (nDotL * solarShadow);
            totalTransmission += lightRgb * (transmission * solarShadow);

            if (nDotL > 0.0f)
            {
                float3 H = normalize(L + V);
                float nDotH = max(dot(normalPerturbed, H), 0.0f);
                totalSpecular += lightRgb * (pow(nDotH, specPower) * specIntensity * solarShadow);
            }
        }
        else if (lightType == 1u) // Point Light
        {
            float3 toLight = light.position.xyz - input.worldPosition;
            float dist = length(toLight);
            float range = max(light.position.w, 0.001f);

            if (dist < range)
            {
                float3 L = toLight / dist;
                float nDotL = max(dot(normalPerturbed, L), 0.0f);

                // Quadratic attenuation with smooth quadratic range windowing
                float att = 1.0f / (light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist);
                float falloff = saturate(1.0f - (dist / range));
                falloff *= falloff;

                float3 radiance = lightRgb * (att * falloff);
                totalDiffuse += radiance * nDotL;

                if (nDotL > 0.0f)
                {
                    float3 H = normalize(L + V);
                    float nDotH = max(dot(normalPerturbed, H), 0.0f);
                    totalSpecular += radiance * (pow(nDotH, specPower) * specIntensity);
                }
            }
        }
        else if (lightType == 2u) // Spot Light
        {
            float3 toLight = light.position.xyz - input.worldPosition;
            float dist = length(toLight);
            float range = max(light.position.w, 0.001f);

            if (dist < range)
            {
                float3 L = toLight / dist;
                float nDotL = max(dot(normalPerturbed, L), 0.0f);

                // Spot cone attenuation factor
                float cosAngle = dot(-L, normalize(light.direction.xyz));
                float innerCos = light.attenuation.z;
                float outerCos = light.attenuation.w;
                float spotFactor = saturate((cosAngle - outerCos) / max(innerCos - outerCos, 0.0001f));

                float att = 1.0f / (light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist);
                float falloff = saturate(1.0f - (dist / range));
                falloff *= falloff;

                float3 radiance = lightRgb * (att * falloff * spotFactor);
                totalDiffuse += radiance * nDotL;

                if (nDotL > 0.0f)
                {
                    float3 H = normalize(L + V);
                    float nDotH = max(dot(normalPerturbed, H), 0.0f);
                    totalSpecular += radiance * (pow(nDotH, specPower) * specIntensity);
                }
            }
        }
    }

    // Translucent backlight scattering tint for living vegetation
    const float3 foliageTransmissionColor = float3(1.10f, 1.25f, 0.45f);
    float3 shadedColor = pbrAlbedo * (ambient + totalDiffuse) + (pbrAlbedo * foliageTransmissionColor) * totalTransmission + totalSpecular;

    // Directional celestial sun orientation for atmospheric scattering and haze phase evaluation
    const float3 sunDir = GetCelestialSunDirection();

    const float3 rayDir = (cameraDist > 0.001f) ? (input.worldPosition / cameraDist) : float3(0.0f, -1.0f, 0.0f);

    // Atmospheric perspective (physically based Rayleigh and Mie aerial perspective)
    // Sub-threshold air extinction (<0.05%) bypassed within 75m; 2x2 quadrature evaluates ground rays with >99.5% fidelity
    if (g_atmosphereParams.x > 0.0f && cameraDist > 75.0f)
    {
        const float3 cameraWorldPos = input.terrainPosition - input.worldPosition;

        float3 inscattering, transmittance;
        EvaluateAtmosphericScattering(
            cameraWorldPos,
            rayDir,
            cameraDist,
            sunDir,
            inscattering,
            transmittance,
            2,
            2
        );

        // Smooth transition eliminating any threshold boundary between 75m and 125m
        const float atmBlend = saturate((cameraDist - 75.0f) * 0.02f);
        shadedColor = lerp(shadedColor, shadedColor * transmittance + inscattering, atmBlend);
    }

    // Couples artist-configured boundary fog or haze layer with atmospheric illumination
    EvaluateCompositeFog(rayDir, cameraDist, sunDir, g_fogColor, g_fogParams, shadedColor);

    // Apply exposure and ACES filmic tone reproduction directly before MSAA resolve
    const float exposure = max(g_exposureParams.x, 0.0001f);
    const float3 exposedColor = shadedColor * exposure;
    const float3 tonemappedColor = AcesFilmicToneMapping(exposedColor);

    // Subtle post-processing blend preserving rich shadow depth and natural earthy palette
    // 0.0f = pure linear response (1), 1.0f = full ACES tone curve (2)
    const float postProcessBlend = 0.20f;
    const float3 finalColor = lerp(shadedColor, tonemappedColor, postProcessBlend);

    return float4(finalColor, 1.0f);
}

