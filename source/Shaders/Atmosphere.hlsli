// Copyright © 2026 spacegirl65. All Rights Reserved.

#ifndef ATMOSPHERE_HLSLI
#define ATMOSPHERE_HLSLI

#include "SceneBuffers.hlsli"

static const float ATMOSPHERE_PI = 3.14159265358979323846f;

// Identifies the celestial sun vector among multiple scene lights, filtering for directional lights affecting the scene
float3 GetCelestialSunDirection()
{
    for (uint i = 0; i < g_lightCount; ++i)
    {
        uint lightType = asuint(g_lights[i].direction.w) & 0xFu;
        if (lightType == 0u) // LightType::Directional
        {
            // Directional celestial sun shines downwards from the sky (direction.y < 0)
            if (g_lights[i].direction.y < 0.0f)
            {
                return -normalize(g_lights[i].direction.xyz);
            }
        }
    }

    return normalize(float3(0.35f, 0.92f, 0.18f));
}

// Analytical ray-sphere intersection testing entry and exit parameters
bool RaySphereIntersect(
    float3 rayOrigin,
    float3 rayDir,
    float3 sphereCenter,
    float radius,
    out float t0,
    out float t1
)
{
    float3 p = rayOrigin - sphereCenter;
    float b = dot(p, rayDir);
    float c = dot(p, p) - radius * radius;
    float discriminant = b * b - c;

    if (discriminant < 0.0f)
    {
        t0 = -1.0f;
        t1 = -1.0f;
        return false;
    }

    float sqrtD = sqrt(discriminant);
    t0 = -b - sqrtD;
    t1 = -b + sqrtD;
    return true;
}

// Determines ray entry and exit distances through the planetary atmosphere shell
bool RayAtmosphereBounds(
    float3 rayOrigin,
    float3 rayDir,
    float3 planetCenter,
    float planetRadius,
    float atmRadius,
    out float tMin,
    out float tMax
)
{
    float tAtm0, tAtm1;
    if (!RaySphereIntersect(rayOrigin, rayDir, planetCenter, atmRadius, tAtm0, tAtm1) || tAtm1 < 0.0f)
    {
        tMin = 0.0f;
        tMax = 0.0f;
        return false;
    }

    tMin = max(tAtm0, 0.0f);
    tMax = tAtm1;

    // Clip maximum ray distance against the solid planetary surface if intersected
    float tGround0, tGround1;
    if (RaySphereIntersect(rayOrigin, rayDir, planetCenter, planetRadius, tGround0, tGround1))
    {
        if (tGround0 > 0.0f && tGround0 < tMax)
        {
            tMax = tGround0;
        }
        else if (tGround1 > 0.0f && tGround0 <= 0.0f)
        {
            tMax = max(tGround0, 0.0f);
        }
    }

    return tMax > tMin;
}

// Evaluates molecular (Rayleigh), aerosol (Mie), and ozone absorption relative densities at altitude (m)
float3 ComputeAtmosphereDensities(
    float altitude,
    float rayleighScaleHeight,
    float mieScaleHeight,
    float ozoneCenterAltitude
)
{
    float clampedAlt = max(altitude, 0.0f);
    float rayleighDensity = exp(-clampedAlt / max(rayleighScaleHeight, 1.0f));
    float mieDensity = exp(-clampedAlt / max(mieScaleHeight, 1.0f));

    // Ozone absorption modeled as a symmetric vertical triangular distribution centered in the stratosphere
    float ozoneWidth = 15000.0f;
    float ozoneDensity = max(0.0f, 1.0f - abs(clampedAlt - ozoneCenterAltitude) / ozoneWidth);

    return float3(rayleighDensity, mieDensity, ozoneDensity);
}

// Rayleigh phase function describing symmetric molecular scattering
float RayleighPhase(float cosTheta)
{
    return (3.0f / (16.0f * ATMOSPHERE_PI)) * (1.0f + cosTheta * cosTheta);
}

// Cornette-Shanks modified Henyey-Greenstein phase function for forward-peaked particulate aerosol scattering
float MiePhase(float cosTheta, float g)
{
    float g2 = g * g;
    float num = 3.0f * (1.0f - g2) * (1.0f + cosTheta * cosTheta);
    float denom = (8.0f * ATMOSPHERE_PI) * (2.0f + g2) * pow(max(1.0f + g2 - 2.0f * g * cosTheta, 0.0001f), 1.5f);
    return num / denom;
}

