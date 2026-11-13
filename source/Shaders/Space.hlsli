// Copyright © 2026 spacegirl65. All Rights Reserved.

#ifndef SPACE_HLSLI
#define SPACE_HLSLI

#include "SceneBuffers.hlsli"

TextureCube g_celestialCube : register(t32);
SamplerState g_celestialSampler : register(s1);

static const float SPACE_PI = 3.14159265358979323846f;

// Smooth 3D celestial field modulating stellar apparent magnitudes across a natural Pogson power-law distribution
float EvaluateCelestialMagnitudeWeight(float3 viewDir)
{
    const float3 p = viewDir * 320.0f;
    const float3 i = floor(p);
    const float3 f = frac(p);
    const float3 u = f * f * (3.0f - 2.0f * f);

    const float n000 = frac(sin(dot(i + float3(0.0f, 0.0f, 0.0f), float3(127.1f, 311.7f, 74.7f))) * 43758.5453f);
    const float n100 = frac(sin(dot(i + float3(1.0f, 0.0f, 0.0f), float3(127.1f, 311.7f, 74.7f))) * 43758.5453f);
    const float n010 = frac(sin(dot(i + float3(0.0f, 1.0f, 0.0f), float3(127.1f, 311.7f, 74.7f))) * 43758.5453f);
    const float n110 = frac(sin(dot(i + float3(1.0f, 1.0f, 0.0f), float3(127.1f, 311.7f, 74.7f))) * 43758.5453f);
    const float n001 = frac(sin(dot(i + float3(0.0f, 0.0f, 1.0f), float3(127.1f, 311.7f, 74.7f))) * 43758.5453f);
    const float n101 = frac(sin(dot(i + float3(1.0f, 0.0f, 1.0f), float3(127.1f, 311.7f, 74.7f))) * 43758.5453f);
    const float n011 = frac(sin(dot(i + float3(0.0f, 1.0f, 1.0f), float3(127.1f, 311.7f, 74.7f))) * 43758.5453f);
    const float n111 = frac(sin(dot(i + float3(1.0f, 1.0f, 1.0f), float3(127.1f, 311.7f, 74.7f))) * 43758.5453f);

    const float nx00 = lerp(n000, n100, u.x);
    const float nx10 = lerp(n010, n110, u.x);
    const float nx01 = lerp(n001, n101, u.x);
    const float nx11 = lerp(n011, n111, u.x);
    const float nxy0 = lerp(nx00, nx10, u.y);
    const float nxy1 = lerp(nx01, nx11, u.y);

    // Remap smooth trilinear variance back to uniform [0, 1] before applying Pogson magnitude power law
    return saturate((lerp(nxy0, nxy1, u.z) - 0.18f) / 0.64f);
}

