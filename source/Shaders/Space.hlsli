// Copyright © 2026 spacegirl65. All Rights Reserved.

#ifndef SPACE_HLSLI
#define SPACE_HLSLI

#include "SceneBuffers.hlsli"

static const float SPACE_PI = 3.14159265358979323846f;

// Fast deterministic 3D-to-4D hash generating pseudo-random coordinates and parameters in [0, 1)
float4 SpaceHash34(float3 p)
{
    float3 q = frac(p * float3(443.897f, 441.423f, 437.195f));
    q += dot(q, q.yzx + 19.19f);
    float h1 = frac((q.x + q.y) * q.z);
    float h2 = frac((q.y + q.z) * q.x);
    float h3 = frac((q.z + q.x) * q.y);
    float h4 = frac((q.x * q.y) + q.z);
    return float4(h1, h2, h3, h4);
}

// Maps stellar parameters to authentic Harvard spectral classification (O, B, A, F, G, K, M)
// In real astronomy and human/camera perception, stars are predominantly silvery-white with subtle delicate tints
float3 EvaluateStellarSpectralColor(float temperatureParam)
{
    if (temperatureParam < 0.25f)
    {
        // M-class red dwarf (subtle warm reddish-amber)
        return float3(1.10f, 0.72f, 0.52f);
    }
    else if (temperatureParam < 0.50f)
    {
        // K-class orange star (pale amber cream)
        return float3(1.06f, 0.88f, 0.74f);
    }
    else if (temperatureParam < 0.78f)
    {
        // G-class solar star (Sol, subtle warm white)
        return float3(1.02f, 0.99f, 0.94f);
    }
    else if (temperatureParam < 0.92f)
    {
        // A/F-class white star (pure crisp silver-white)
        return float3(1.00f, 1.00f, 1.00f);
    }
    else
    {
        // O/B-class hot star (crisp diamond ice-blue)
        return float3(0.88f, 0.94f, 1.18f);
    }
}

