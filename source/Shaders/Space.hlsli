// Copyright © 2026 spacegirl65. All Rights Reserved.

#ifndef SPACE_HLSLI
#define SPACE_HLSLI

#include "SceneBuffers.hlsli"

TextureCube g_celestialCube : register(t32);
SamplerState g_celestialSampler : register(s1);

static const float SPACE_PI = 3.14159265358979323846f;

// Evaluates the full-sky stellar photometry and diffuse interstellar medium from the ESA Gaia DR3 survey cubemap
float3 EvaluateCelestialBackground(float3 viewDir)
{
    // Sample mip 0 directly with the trilinear clamp sampler (s1) to guarantee seamless cubemap face transitions
    const float3 surveySample = g_celestialCube.SampleLevel(g_celestialSampler, viewDir, 0.0f).rgb;
    const float lum = dot(surveySample, float3(0.2126f, 0.7152f, 0.0722f));

    // Suppress unresolved sub-visual (magnitude 17-21) background glow so intergalactic vacuum remains pure black
    const float cleanLum = saturate((lum - 0.008f) / 0.992f);
    if (cleanLum <= 0.0f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    // Evaluate 4-tap isotropic angular neighbourhood on mip 0 (~2.2 cubemap texels away) to measure local background
    // without touching lower mip levels (avoiding per-face mipmap boundary seams)
    const float3 upRef = (abs(viewDir.y) < 0.90f) ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    const float3 tangentX = normalize(cross(upRef, viewDir)) * 0.0018f;
    const float3 tangentY = cross(viewDir, tangentX);

    const float3 bg0 = g_celestialCube.SampleLevel(g_celestialSampler, viewDir + tangentX, 0.0f).rgb;
    const float3 bg1 = g_celestialCube.SampleLevel(g_celestialSampler, viewDir - tangentX, 0.0f).rgb;
    const float3 bg2 = g_celestialCube.SampleLevel(g_celestialSampler, viewDir + tangentY, 0.0f).rgb;
    const float3 bg3 = g_celestialCube.SampleLevel(g_celestialSampler, viewDir - tangentY, 0.0f).rgb;
    const float localBgLum = dot((bg0 + bg1 + bg2 + bg3) * 0.25f, float3(0.2126f, 0.7152f, 0.0722f));

    // Deep-space / Bortle 1 photometric calibration:
    // 1. Base diffuse galactic star-cloud curve and primary bright stellar curve
    // 2. Local point-source contrast restoring smaller stars diluted by bilinear filtering without brightening diffuse clouds
    const float diffuseTerm = pow(cleanLum, 1.70f) * 0.16f + pow(cleanLum, 3.80f) * 0.50f;
    const float starDelta = max(lum - localBgLum * 1.12f - 0.012f, 0.0f);
    const float pinpointTerm = pow(saturate(starDelta * 3.5f), 1.85f) * 0.55f;
    const float totalRadiance = diffuseTerm + pinpointTerm;

    // Preserve natural subtle spectral chromaticity without per-channel power-law colour shifts
    const float3 chroma = surveySample / max(lum, 0.0001f);
    const float3 naturalColor = lerp(float3(0.96f, 0.98f, 1.02f), chroma, 0.55f);

    return naturalColor * totalRadiance;
}

// Evaluates the real-world vacuum Sun across any heliocentric distance in Astronomical Units (AU)
void EvaluateVacuumSun(float3 viewDir, float3 sunDir, float solarDistanceAU, out float3 outSunRadiance, out float outDiscMask)
{
    const float distAU = max(solarDistanceAU, 0.001f);
    const float invSquareFlux = 1.0f / (distAU * distAU);

    // True solar photosphere angular radius: 0.00465047 radians (~0.2665 degrees, 31.98 arcmin diameter) at 1.0 AU
    const float sunAngularRadius = 0.00465047f / distAU;

    // High-precision angular separation via chord length, avoiding acos() float32 catastrophic cancellation near 0 degrees
    const float chord = length(viewDir - sunDir);
    const float theta = 2.0f * asin(saturate(0.5f * chord));

    // Screen-space derivative angular footprint for razor-sharp anti-aliased solar limb
    const float pixelAngularSize = max(length(fwidth(viewDir)), 0.00025f);
    const float effectiveRadius = max(sunAngularRadius, pixelAngularSize * 0.65f);

    // Sub-pixel coverage scaling when the solar disc shrinks below 1 pixel in the outer solar system (> 20 AU)
    const float subPixelCoverage = saturate((sunAngularRadius * sunAngularRadius) / (effectiveRadius * effectiveRadius));

    // Anti-aliased photosphere disc mask
    outDiscMask = 1.0f - smoothstep(effectiveRadius - pixelAngularSize * 0.5f, effectiveRadius + pixelAngularSize * 0.5f, theta);

    // Wavelength-dependent Eddington-Barbier solar limb darkening across the photosphere disc
    const float rNorm = saturate(theta / max(effectiveRadius, 1e-6f));
    const float mu = sqrt(saturate(1.0f - rNorm * rNorm));
    const float3 limbDarkening = pow(max(mu, 0.01f), float3(0.42f, 0.54f, 0.68f));

    // High-radiance G2V photosphere core (~5778 K, brilliant white in space with subtle warm limb)
    const float3 photosphereRadiance = float3(1.00f, 0.99f, 0.97f) * limbDarkening * (50.0f * subPixelCoverage);

    // Translucent multi-scale K/F-corona and optical small-angle scatter aureole outside the photosphere limb
    const float dTheta = max(theta - sunAngularRadius, 0.0f);
    const float innerCorona = 0.24f / pow(1.0f + dTheta / max(sunAngularRadius * 0.40f, 0.0003f), 2.3f);
    const float outerAureole = 0.022f / pow(1.0f + dTheta / 0.018f, 1.85f);
    const float3 coronaRadiance = float3(1.00f, 0.96f, 0.90f) * ((innerCorona + outerAureole) * invSquareFlux);

    outSunRadiance = photosphereRadiance * outDiscMask + coronaRadiance;
}

// Master evaluation function for the deep-space celestial cosmic sphere (4pi steradians)
float3 EvaluateCosmicRadiance(float3 viewDir, float3 sunDir, float solarDistanceAU)
{
    const float3 celestialBackground = EvaluateCelestialBackground(viewDir);

    float3 sunRadiance;
    float sunDiscMask;
    EvaluateVacuumSun(viewDir, sunDir, solarDistanceAU, sunRadiance, sunDiscMask);

    // Solid solar photosphere disc physically occludes background stars directly behind the Sun
    return celestialBackground * (1.0f - sunDiscMask) + sunRadiance;
}

#endif // SPACE_HLSLI
