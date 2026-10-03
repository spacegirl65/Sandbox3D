// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "SceneBuffers.hlsli"

// Terrain PBR material textures bound to root descriptor table slots t1 through t20
// Biome 0: Meadow and alluvial valley pasture (uncut_grass_oilpt20_4k)
Texture2D g_texMeadowAlbedo    : register(t1);
Texture2D g_texMeadowNormal    : register(t2);
Texture2D g_texMeadowRoughness : register(t3);
Texture2D g_texMeadowAO        : register(t4);

// Biome 1: Mid-slope fell turf (wild_grass_umjlabus_4k)
Texture2D g_texMidSlopeAlbedo    : register(t5);
Texture2D g_texMidSlopeNormal    : register(t6);
Texture2D g_texMidSlopeRoughness : register(t7);
Texture2D g_texMidSlopeAO        : register(t8);

// Biome 2: High plateau moorland (wild_grass_vbslfeqfw_4k)
Texture2D g_texPlateauAlbedo    : register(t9);
Texture2D g_texPlateauNormal    : register(t10);
Texture2D g_texPlateauRoughness : register(t11);
Texture2D g_texPlateauAO        : register(t12);

// Biome 3: Dry thatch knolls and weathered ridges (grass_dried_olqkj0_4k)
Texture2D g_texDryThatchAlbedo    : register(t13);
Texture2D g_texDryThatchNormal    : register(t14);
Texture2D g_texDryThatchRoughness : register(t15);
Texture2D g_texDryThatchAO        : register(t16);

// Biome 4: Crevice, scar, and sheer cliff rock (rock_cliff_vl3ibcxlw_4k)
Texture2D g_texRockAlbedo    : register(t17);
Texture2D g_texRockNormal    : register(t18);
Texture2D g_texRockRoughness : register(t19);
Texture2D g_texRockAO        : register(t20);

// Displacement heightmaps for Parallax Occlusion Mapping (POM)
Texture2D g_texMeadowDisp    : register(t21);
Texture2D g_texMidSlopeDisp  : register(t22);
Texture2D g_texPlateauDisp   : register(t23);
Texture2D g_texDryThatchDisp : register(t24);
Texture2D g_texRockDisp      : register(t25);

// Static anisotropic 16x wrap sampler
SamplerState g_samplerAniso : register(s0);

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