// Integrates optical depth along a line of sight towards the celestial sun vector
float3 ComputeOpticalDepthToSun(
    float3 samplePos,
    float3 sunDir,
    float3 planetCenter,
    float planetRadius,
    float atmRadius,
    float rayleighScaleHeight,
    float mieScaleHeight,
    float ozoneCenterAltitude,
    int stepCount = 4
)
{
    // If the sun vector intersects the planetary sphere, the sun is occluded below the physical horizon
    float tG0, tG1;
    if (RaySphereIntersect(samplePos, sunDir, planetCenter, planetRadius, tG0, tG1))
    {
        if (tG0 > 0.0f)
        {
            return float3(1e9f, 1e9f, 1e9f);
        }
    }

    float tAtm0, tAtm1;
    if (!RaySphereIntersect(samplePos, sunDir, planetCenter, atmRadius, tAtm0, tAtm1) || tAtm1 <= 0.0f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    float rayLength = tAtm1;
    float stepSize = rayLength / float(stepCount);
    float3 accumulatedDepth = float3(0.0f, 0.0f, 0.0f);

    for (int i = 0; i < stepCount; ++i)
    {
        float3 p = samplePos + sunDir * ((float(i) + 0.5f) * stepSize);
        float alt = length(p - planetCenter) - planetRadius;
        float3 densities = ComputeAtmosphereDensities(alt, rayleighScaleHeight, mieScaleHeight, ozoneCenterAltitude);
        accumulatedDepth += densities * stepSize;
    }

    return accumulatedDepth;
}

// Evaluates spectral transmittance extinction given accumulated optical depths
float3 EvaluateTransmittance(
    float opticalDepthRayleigh,
    float opticalDepthMie,
    float opticalDepthOzone,
    float3 betaRayleigh,
    float3 betaMie,
    float3 betaOzone
)
{
    // Aerosol extinction includes ~11% absorption in addition to pure scattering
    float3 betaMieExt = betaMie * 1.11f;
    float3 extinction = betaRayleigh * opticalDepthRayleigh + betaMieExt * opticalDepthMie + betaOzone * opticalDepthOzone;
    return exp(-max(extinction, 0.0f));
}

// Single-scattering numerical raymarching along arbitrary view rays
void EvaluateAtmosphericScattering(
    float3 rayOrigin,
    float3 rayDir,
    float rayLength,
    float3 sunDir,
    out float3 inscattering,
    out float3 transmittance,
    int stepCount = 12
)
{
    float3 planetCenter = float3(0.0f, -g_atmosphereParams.x, 0.0f);
    float planetRadius = g_atmosphereParams.x;
    float atmRadius = g_atmosphereParams.y;
    float sunIntensity = g_atmosphereParams.z;
    float ozoneCenter = g_atmosphereParams.w;

    float3 betaRayleigh = g_rayleighParams.xyz;
    float rayleighScaleHeight = g_rayleighParams.w;

    float3 betaMie = g_mieParams.xyz;
    float mieScaleHeight = g_mieParams.w;

    float3 betaOzone = g_ozoneParams.xyz;
    float mieAsymmetry = g_ozoneParams.w;

    float cosTheta = dot(rayDir, sunDir);
    float phaseR = RayleighPhase(cosTheta);
    float phaseM = MiePhase(cosTheta, mieAsymmetry);

    float stepSize = rayLength / float(stepCount);
    float3 viewOpticalDepth = float3(0.0f, 0.0f, 0.0f);
    float3 accumulatedInscatter = float3(0.0f, 0.0f, 0.0f);

    for (int i = 0; i < stepCount; ++i)
    {
        float3 samplePos = rayOrigin + rayDir * ((float(i) + 0.5f) * stepSize);
        float sampleAlt = length(samplePos - planetCenter) - planetRadius;
        float3 sampleDensities = ComputeAtmosphereDensities(sampleAlt, rayleighScaleHeight, mieScaleHeight, ozoneCenter);

        viewOpticalDepth += sampleDensities * stepSize;

        float3 sunOpticalDepth = ComputeOpticalDepthToSun(
            samplePos,
            sunDir,
            planetCenter,
            planetRadius,
            atmRadius,
            rayleighScaleHeight,
            mieScaleHeight,
            ozoneCenter,
            4
        );

        float3 totalTransmittance = EvaluateTransmittance(
            viewOpticalDepth.x + sunOpticalDepth.x,
            viewOpticalDepth.y + sunOpticalDepth.y,
            viewOpticalDepth.z + sunOpticalDepth.z,
            betaRayleigh,
            betaMie,
            betaOzone
        );

        float3 scatteringCoeff = betaRayleigh * (sampleDensities.x * phaseR) + betaMie * (sampleDensities.y * phaseM);
        accumulatedInscatter += scatteringCoeff * totalTransmittance * (sunIntensity * stepSize);
    }

    inscattering = accumulatedInscatter;
    transmittance = EvaluateTransmittance(
        viewOpticalDepth.x,
        viewOpticalDepth.y,
        viewOpticalDepth.z,
        betaRayleigh,
        betaMie,
        betaOzone
    );
}

// Evaluates celestial sky dome radiance along an unbounded view direction
float3 EvaluateSkyRadiance(
    float3 cameraPos,
    float3 viewDir,
    float3 sunDir,
    int stepCount = 16
)
{
    float3 planetCenter = float3(0.0f, -g_atmosphereParams.x, 0.0f);
    float planetRadius = g_atmosphereParams.x;
    float atmRadius = g_atmosphereParams.y;

    float tMin, tMax;
    if (!RayAtmosphereBounds(cameraPos, viewDir, planetCenter, planetRadius, atmRadius, tMin, tMax))
    {
        return g_ambientColor.rgb * 0.25f;
    }

    float3 rayStart = cameraPos + viewDir * tMin;
    float rayLength = tMax - tMin;

    float3 inscattering, transmittance;
    EvaluateAtmosphericScattering(rayStart, viewDir, rayLength, sunDir, inscattering, transmittance, stepCount);

    // Celestial solar disc radiance attenuated by top-of-atmosphere optical transmittance
    float cosTheta = dot(viewDir, sunDir);
    float sunAngularRadius = 0.00465f; // ~0.27 degrees angular radius
    float cosSunRadius = cos(sunAngularRadius);

    if (cosTheta > cosSunRadius)
    {
        // Suppress solar disc if the view ray hits the planetary sphere in front of the sun
        float tGround0, tGround1;
        const bool hitsPlanet = RaySphereIntersect(cameraPos, viewDir, planetCenter, planetRadius, tGround0, tGround1) && (tGround0 > 0.0f);

        if (!hitsPlanet)
        {
            float sunDisc = smoothstep(cosSunRadius - 0.00005f, cosSunRadius, cosTheta);
            float3 sunDirectRadiance = float3(1.0f, 0.98f, 0.95f) * (g_atmosphereParams.z * 18.0f);
            inscattering += sunDirectRadiance * (transmittance * sunDisc);
        }
    }

    return inscattering;
}

// Couples an artist-configured boundary fog or haze layer with atmospheric illumination
void EvaluateCompositeFog(
    float3 rayDir,
    float cameraDist,
    float3 sunDir,
    float4 fogColor,
    float4 fogParams,
    inout float3 shadedColor
)
{
    if (fogColor.a <= 0.0f)
    {
        return;
    }

    const float fogExtent = max(cameraDist - fogParams.x, 0.0f);
    const float opticalDepth = fogExtent * fogParams.z;
    const float fogDensity = saturate((1.0f - exp(-opticalDepth * opticalDepth)) * fogColor.a);

    if (fogDensity <= 0.0001f)
    {
        return;
    }

    // Forward solar alignment for gentle golden haze warmth towards the celestial sun
    const float cosTheta = dot(rayDir, sunDir);
    const float forwardGlow = pow(saturate(cosTheta), 6.0f);
    const float3 solarWarmth = float3(1.05f, 0.98f, 0.85f);
    const float3 hazeInscatter = lerp(fogColor.rgb, fogColor.rgb * solarWarmth, forwardGlow * 0.5f);

    // Physical Beer-Lambert extinction and additive inscattering
    const float fogTransmittance = 1.0f - fogDensity;
    shadedColor = shadedColor * fogTransmittance + hazeInscatter * fogDensity;
}

#endif // ATMOSPHERE_HLSLI