// Evaluates a single cellular star layer across the celestial sphere as sharp sub-pixel pinpoints
float3 EvaluateStarLayer(
    float3 viewDir,
    float gridScale,
    float densityThreshold,
    float angularRadius,
    float brightnessScale
)
{
    const float3 p = viewDir * gridScale;
    const float3 cell = floor(p);

    const float4 h = SpaceHash34(cell);

    // Strict probability filter: only a tiny fraction of cells contain a visible star
    if (h.w < densityThreshold)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    // Position star strictly within the inner core [0.28, 0.72] of the cell to prevent boundary clipping
    const float3 starLocal = 0.28f + 0.44f * h.xyz;
    const float3 starWorld = cell + starLocal;
    const float3 starDir = normalize(starWorld);

    // Angular distance between sightline and star centroid
    const float cosAng = clamp(dot(viewDir, starDir), -1.0f, 1.0f);
    const float angDist = acos(cosAng);

    // Sharp cutoff at 2x angular radius (guarantees crisp 1-2 pixel pinpoint rendering)
    if (angDist > angularRadius * 2.0f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    // Steep Gaussian point-spread function: core rapidly falls off to 0 within sub-pixel radius
    const float normalizedDist = angDist / max(angularRadius, 0.00005f);
    const float pointSpread = exp(-1.8f * normalizedDist * normalizedDist);

    // Astronomical apparent magnitude distribution (exponential power law)
    const float magnitudeFactor = pow(saturate((h.w - densityThreshold) / max(1.0f - densityThreshold, 0.0001f)), 2.8f);
    const float3 spectralColor = EvaluateStellarSpectralColor(h.x);

    return spectralColor * (pointSpread * magnitudeFactor * brightnessScale);
}

// Evaluates the Milky Way galactic plane with authentic Sagittarius core bulge and the Great Rift dust lane
float3 EvaluateMilkyWay(float3 viewDir)
{
    // North Galactic Pole unit vector (J2000 equatorial orientation)
    const float3 galacticPole = normalize(float3(-0.867666f, 0.456012f, -0.198076f));

    // Galactic Center unit vector (towards Sagittarius A*, orthogonalised against the galactic pole)
    const float3 rawCore = float3(-0.054876f, -0.485126f, -0.872719f);
    const float3 galacticCore = normalize(rawCore - dot(rawCore, galacticPole) * galacticPole);

    // Galactic Y-axis (completing the orthonormal galactic coordinate basis towards l = +90 degrees in Cygnus)
    const float3 galacticY = normalize(cross(galacticPole, galacticCore));

    // Transform 3D celestial sightline into continuous galactic coordinates:
    // z_gal = sin(b), where b is galactic latitude (-pi/2 to +pi/2)
    // l = atan2(y_gal, x_gal) is galactic longitude (-pi to +pi)
    const float z_gal = clamp(dot(viewDir, galacticPole), -1.0f, 1.0f);
    const float x_gal = dot(viewDir, galacticCore);
    const float y_gal = dot(viewDir, galacticY);

    const float b = asin(z_gal);
    const float l = atan2(y_gal, x_gal);
    const float absB = abs(b);

    // Soft dual-component galactic disc profile (gentle core and broad diffuse halo)
    const float discCore = exp(-(absB * absB) / (2.0f * 0.075f * 0.075f));
    const float discHalo = exp(-(absB * absB) / (2.0f * 0.200f * 0.200f));
    const float discProfile = discCore * 0.65f + discHalo * 0.35f;

    if (discProfile < 0.0005f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    // Galactic longitude brightness modulation: brightest towards inner galaxy, fainter towards anticenter
    const float longModulation = 0.35f + 0.65f * (0.5f + 0.5f * cos(l));

    // Galactic Center bulge (Sagittarius A* at l = 0, b = 0)
    const float coreBulge = 1.8f * exp(-((l * l) / (2.0f * 0.35f * 0.35f)) - ((b * b) / (2.0f * 0.16f * 0.16f)));

    // Authentic Great Rift interstellar dust lane (weaving organically along the inner galaxy)
    const float riftCenterB = 0.012f * sin(l * 3.2f + 0.5f) + 0.006f * sin(l * 7.1f - 1.2f);
    const float riftDist = abs(b - riftCenterB);
    const float riftEnvelope = exp(-((l - 0.45f) * (l - 0.45f)) / (2.0f * 0.55f * 0.55f));
    const float riftProfile = exp(-(riftDist * riftDist) / (2.0f * 0.026f * 0.026f)) * riftEnvelope;

    // Secondary bifurcation branch (modeling the Aquila-Ophiuchus spur)
    const float branchCenterB = -0.016f * cos(l * 4.2f - 0.3f);
    const float branchDist = abs(b - branchCenterB);
    const float branchEnvelope = exp(-((l - 0.20f) * (l - 0.20f)) / (2.0f * 0.35f * 0.35f));
    const float branchProfile = exp(-(branchDist * branchDist) / (2.0f * 0.018f * 0.018f)) * branchEnvelope;

    // Molecular cloud dust extinction factor
    const float dustTransmission = saturate(1.0f - (riftProfile * 0.68f + branchProfile * 0.42f));

    // Subtle celestial starlight surface brightness calibrated for authentic astrophotography
    const float baseRadiance = (discProfile * longModulation + coreBulge) * dustTransmission * 0.032f;

    // Natural silvery-white to subtle warm star-cloud radiance
    const float3 milkyWayColor = float3(0.90f, 0.94f, 1.05f) * 0.55f + float3(1.05f, 0.96f, 0.85f) * 0.45f;

    return milkyWayColor * baseRadiance;
}

// Evaluates the vacuum celestial Sun with AU-scaled angular diameter, Eddington limb darkening, corona, and diffraction spikes
float3 EvaluateVacuumSun(float3 viewDir, float3 sunDir, float solarDistanceAU)
{
    // Astronomical solar angular radius: 0.00465 radians (~0.267 degrees) at 1.0 AU, scaling inversely with distance
    const float distAU = max(solarDistanceAU, 0.001f);
    const float sunAngularRadius = 0.00465f / distAU;
    const float cosSunRadius = cos(sunAngularRadius);

    const float cosTheta = dot(viewDir, sunDir);

    // Inverse-square solar flux attenuation (calibrated to 1.0 AU baseline)
    const float invSquareFlux = 1.0f / (distAU * distAU);
    const float baseSunIntensity = (g_atmosphereParams.z > 0.0f ? g_atmosphereParams.z : 22.0f) * 18.0f * invSquareFlux;
    const float3 solarPhotosphereColor = float3(1.0f, 0.985f, 0.955f) * baseSunIntensity;

    float3 solarRadiance = float3(0.0f, 0.0f, 0.0f);

    // Primary Solar Photosphere Disc with Eddington Limb Darkening
    if (cosTheta >= cosSunRadius)
    {
        const float theta = acos(clamp(cosTheta, -1.0f, 1.0f));
        const float mu = sqrt(saturate(1.0f - (theta / max(sunAngularRadius, 1e-6f)) * (theta / max(sunAngularRadius, 1e-6f))));

        // Eddington-Barbier limb darkening law: I(mu) = I0 * (0.4 + 0.6 * mu)
        const float limbDarkening = 0.40f + 0.60f * mu;

        // Anti-aliased outer rim transition
        const float edgeSmoothing = smoothstep(cosSunRadius - 0.00002f, cosSunRadius, cosTheta);
        solarRadiance += solarPhotosphereColor * (limbDarkening * edgeSmoothing);
    }

    // Vacuum Solar Corona (tenuous high-temperature plasma envelope)
    const float angleFromSun = acos(clamp(cosTheta, -1.0f, 1.0f));
    const float coronaExtent = max(angleFromSun - sunAngularRadius, 0.0f);
    const float coronaProfile = exp(-coronaExtent / max(sunAngularRadius * 1.6f, 0.002f)) * 0.045f;
    solarRadiance += solarPhotosphereColor * (coronaProfile * 0.35f);

    // High-contrast optical sensor diffraction spikes (4-point spider cross)
    if (angleFromSun < 0.35f)
    {
        const float3 sunUp = (abs(sunDir.y) < 0.95f) ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
        const float3 sunRight = normalize(cross(sunDir, sunUp));
        const float3 sunOrthogonalUp = cross(sunRight, sunDir);

        const float spikeProjX = dot(viewDir, sunRight);
        const float spikeProjY = dot(viewDir, sunOrthogonalUp);

        // Thin horizontal and vertical diffraction rays
        const float spikeThickness = 0.0008f;
        const float spikeLength = 0.22f;
        const float spikeH = exp(-abs(spikeProjY) / spikeThickness) * exp(-abs(spikeProjX) / spikeLength);
        const float spikeV = exp(-abs(spikeProjX) / spikeThickness) * exp(-abs(spikeProjY) / spikeLength);

        const float diffractionSpikes = (spikeH + spikeV) * (0.012f * invSquareFlux);
        solarRadiance += float3(1.0f, 0.99f, 0.97f) * (diffractionSpikes * baseSunIntensity);
    }

    return solarRadiance;
}

// Master evaluation function for the deep-space celestial cosmic sphere (4pi steradians)
float3 EvaluateCosmicRadiance(float3 viewDir, float3 sunDir, float solarDistanceAU)
{
    float3 radiance = float3(0.0f, 0.0f, 0.0f);

    // 1. Milky Way galactic plane, core bulge, and interstellar dust rifts
    radiance += EvaluateMilkyWay(viewDir);

    // 2. Multi-octave procedural starfield (calibrated for authentic pinpoint astrophotography)
    // Octave A: Faint background pinpricks (~130 in standard FOV, razor-sharp sub-pixel points)
    radiance += EvaluateStarLayer(viewDir, 140.0f, 0.970f, 0.00030f, 0.65f);

    // Octave B: Prominent visible navigation stars (~45 in standard FOV, crisp 1-pixel points)
    radiance += EvaluateStarLayer(viewDir, 60.0f, 0.975f, 0.00045f, 2.2f);

    // Octave C: Sparse brilliant anchor stars (~15 in standard FOV, bright points with subtle Airy glint)
    radiance += EvaluateStarLayer(viewDir, 28.0f, 0.985f, 0.00065f, 5.5f);

    // 3. The Vacuum Sun (AU-scaled disc, limb darkening, corona, and diffraction spikes)
    radiance += EvaluateVacuumSun(viewDir, sunDir, solarDistanceAU);

    return radiance;
}

#endif // SPACE_HLSLI