// Dual-frequency planar PBR projection evaluator with Parallax Occlusion Mapping
PbrSurface SamplePlanePbr(
    Texture2D albedoTex,
    Texture2D normalTex,
    Texture2D roughnessTex,
    Texture2D aoTex,
    Texture2D dispTex,
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

    // Parallax Occlusion Mapping on primary micro layer evaluating optical self-occlusion
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
        float h = dispTex.SampleGrad(g_samplerAniso, currentUv, ddxUv * scaleA, ddyUv * scaleA).r;

        [unroll(8)]
        for (uint s = 0; s < numSteps; ++s)
        {
            if (h >= currentLayer)
                break;
            currentLayer -= stepSize;
            currentUv += uvDelta;
            h = dispTex.SampleGrad(g_samplerAniso, currentUv, ddxUv * scaleA, ddyUv * scaleA).r;
        }

        float prevLayer = currentLayer + stepSize;
        float prevH = dispTex.SampleGrad(g_samplerAniso, currentUv - uvDelta, ddxUv * scaleA, ddyUv * scaleA).r;
        float nextDiff = h - currentLayer;
        float prevDiff = prevLayer - prevH;
        float weight = saturate(nextDiff / max(nextDiff + prevDiff, 0.0001f));
        pomUvA = lerp(currentUv, currentUv - uvDelta, weight);
    }

    float3 albedoA = albedoTex.SampleGrad(g_samplerAniso, pomUvA, ddxUv * scaleA, ddyUv * scaleA).rgb;
    float3 albedoB = albedoTex.SampleGrad(g_samplerAniso, uv * scaleB, ddxUv * scaleB, ddyUv * scaleB).rgb;
    float2 normA   = normalTex.SampleGrad(g_samplerAniso, pomUvA, ddxUv * scaleA, ddyUv * scaleA).rg;
    float  roughA  = roughnessTex.SampleGrad(g_samplerAniso, pomUvA, ddxUv * scaleA, ddyUv * scaleA).r;
    float  aoA     = aoTex.SampleGrad(g_samplerAniso, pomUvA, ddxUv * scaleA, ddyUv * scaleA).r;

    // Convert sampled albedo back from hardware-linearised sRGB to display gamma space
    albedoA = pow(max(albedoA, 0.0001f), 1.0f / 2.2f);
    albedoB = pow(max(albedoB, 0.0001f), 1.0f / 2.2f);

    // Relative detail ratio: spatial mean is strictly (1, 1, 1), preserving the authored palette at distance
    // whilst capturing high-frequency micro-contrast, leaf edges, and dead thatch up close
    const float3 detailRatio = clamp(albedoA / max(albedoB, 0.05f), 0.45f, 1.75f);

    // Distance-based micro-detail attenuation
    // Smoothly fade micro-detail ratio, high-frequency normal perturbations, and micro-AO between 15m and 45m
    const float nearDistance = 15.0f;
    const float farDistance  = 45.0f;
    const float microFade    = 1.0f - smoothstep(nearDistance, farDistance, cameraDist);

    const float3 effectiveDetail = lerp(float3(1.0f, 1.0f, 1.0f), detailRatio, microFade);
    const float  effectiveAo     = lerp(1.0f, aoA, microFade);

    PbrSurface result;
    result.albedo    = effectiveDetail;
    result.roughness = roughA;
    result.ao        = effectiveAo;

    // Convert planar tangent-space normal offsets to world-space normal perturbations
    float2 tanXY = (normA * 2.0f - 1.0f) * microFade;
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