// Evaluates the full-sky stellar photometry and diffuse interstellar medium from the ESA Gaia DR3 survey cubemap
float3 EvaluateCelestialBackground(float3 viewDir)
{
    const float3 lumaWeights = float3(0.2126f, 0.7152f, 0.0722f);
    const float faintNoiseFloor = 0.00001f;
    const float diffuseBgFloor = 0.005f;
    const float primaryStarFloor = 0.025f;
    const float primaryBgScale = 1.05f;
    const float magWeightFloor = 0.035f;
    const float beaconWeightFloor = 0.35f;
    const float faintBgScale = 1.02f;
    const float faintProminenceCeiling = 0.018f;
    const float faintRadianceFloor = 0.0060f;
    const float faintRadianceCeiling = 0.0140f;
    const float bandDimScale = 0.5184f;
    const float outsideBandBoost = 1.188f;
    const float chromaLumaFloor = 0.0001f;

    // Sample mip 0 directly with the trilinear clamp sampler (s1) to guarantee seamless cubemap face transitions
    const float3 surveySample = g_celestialCube.SampleLevel(g_celestialSampler, viewDir, 0.0f).rgb;
    const float lum = dot(surveySample, lumaWeights);

    if (lum <= faintNoiseFloor)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    // Construct tangent basis to measure both the immediate 1-texel stellar PSF wing (ring 1)
    // and the surrounding 3.5-texel local galactic background (ring 2) strictly on mip 0
    const float3 upRef = (abs(viewDir.y) < 0.90f) ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    const float3 basisX = normalize(cross(upRef, viewDir));
    const float3 basisY = cross(viewDir, basisX);

    // Ring 1 (~1.1 cubemap texels): captures the wider PSF halo present only on genuine bright catalog stars
    const float ring1Step = 0.00085f;
    const float3 stepInnerX = basisX * ring1Step;
    const float3 stepInnerY = basisY * ring1Step;
    const float3 inner0 = g_celestialCube.SampleLevel(g_celestialSampler, viewDir + stepInnerX, 0.0f).rgb;
    const float3 inner1 = g_celestialCube.SampleLevel(g_celestialSampler, viewDir - stepInnerX, 0.0f).rgb;
    const float3 inner2 = g_celestialCube.SampleLevel(g_celestialSampler, viewDir + stepInnerY, 0.0f).rgb;
    const float3 inner3 = g_celestialCube.SampleLevel(g_celestialSampler, viewDir - stepInnerY, 0.0f).rgb;
    const float ring1Lum = dot((inner0 + inner1 + inner2 + inner3) * 0.25f, lumaWeights);

    // Ring 2 (~3.5 cubemap texels, rotated 45 degrees): measures true local sky/galactic background outside the star
    const float ring2Step = 0.0019f;
    const float3 stepOuterA = (basisX + basisY) * ring2Step;
    const float3 stepOuterB = (basisX - basisY) * ring2Step;
    const float3 outer0 = g_celestialCube.SampleLevel(g_celestialSampler, viewDir + stepOuterA, 0.0f).rgb;
    const float3 outer1 = g_celestialCube.SampleLevel(g_celestialSampler, viewDir - stepOuterA, 0.0f).rgb;
    const float3 outer2 = g_celestialCube.SampleLevel(g_celestialSampler, viewDir + stepOuterB, 0.0f).rgb;
    const float3 outer3 = g_celestialCube.SampleLevel(g_celestialSampler, viewDir - stepOuterB, 0.0f).rgb;
    const float ring2Lum = dot((outer0 + outer1 + outer2 + outer3) * 0.25f, lumaWeights);

    // 1. Diffuse galactic star clouds evaluated from the outer background ring so point stars are not double-counted
    const float cleanBg = saturate((ring2Lum - diffuseBgFloor) / (1.0f - diffuseBgFloor));
    const float diffuseTerm = pow(cleanBg, 1.90f) * 0.095f + pow(cleanBg, 3.80f) * 0.22f;

    // 2. Restore natural apparent-magnitude variation across the Gaia survey's logarithmically clipped star peaks:
    //    - 85% of visible stars are delicate 5th- to 6th-magnitude pinpoints (weight 0.035..0.12)
    //    - 12% are medium 3rd- to 4th-magnitude constellation stars (weight 0.12..0.45)
    //    - Only stars with both a strong central peak and a multi-texel PSF wing (ring1 > ring2) shine as bright beacons
    const float magParam = EvaluateCelestialMagnitudeWeight(viewDir);
    const float magWeight = lerp(magWeightFloor, 1.0f, pow(magParam, 6.5f));

    const float primaryBgFloor = ring2Lum * primaryBgScale + primaryStarFloor;
    const float starProminence = max(lum - primaryBgFloor, 0.0f);
    const float haloProminence = max(ring1Lum - primaryBgFloor, 0.0f);

    // Exaggerated sub-threshold toe so the faintest survey stars are clearly visible without brightening medium/bright stars
    const float faintProminence = max(lum - ring2Lum * faintBgScale - faintNoiseFloor, 0.0f);
    const float faintStars = pow(saturate(faintProminence / faintProminenceCeiling), 0.45f) * lerp(faintRadianceFloor, faintRadianceCeiling, magParam);

    const float variedFieldStars = pow(starProminence * 2.10f, 2.10f) * (0.42f * magWeight);
    const float navigationBeacons = pow(starProminence * haloProminence * 14.0f, 2.20f) * lerp(beaconWeightFloor, 1.0f, pow(magParam, 3.0f));
    const float fieldStars = max(faintStars, variedFieldStars);

    // Dim the galactic band while boosting stars outside the band
    const float bandMask = smoothstep(0.01f, 0.14f, cleanBg);
    const float regionScale = lerp(outsideBandBoost, bandDimScale, bandMask);
    const float totalRadiance = diffuseTerm * bandDimScale + (fieldStars + navigationBeacons) * regionScale;

    // Preserve natural subtle spectral chromaticity without per-channel power-law colour shifts
    const float3 chroma = surveySample / max(lum, chromaLumaFloor);
    const float3 naturalColor = lerp(float3(0.96f, 0.98f, 1.02f), chroma, 0.55f);

    return naturalColor * totalRadiance;
}

