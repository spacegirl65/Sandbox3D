// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Atmosphere.hlsli"
#include "Space.hlsli"

struct SkyVertexOutput
{
    float4 position : SV_POSITION;
    float3 viewDir  : TEXCOORD0;
};

// Full-screen triangle covering the screen viewport at Reverse-Z depth 0.0 (infinite distance)
SkyVertexOutput VSMain(uint vertexId : SV_VertexID)
{
    SkyVertexOutput output;

    // Generates a full-screen triangle covering [-1, 1] in clip space
    // Vertex 0: (-1, -1), Vertex 1: (-1, 3), Vertex 2: (3, -1)
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    float2 clipPos = uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);

    // In a Reverse-Z projection pipeline, 0.0f represents infinite distance (far clip plane)
    output.position = float4(clipPos, 0.0f, 1.0f);

    // Unproject the clip position into camera view space using the inverse projection matrix
    float invProjX = 1.0f / max(g_mvp._m00, 0.0001f);
    float invProjY = 1.0f / max(g_mvp._m11, 0.0001f);
    float3 viewRayCamera = float3(clipPos.x * invProjX, clipPos.y * invProjY, 1.0f);

    // Rotate view ray from camera space into world space using the camera's world orientation
    output.viewDir = mul(float4(viewRayCamera, 0.0f), g_world).xyz;

    return output;
}

// Pixel shader evaluating the universal deep-space cosmic background and planetary atmosphere
float4 PSMain(SkyVertexOutput input) : SV_TARGET
{
    // Reconstruct normalized celestial view direction across 4pi steradians
    const float3 viewDir = normalize(input.viewDir);

    // Identify primary celestial sun orientation
    const float3 sunDir = GetCelestialSunDirection();

    // Astronomical solar distance in Astronomical Units (AU), defaulting to 1.0 AU when unspecified
    const float solarDistAU = (g_adaptationParams.z > 0.0001f) ? g_adaptationParams.z : 1.0f;

    // Deep space mode: when planetary radius is zero (or negative), evaluate pure celestial vacuum
    const bool isDeepSpace = (g_atmosphereParams.x <= 0.0f);

    float3 finalRadiance = float3(0.0f, 0.0f, 0.0f);

    if (isDeepSpace)
    {
        // Unobstructed deep-space cosmic sphere (stars, Milky Way, and distance-scaled Sun)
        finalRadiance = EvaluateCosmicRadiance(viewDir, sunDir, solarDistAU);
    }
    else
    {
        // Planetary surface / orbital atmospheric composite
        const float cameraAltitude = max(g_cameraPosition.y, 0.0f) + 1.0f;
        const float3 cameraWorldPos = float3(0.0f, cameraAltitude, 0.0f);

        const float lenXZ = length(viewDir.xz);
        const float3 domeViewDir = (viewDir.y >= 0.002f)
            ? viewDir
            : ((lenXZ > 0.0001f) ? normalize(float3(viewDir.x, 0.002f, viewDir.z)) : float3(0.0f, 1.0f, 0.0f));

        float3 skyInscattering = EvaluateSkyRadiance(cameraWorldPos, domeViewDir, sunDir, 16);

        // Evaluate atmospheric optical transmittance along the view ray to naturally attenuate cosmic background
        float3 planetCenter = float3(0.0f, -g_atmosphereParams.x, 0.0f);
        float tMin, tMax;
        float3 atmosphereTransmittance = float3(1.0f, 1.0f, 1.0f);
        if (RayAtmosphereBounds(cameraWorldPos, domeViewDir, planetCenter, g_atmosphereParams.x, g_atmosphereParams.y, tMin, tMax))
        {
            float3 rayStart = cameraWorldPos + domeViewDir * tMin;
            float rayLength = min(tMax - tMin, 90000.0f);
            float3 stepInscatter;
            EvaluateAtmosphericScattering(rayStart, domeViewDir, rayLength, sunDir, stepInscatter, atmosphereTransmittance, 8);
        }

        // Evaluate cosmic background (stars, Milky Way, and solar disc) transmitted through atmosphere
        const float3 cosmicRadiance = EvaluateCosmicRadiance(domeViewDir, sunDir, solarDistAU);
        float3 skyRadiance = skyInscattering + cosmicRadiance * atmosphereTransmittance;

        // Ground-level nadir dissolution for planar terrain demo patch
        if (viewDir.y < -0.02f)
        {
            const float3 orthoSun = cross(sunDir, float3(0.0f, 1.0f, 0.0f));
            const float lenOrtho = length(orthoSun.xz);
            const float2 neutralAzimuth = (lenOrtho > 0.0001f) ? (orthoSun.xz / lenOrtho) : float2(1.0f, 0.0f);
            const float3 neutralHorizonDir = normalize(float3(neutralAzimuth.x, 0.002f, neutralAzimuth.y));
            const float3 neutralHorizonRadiance = EvaluateSkyRadiance(cameraWorldPos, neutralHorizonDir, sunDir, 8);

            const float3 nadirRadiance = lerp(neutralHorizonRadiance, g_ambientColor.rgb * 0.35f, 0.50f);
            const float nadirBlend = smoothstep(0.0f, 1.0f, saturate((-viewDir.y - 0.02f) / 0.43f));
            skyRadiance = lerp(skyRadiance, nadirRadiance, nadirBlend);
        }

        finalRadiance = skyRadiance;
    }

    // Apply exposure and ACES filmic tone reproduction directly before MSAA resolve
    const float exposure = max(g_exposureParams.x, 0.0001f);
    const float3 exposedRadiance = finalRadiance * exposure;
    const float3 tonemappedRadiance = AcesFilmicToneMapping(exposedRadiance);

    // Subtle +/-0.5 LSB triangular dither to eliminate 8-bit banding in dark space and solar glare gradients
    const float ditherSeedA = frac(sin(dot(input.position.xy, float2(12.9898f, 78.233f))) * 43758.5453f);
    const float ditherSeedB = frac(sin(dot(input.position.xy + float2(1.0f, 1.0f), float2(12.9898f, 78.233f))) * 43758.5453f);
    const float dither = (ditherSeedA + ditherSeedB - 1.0f) * (0.5f / 255.0f);
    const float hasSignal = step(0.0001f, dot(tonemappedRadiance, float3(1.0f, 1.0f, 1.0f)));

    return float4(saturate(tonemappedRadiance + dither * hasSignal), 1.0f);
}