// Triplanar material projection synthesiser
PbrSurface SampleTriplanarMaterial(
    Texture2D albedoTex,
    Texture2D normalTex,
    Texture2D roughnessTex,
    Texture2D aoTex,
    Texture2D dispTex,
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

    PbrSurface sampleX = SamplePlanePbr(albedoTex, normalTex, roughnessTex, aoTex, dispTex, terrainPos.zy, grads.ddxX, grads.ddyX, vTanX, cameraDist, 0);
    PbrSurface sampleY = SamplePlanePbr(albedoTex, normalTex, roughnessTex, aoTex, dispTex, terrainPos.xz, grads.ddxY, grads.ddyY, vTanY, cameraDist, 1);
    PbrSurface sampleZ = SamplePlanePbr(albedoTex, normalTex, roughnessTex, aoTex, dispTex, terrainPos.xy, grads.ddxZ, grads.ddyZ, vTanZ, cameraDist, 2);

    PbrSurface result;
    result.albedo       = sampleX.albedo * blendWeights.x + sampleY.albedo * blendWeights.y + sampleZ.albedo * blendWeights.z;
    result.normalOffset = sampleX.normalOffset * blendWeights.x + sampleY.normalOffset * blendWeights.y + sampleZ.normalOffset * blendWeights.z;
    result.roughness    = sampleX.roughness * blendWeights.x + sampleY.roughness * blendWeights.y + sampleZ.roughness * blendWeights.z;
    result.ao           = sampleX.ao * blendWeights.x + sampleY.ao * blendWeights.y + sampleZ.ao * blendWeights.z;
    return result;
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

    // Continuous slope and desaturation factors preventing sharp specular threshold facets
    const float rockSlopeMinNy = 0.70f;
    const float rockSlopeMaxNy = 0.85f;
    const float slopeRockFactor = 1.0f - smoothstep(rockSlopeMinNy, rockSlopeMaxNy, N.y);

    const float desatMin = 0.04f;
    const float desatMax = 0.12f;
    const float desatFactor = 1.0f - smoothstep(desatMin, desatMax, colorSaturation);

    const float rockGreenMin = 0.50f;
    const float rockGreenMax = 0.65f;
    const float lowGreenFactor = 1.0f - smoothstep(rockGreenMin, rockGreenMax, input.color.g);

    const float satRockFactor = desatFactor * lowGreenFactor;
    const float rockFactor = saturate(max(slopeRockFactor, satRockFactor));

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

    // Altitudinal zone evaluation across procedural and LiDAR height ranges
    float altNorm = (input.terrainPosition.y > 100.0f)
        ? saturate((input.terrainPosition.y - 100.0f) / 450.0f)
        : saturate((input.terrainPosition.y + 4.0f) / 93.6f);

    const float valleyFade = smoothstep(0.18f, 0.26f, altNorm);
    const float plateauFade = smoothstep(0.48f, 0.58f, altNorm);
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
    grads.ddxY = ddx(input.terrainPosition.xz);
    grads.ddyY = ddy(input.terrainPosition.xz);
    grads.ddxZ = ddx(input.terrainPosition.xy);
    grads.ddyZ = ddy(input.terrainPosition.xy);

    // Rock PBR evaluation via full triplanar projection across all spatial planes
    PbrSurface rockSurface = SampleTriplanarMaterial(
        g_texRockAlbedo, g_texRockNormal, g_texRockRoughness, g_texRockAO, g_texRockDisp,
        input.terrainPosition, blendWeights, grads, V, cameraDist
    );

    // Altitudinal vegetation evaluation with smooth biome transitions
    PbrSurface vegSurface;
    if (altNorm < 0.35f)
    {
        PbrSurface meadow = SampleTriplanarMaterial(
            g_texMeadowAlbedo, g_texMeadowNormal, g_texMeadowRoughness, g_texMeadowAO, g_texMeadowDisp,
            input.terrainPosition, blendWeights, grads, V, cameraDist
        );
        PbrSurface midSlope = SampleTriplanarMaterial(
            g_texMidSlopeAlbedo, g_texMidSlopeNormal, g_texMidSlopeRoughness, g_texMidSlopeAO, g_texMidSlopeDisp,
            input.terrainPosition, blendWeights, grads, V, cameraDist
        );
        PbrSurface thatch = SampleTriplanarMaterial(
            g_texDryThatchAlbedo, g_texDryThatchNormal, g_texDryThatchRoughness, g_texDryThatchAO, g_texDryThatchDisp,
            input.terrainPosition, blendWeights, grads, V, cameraDist
        );

        vegSurface.albedo       = meadow.albedo * wMeadow + midSlope.albedo * wMidSlope + thatch.albedo * wDryThatch;
        vegSurface.normalOffset = meadow.normalOffset * wMeadow + midSlope.normalOffset * wMidSlope + thatch.normalOffset * wDryThatch;
        vegSurface.roughness    = meadow.roughness * wMeadow + midSlope.roughness * wMidSlope + thatch.roughness * wDryThatch;
        vegSurface.ao           = meadow.ao * wMeadow + midSlope.ao * wMidSlope + thatch.ao * wDryThatch;
    }
    else
    {
        PbrSurface midSlope = SampleTriplanarMaterial(
            g_texMidSlopeAlbedo, g_texMidSlopeNormal, g_texMidSlopeRoughness, g_texMidSlopeAO, g_texMidSlopeDisp,
            input.terrainPosition, blendWeights, grads, V, cameraDist
        );
        PbrSurface plateau = SampleTriplanarMaterial(
            g_texPlateauAlbedo, g_texPlateauNormal, g_texPlateauRoughness, g_texPlateauAO, g_texPlateauDisp,
            input.terrainPosition, blendWeights, grads, V, cameraDist
        );
        PbrSurface thatch = SampleTriplanarMaterial(
            g_texDryThatchAlbedo, g_texDryThatchNormal, g_texDryThatchRoughness, g_texDryThatchAO, g_texDryThatchDisp,
            input.terrainPosition, blendWeights, grads, V, cameraDist
        );

        vegSurface.albedo       = midSlope.albedo * wMidSlope + plateau.albedo * wPlateau + thatch.albedo * wDryThatch;
        vegSurface.normalOffset = midSlope.normalOffset * wMidSlope + plateau.normalOffset * wPlateau + thatch.normalOffset * wDryThatch;
        vegSurface.roughness    = midSlope.roughness * wMidSlope + plateau.roughness * wPlateau + thatch.roughness * wDryThatch;
        vegSurface.ao           = midSlope.ao * wMidSlope + plateau.ao * wPlateau + thatch.ao * wDryThatch;
    }

    // Composite terrain surface synthesis blending vegetation and exposed rock
    PbrSurface terrainSurface;
    terrainSurface.albedo       = lerp(vegSurface.albedo, rockSurface.albedo, rockFactor);
    terrainSurface.normalOffset = lerp(vegSurface.normalOffset, rockSurface.normalOffset, rockFactor);
    terrainSurface.roughness    = lerp(vegSurface.roughness, rockSurface.roughness, rockFactor);
    terrainSurface.ao           = lerp(vegSurface.ao, rockSurface.ao, rockFactor);

    // Apply tangent-space normal perturbation to world-space geometric normal
    const float normalStrength = 0.85f;
    const float3 normalPerturbed = normalize(N + terrainSurface.normalOffset * normalStrength);

    // Full-strength photogrammetry micro-detail mapped onto the authored Yorkshire Dales palette
    const float3 pbrAlbedo = input.color.rgb * terrainSurface.albedo;

    // Specular reflectance parameters derived from physical microfacet roughness
    const float specPower = exp2(9.0f * (1.0f - terrainSurface.roughness) + 1.0f);
    const float baseReflectivity = lerp(0.04f, 0.06f, rockFactor);
    const float specIntensity = baseReflectivity * (1.0f - terrainSurface.roughness) * 2.5f;

    // Contact ambient occlusion darkening hollows and crevices between blades
    const float aoFactor = lerp(0.35f, 1.0f, terrainSurface.ao);
    float3 ambient = g_ambientColor.rgb * aoFactor;
    float3 totalDiffuse = float3(0.0f, 0.0f, 0.0f);
    float3 totalSpecular = float3(0.0f, 0.0f, 0.0f);
    float3 totalTransmission = float3(0.0f, 0.0f, 0.0f);

    const uint activeLightCount = min(g_lightCount, 16u);

    for (uint i = 0; i < activeLightCount; ++i)
    {
        LightData light = g_lights[i];
        uint lightType = (uint)light.direction.w;
        float intensity = light.color.w;
        float3 lightRgb = light.color.rgb * intensity;

        if (lightType == 0u) // Directional Light
        {
            float3 L = normalize(-light.direction.xyz);
            float nDotL = max(dot(normalPerturbed, L), 0.0f);
            totalDiffuse += lightRgb * nDotL;

            // Thin-walled foliage subsurface scattering (backlight blade transmission)
            const float backLight = saturate(dot(-V, L));
            const float transmission = pow(backLight, 3.0f) * (1.0f - rockFactor) * 0.40f;
            totalTransmission += lightRgb * transmission;

            if (nDotL > 0.0f)
            {
                float3 H = normalize(L + V);
                float nDotH = max(dot(normalPerturbed, H), 0.0f);
                totalSpecular += lightRgb * (pow(nDotH, specPower) * specIntensity);
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

    // Atmospheric perspective (aerial distance fog)
    if (g_fogColor.a > 0.0f)
    {
        const float fogExtent = max(cameraDist - g_fogParams.x, 0.0f);
        const float opticalDepth = fogExtent * g_fogParams.z;
        const float fogFactor = saturate((1.0f - exp(-opticalDepth * opticalDepth)) * g_fogColor.a);
        shadedColor = lerp(shadedColor, g_fogColor.rgb, fogFactor);
    }

    return float4(shadedColor, input.color.a);
}

