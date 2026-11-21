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
    const float primaryBgScale = 1.050f;
    const float magWeightFloor = 0.035f;
    const float beaconWeightFloor = 0.350f;
    const float faintBgScale = 1.020f;
    const float faintProminenceCeiling = 0.018f;
    const float faintRadianceFloor = 0.006f;
    const float faintRadianceCeiling = 0.014f;
    const float bandDimScale = 0.467f;
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

// High-precision angular separation in radians via chord length, avoiding acos() cancellation near 0 degrees
float EvaluateAngularSeparation(float3 dirA, float3 dirB)
{
    return 2.0f * asin(saturate(0.5f * length(dirA - dirB)));
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
    const float photospherePeakRadiance = 24.0f;

    const float distAU = max(solarDistanceAU, minSolarDistanceAU);
    const float invSquareFlux = 1.0f / (distAU * distAU);

    // True solar photosphere angular radius: 0.00465047 radians (~0.2665 degrees, 31.98 arcmin diameter) at 1.0 AU
    const float sunAngularRadius = solarAngularRadius1AU / distAU;

    // High-precision angular separation between view ray and solar centre
    const float theta = EvaluateAngularSeparation(viewDir, sunDir);

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

    // TODO: Dynamically compute solar photosphere and coronal colour from camera position and atmospheric optical transmittance.
    // Currently assumes exo-atmospheric Air Mass Zero (AM0) vacuum: 5778 K G2V sunlight relative to a 6500 K D65 display white point.
    const float3 am0PhotosphereTint = float3(1.00f, 0.985f, 0.95f);
    const float3 am0CoronaTint      = float3(1.00f, 0.95f, 0.88f);
    const float3 photosphereRadiance = am0PhotosphereTint * limbDarkening * (photospherePeakRadiance * subPixelCoverage);

    // Translucent multi-scale K/F-corona and optical small-angle scatter aureole outside the photosphere limb
    const float dTheta = max(theta - sunAngularRadius, 0.0f);
    const float innerCorona = 0.24f / pow(1.0f + dTheta / max(sunAngularRadius * 0.40f, innerCoronaScaleFloor), 2.3f);
    const float outerAureole = 0.022f / pow(1.0f + dTheta / 0.018f, 1.85f);
    const float3 coronaRadiance = am0CoronaTint * ((innerCorona + outerAureole) * invSquareFlux);

    outSunRadiance = photosphereRadiance * outDiscMask + coronaRadiance;
}