// Evaluates the real-world vacuum Sun across any heliocentric distance in Astronomical Units (AU)
void EvaluateVacuumSun(float3 viewDir, float3 sunDir, float solarDistanceAU, out float3 outSunRadiance, out float outDiscMask)
{
    const float minSolarDistanceAU = 0.001f;
    const float solarAngularRadius1AU = 0.00465047f;
    const float pixelAngularFloor = 0.00025f;
    const float effectiveRadiusFloor = 1e-6f;
    const float limbMuFloor = 0.01f;
    const float innerCoronaScaleFloor = 0.0003f;
    const float photospherePeakRadiance = 50.0f;

    const float distAU = max(solarDistanceAU, minSolarDistanceAU);
    const float invSquareFlux = 1.0f / (distAU * distAU);

    // True solar photosphere angular radius: 0.00465047 radians (~0.2665 degrees, 31.98 arcmin diameter) at 1.0 AU
    const float sunAngularRadius = solarAngularRadius1AU / distAU;

    // High-precision angular separation via chord length, avoiding acos() float32 catastrophic cancellation near 0 degrees
    const float chord = length(viewDir - sunDir);
    const float theta = 2.0f * asin(saturate(0.5f * chord));

    // Screen-space derivative angular footprint for razor-sharp anti-aliased solar limb
    const float pixelAngularSize = max(length(fwidth(viewDir)), pixelAngularFloor);
    const float effectiveRadius = max(sunAngularRadius, pixelAngularSize * 0.65f);

    // Sub-pixel coverage scaling when the solar disc shrinks below 1 pixel in the outer solar system (> 20 AU)
    const float subPixelCoverage = saturate((sunAngularRadius * sunAngularRadius) / (effectiveRadius * effectiveRadius));

    // Anti-aliased photosphere disc mask
    outDiscMask = 1.0f - smoothstep(effectiveRadius - pixelAngularSize * 0.5f, effectiveRadius + pixelAngularSize * 0.5f, theta);

    // Wavelength-dependent Eddington-Barbier solar limb darkening across the photosphere disc
    const float rNorm = saturate(theta / max(effectiveRadius, effectiveRadiusFloor));
    const float mu = sqrt(saturate(1.0f - rNorm * rNorm));
    const float3 limbDarkening = pow(max(mu, limbMuFloor), float3(0.42f, 0.54f, 0.68f));

    // High-radiance G2V photosphere core (~5778 K, brilliant white in space with subtle warm limb)
    const float3 photosphereRadiance = float3(1.00f, 0.99f, 0.97f) * limbDarkening * (photospherePeakRadiance * subPixelCoverage);

    // Translucent multi-scale K/F-corona and optical small-angle scatter aureole outside the photosphere limb
    const float dTheta = max(theta - sunAngularRadius, 0.0f);
    const float innerCorona = 0.24f / pow(1.0f + dTheta / max(sunAngularRadius * 0.40f, innerCoronaScaleFloor), 2.3f);
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