// Evaluates a single multi-coated hexagonal aperture ghost reflection in aspect-corrected NDC space
float3 EvaluateApertureGhost(float2 pixCoord, float2 ghostCoord, float radius, float3 tint)
{
    const float radiusFloor = 0.0001f;
    const float2 delta = pixCoord - ghostCoord;
    const float angle = atan2(delta.y, delta.x);
    const float hexMod = 1.0f + 0.045f * cos(6.0f * angle);
    const float dist = length(delta) / (max(radius, radiusFloor) * hexMod);

    if (dist >= 1.0f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const float edge = smoothstep(1.0f, 0.72f, dist);
    const float causticRim = 0.35f + 0.65f * smoothstep(0.45f, 0.92f, dist);
    return tint * (edge * causticRim);
}

// Evaluates a physically grounded hard-SF optical lens flare (veiling glare, 6-blade diffraction, and AR-coated ghosts)
float3 EvaluateSolarLensFlare(float3 viewDir, float3 sunDir, float solarDistanceAU)
{
    const float minSolarDistanceAU = 0.001f;
    const float glarePeakClamp = 12.0f;
    const float projFloor = 0.0001f;

    const float distAU = max(solarDistanceAU, minSolarDistanceAU);
    const float flareFlux = min(1.0f / (distAU * distAU), glarePeakClamp);

    // High-precision angular separation from the solar centre
    const float theta = EvaluateAngularSeparation(viewDir, sunDir);

    // Roll-stabilised camera-aligned angle around the Sun for aperture diffraction blades
    const float3 camRight   = g_world[0].xyz;
    const float3 camUp      = g_world[1].xyz;
    const float3 camForward = g_world[2].xyz;
    const float3 delta      = viewDir - sunDir;
    const float  phi        = atan2(dot(delta, camUp), dot(delta, camRight));

    // Camera-to-Sun optical axis alignment: drives broad internal lens glass scatter only when the Sun enters the forward FOV
    const float3 pixCam = float3(dot(viewDir, camRight), dot(viewDir, camUp), dot(viewDir, camForward));
    const float3 sunCam = float3(dot(sunDir, camRight), dot(sunDir, camUp), dot(sunDir, camForward));
    const float cameraSunAlignment    = smoothstep(0.45f, 0.96f, sunCam.z);
    const float eyeAngleToSun         = EvaluateAngularSeparation(camForward, sunDir);
    const float withinGlareThreshold  = 1.0f - smoothstep(0.65f, 1.00f, eyeAngleToSun);
    const float offAxisReachScale     = lerp(0.50f, 1.0f, withinGlareThreshold);

    // TODO: Dynamically tint solar flare scatter and diffraction spectrum from camera altitude and atmospheric transmittance.
    // Currently assumes exo-atmospheric AM0 vacuum: compact white-hot 0.53 deg core paired with post-process HDR bloom.
    const float3 innerBloom     = float3(1.00f, 0.96f, 0.89f) * (1.95f * exp(-theta / 0.0056f));
    const float3 opticalScatter = float3(1.00f, 0.84f, 0.60f) * (0.28f * exp(-theta / 0.026f));
    const float3 wideVeil       = float3(1.00f, 0.90f, 0.78f) * (0.042f / pow(1.0f + theta / 0.060f, 2.2f));
    const float3 broadLensVeil  = float3(0.84f, 0.90f, 1.00f) * (0.036f / pow(1.0f + theta / 0.26f, 1.65f)) * cameraSunAlignment;
    const float3 glareColor     = innerBloom + opticalScatter + wideVeil + broadLensVeil;

    // 2-fold angular envelope compressing opposite right (~15 deg) and left (~-165 deg) sectors
    // while scaling spoke reach down smoothly when the eye looks outside the glare threshold
    const float redSectorMask    = smoothstep(0.50f, 0.95f, cos(2.0f * (phi - 0.26f)));
    const float sectorReachScale = lerp(1.0f, 0.28f, redSectorMask) * offAxisReachScale;

    // Per-angle radial length modulation so rays terminate organically at varied distances instead of a uniform circle
    const float lengthModA = 0.52f + 0.48f * sin(phi * 3.0f + 0.9f) * cos(phi * 5.0f - 0.4f);
    const float lengthModB = 0.45f + 0.55f * cos(phi * 7.0f - 1.3f) * sin(phi * 4.0f + 0.6f);
    const float rayReachA  = lerp(0.022f, 0.068f, saturate(lengthModA)) * sectorReachScale;
    const float rayReachB  = lerp(0.016f, 0.052f, saturate(lengthModB)) * sectorReachScale;
    const float rayReachFine = 0.024f * sectorReachScale;

    // Irregularly spaced, multi-width organic rays saturating into the core rim to break up any circular edge
    const float raysBroad = pow(0.5f + 0.5f * cos(phi * 11.0f + 0.6f) * cos(phi * 19.0f - 0.9f), 7.5f);
    const float raysMed   = pow(0.5f + 0.5f * sin(phi * 23.0f - 0.5f) * cos(phi * 37.0f + 1.1f), 10.5f);
    const float raysFine  = pow(0.5f + 0.5f * cos(phi * 47.0f + 0.3f) * sin(phi * 61.0f - 0.8f), 14.0f);

    const float3 wavelengthRatio = float3(1.18f, 1.00f, 0.84f); // R/G/B relative to 540 nm
    const float  fringeBlend     = smoothstep(0.012f, 0.055f, theta);
    const float3 fraunhoferWave  = 0.76f + 0.24f * cos((theta / wavelengthRatio) * 145.0f);
    const float3 chromaticFringe = lerp(float3(1.0f, 1.0f, 1.0f), fraunhoferWave, fringeBlend);

    const float broadWeight = raysBroad * 0.70f;
    const float medWeight   = raysMed   * 0.85f;
    const float fineWeight  = raysFine  * 0.65f;

    const float baseRayEnergy =
          broadWeight * exp(-theta / rayReachA)    / (1.0f + theta * 26.0f)
        + medWeight   * exp(-theta / rayReachB)    / (1.0f + theta * 30.0f)
        + fineWeight  * exp(-theta / rayReachFine) / (1.0f + theta * 38.0f);
    const float extendedRayTail =
          broadWeight * exp(-theta / (rayReachA * 2.25f))             / (1.0f + theta * 20.0f)
        + medWeight   * exp(-theta / (rayReachB * 2.20f))             / (1.0f + theta * 22.0f)
        + fineWeight  * exp(-theta / (0.055f * sectorReachScale))     / (1.0f + theta * 28.0f);
    const float organicRayEnergy = max(baseRayEnergy, extendedRayTail * 0.16f);
    const float3 ciliaryRays = float3(1.00f, 0.91f, 0.78f) * organicRayEnergy;

    // Asymmetric 6-blade diffraction streaks with per-ray length variation (one subtle longer diagonal streak like real optics)
    const float irisPhi = phi + 0.28f;
    const float spikeLengthScale = (0.60f + 0.75f * pow(0.5f + 0.5f * cos(irisPhi * 2.0f - 0.35f), 2.5f)
                                  + 0.30f * sin(irisPhi * 5.0f + 1.1f)) * sectorReachScale;
    const float spikeAsym = 0.55f + 0.45f * sin(irisPhi * 5.0f + 1.4f);
    const float spikeCore = pow(abs(cos(3.0f * irisPhi)), 170.0f + theta * 4200.0f);
    const float spikeWing = pow(abs(cos(3.0f * irisPhi)), 26.0f + theta * 950.0f) * 0.20f;

    const float3 baseSpikeDecay     = exp(-theta / (0.068f * max(spikeLengthScale, 0.12f) * wavelengthRatio));
    const float3 extendedSpikeDecay = exp(-theta / (0.155f * max(spikeLengthScale, 0.12f) * wavelengthRatio)) * 0.16f;
    const float3 spikeDecay         = max(baseSpikeDecay, extendedSpikeDecay);
    const float3 spikeSpectrum = float3(1.09f, 0.97f, 0.82f) * spikeDecay * chromaticFringe
                               + float3(0.55f, 0.86f, 1.22f) * (exp(-abs(theta - 0.045f) / 0.025f) * 0.30f);
    const float starburstScale = lerp(0.45f, 0.70f, withinGlareThreshold);
    const float3 starburst = (spikeSpectrum * ((spikeCore + spikeWing) * spikeAsym / (1.0f + theta * 26.0f)) + ciliaryRays) * starburstScale;

    // Multi-coated internal lens reflection ghosts along the optical axis
    float3 ghosts = float3(0.0f, 0.0f, 0.0f);

    if (sunCam.z > 0.0f && pixCam.z > 0.0f)
    {
        const float aspect = g_mvp._m11 / max(g_mvp._m00, projFloor);
        const float2 sunNdc = float2((sunCam.x / sunCam.z) * g_mvp._m00, (sunCam.y / sunCam.z) * g_mvp._m11);
        const float2 pixNdc = float2((pixCam.x / pixCam.z) * g_mvp._m00, (pixCam.y / pixCam.z) * g_mvp._m11);
        const float2 sunCoord = float2(sunNdc.x * aspect, sunNdc.y);
        const float2 pixCoord = float2(pixNdc.x * aspect, pixNdc.y);

        const float sunScreenFade = saturate(1.0f - max(abs(sunNdc.x), abs(sunNdc.y)) / 1.35f) * smoothstep(0.0f, 0.18f, sunCam.z);
        if (sunScreenFade > 0.0f)
        {
            ghosts += EvaluateApertureGhost(pixCoord, sunCoord * 0.68f, 0.022f, float3(0.020f, 0.052f, 0.044f));
            ghosts += EvaluateApertureGhost(pixCoord, sunCoord * 0.42f, 0.055f, float3(0.028f, 0.044f, 0.054f));
            ghosts += EvaluateApertureGhost(pixCoord, sunCoord * -0.35f, 0.095f, float3(0.036f, 0.026f, 0.014f));
            ghosts += EvaluateApertureGhost(pixCoord, sunCoord * -0.78f, 0.145f, float3(0.014f, 0.030f, 0.024f));
            ghosts += EvaluateApertureGhost(pixCoord, sunCoord * -1.18f, 0.042f, float3(0.040f, 0.022f, 0.046f));
            ghosts += EvaluateApertureGhost(pixCoord, sunCoord * -1.45f, 0.210f, float3(0.010f, 0.014f, 0.022f));
            ghosts *= sunScreenFade;
        }
    }

    return (glareColor + starburst + ghosts) * flareFlux;
}

// Master evaluation function for the deep-space celestial cosmic sphere (4pi steradians)
float3 EvaluateCosmicRadiance(float3 viewDir, float3 sunDir, float solarDistanceAU)
{
    const float3 celestialBackground = EvaluateCelestialBackground(viewDir);

    float3 sunRadiance;
    float sunDiscMask;
    EvaluateVacuumSun(viewDir, sunDir, solarDistanceAU, sunRadiance, sunDiscMask);

    const float3 solarLensFlare = EvaluateSolarLensFlare(viewDir, sunDir, solarDistanceAU);

    // Attenuate faint background stars inside the immediate solar glare cone where optical scatter swamps starlight
    const float theta = EvaluateAngularSeparation(viewDir, sunDir);
    const float invSquareFlux = saturate(1.0f / max(solarDistanceAU * solarDistanceAU, 0.0001f));
    const float glareStarVisibility = saturate(1.0f - 0.88f * exp(-theta / 0.10f) * invSquareFlux);

    // Solid solar photosphere disc physically occludes background stars directly behind the Sun
    return celestialBackground * (1.0f - sunDiscMask) * glareStarVisibility + sunRadiance + solarLensFlare;
}

#endif // SPACE_HLSLI
