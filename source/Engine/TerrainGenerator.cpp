// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "TerrainGenerator.h"

#include <algorithm>
#include <cmath>

namespace Sandbox3D::Engine
{
    TerrainGenerator::TerrainGenerator(const TerrainConfig& config)
        : m_config(config)
        , m_noise(config.seed)
    {
    }

    void TerrainGenerator::SetConfig(const TerrainConfig& config)
    {
        m_config = config;
        m_noise.SetSeed(config.seed);
    }

    double TerrainGenerator::CalculateValleyCenterline(double u) const noexcept
    {
        // Gentle, naturalistic meandering curve across the dale
        // Broad, gentle sweeping primary arc (wavelength ~480m)
        constexpr double primaryWavelength = 480.0;
        const double phase1 = u * (Maths::TwoPi<double> / primaryWavelength);
        const double primaryBend = std::sin(phase1);

        // Soft secondary harmonic for natural asymmetry
        constexpr double secondaryWavelength = 240.0;
        const double phase2 = u * (Maths::TwoPi<double> / secondaryWavelength) + 0.65;
        const double secondaryBend = std::sin(phase2) * 0.22;

        // Subtle organic wandering from Perlin noise
        const double organicWarp = m_noise.Perlin(u * 0.009, 13.37) * 0.20 +
                                   m_noise.Perlin(u * 0.020, 47.19) * 0.08;

        const double normalizedMeander = primaryBend * 0.75 + secondaryBend + organicWarp;
        return normalizedMeander * m_config.valleyMeander;
    }

    double TerrainGenerator::CalculateRiverOffset(double u) const noexcept
    {
        // High-frequency natural wandering of the river channel within the alluvial valley floor
        return (m_noise.Perlin(u * 0.018, 31.41) * 0.70 +
                m_noise.Perlin(u * 0.045, 78.19) * 0.30) * (m_config.riverWidth * 0.50);
    }

    double TerrainGenerator::GenerateHeight(double x, double z) const noexcept
    {
        // Primary dale valley trough orientation and coordinate transform
        // Valley axis runs along an east-west orientation with natural meandering warp
        constexpr double valleyAngle = 0.22; // ~12 degrees inclination
        const double cosV = std::cos(valleyAngle);
        const double sinV = std::sin(valleyAngle);
        const double u = x * cosV + z * sinV;  // Along-valley coordinate
        const double v = -x * sinV + z * cosV; // Across-valley coordinate

        // Multi-harmonic curved wandering of the valley centerline
        const double meander = CalculateValleyCenterline(u);
        const double distFromCenter = std::abs(v - meander);

        // Glaciated U/V dale trough profile
        const double halfWidth = m_config.width * 0.44;
        const double halfFloor = m_config.valleyFloorWidth * 0.5;

        double troughFactor = 0.0;
        if (distFromCenter > halfFloor)
        {
            const double flankDist = (distFromCenter - halfFloor) / (halfWidth - halfFloor);
            troughFactor = std::pow(std::clamp(flankDist, 0.0, 1.0), m_config.valleyFlankSteepness);
        }

        // Macro fell topography using multi-octave Fractal Brownian Motion
        const double nx = x * m_config.baseFrequency;
        const double nz = z * m_config.baseFrequency;
        const double rawMacro = m_noise.FBM(nx, nz, m_config.octaves, m_config.persistence, m_config.lacunarity);
        const double normMacro = std::clamp(rawMacro * 0.5 + 0.5, 0.0, 1.0);

        // Asymmetry between northern fell massif (Baugh Fell) and southern ridge (Rise Hill)
        const double fellAsymmetry = (v > meander) ? 0.06 : -0.03;

        // Base landscape elevation combining glaciated dale trough with macro fell relief
        double elevation = (troughFactor * m_config.valleyDepth) +
                           (std::pow(normMacro, m_config.convexity) * (1.0 - m_config.valleyDepth * 0.45)) +
                           (fellAsymmetry * troughFactor);

        // Stepped cyclothem stratification (Carboniferous Yoredale limestone benches and scars)
        const double worldH = elevation * m_config.heightScale;
        const double flankMask = std::clamp((distFromCenter - halfFloor * 0.6) / 28.0, 0.0, 1.0);

        if (m_config.terracingStrength > 0.0 && m_config.cyclothemStepHeight > 0.0)
        {
            const double step = m_config.cyclothemStepHeight;
            const double layerPhase = worldH / step;
            const double layerIdx = std::floor(layerPhase);
            const double layerFrac = layerPhase - layerIdx; // [0, 1)

            // Non-linear bench profile: broad flat structural shelf transitioning to a steep vertical scar riser
            const double scarThreshold = 1.0 - m_config.cyclothemScarSharpness * 0.45;
            double steppedFrac = 0.0;
            if (layerFrac < scarThreshold)
            {
                // Gentle structural terrace shelf
                steppedFrac = (layerFrac / scarThreshold) * 0.22;
            }
            else
            {
                // Steep rocky scar riser (using smoothstep)
                const double t = (layerFrac - scarThreshold) / (1.0 - scarThreshold);
                const double s = t * t * (3.0 - 2.0 * t);
                steppedFrac = 0.22 + s * 0.78;
            }

            const double steppedWorldH = (layerIdx + steppedFrac) * step;
            const double steppedNormElev = steppedWorldH / m_config.heightScale;

            // Apply stepped terracing primarily to the fell flanks above the alluvial valley bottom
            elevation = Maths::Lerp(elevation, steppedNormElev, m_config.terracingStrength * flankMask);

            // Secondary thin-bedded limestone stratum granularity
            if (m_config.subTerracingStrength > 0.0 && m_config.subTerracingStepHeight > 0.0)
            {
                const double subStep = m_config.subTerracingStepHeight;
                const double subPhase = worldH / subStep;
                const double subFrac = subPhase - std::floor(subPhase);
                const double subThreshold = 0.70;
                double subFracStepped = 0.0;
                if (subFrac < subThreshold)
                {
                    subFracStepped = (subFrac / subThreshold) * 0.32;
                }
                else
                {
                    const double st = (subFrac - subThreshold) / (1.0 - subThreshold);
                    subFracStepped = 0.32 + (st * st * (3.0 - 2.0 * st)) * 0.68;
                }
                const double subSteppedWorldH = (std::floor(subPhase) + subFracStepped) * subStep;
                const double subNormElev = subSteppedWorldH / m_config.heightScale;
                elevation = Maths::Lerp(elevation, subNormElev, m_config.subTerracingStrength * flankMask * 0.45);
            }
        }

        // Incised lateral gills (drainage ravines cutting down the fell flanks into the dale)
        if (m_config.gullyStrength > 0.0)
        {
            // Primary stream gill channels with natural meandering warp
            const double lateralWarp = m_noise.Perlin(u * 0.035, v * 0.035) * 4.2;
            const double gu1 = (u + lateralWarp) * m_config.gullyFrequency;
            const double gullyNoise1 = m_noise.Perlin(gu1, 42.17);
            const double crease1 = 1.0 - std::abs(gullyNoise1);
            const double gullyMask1 = crease1 * crease1 * crease1 * crease1;

            const double flankGully = std::clamp((distFromCenter - halfFloor) / 35.0, 0.0, 1.0);
            elevation -= gullyMask1 * flankGully * (m_config.gullyStrength * 0.18);

            // High-frequency secondary ravines and tributary incisions
            if (m_config.tributaryStrength > 0.0)
            {
                const double gu2 = (u - lateralWarp * 0.6) * (m_config.gullyFrequency * 2.85);
                const double gullyNoise2 = m_noise.Perlin(gu2, 87.31);
                const double crease2 = 1.0 - std::abs(gullyNoise2);
                const double gullyMask2 = crease2 * crease2 * crease2 * crease2;
                elevation -= gullyMask2 * flankGully * (m_config.tributaryStrength * 0.12);
            }
        }

        // High fell plateau levelling (modelling the sprawling, peaty summit plateau of Baugh Fell)
        if (elevation > m_config.plateauElevation)
        {
            const double excess = elevation - m_config.plateauElevation;
            const double compressedExcess = m_config.plateauSoftness * std::tanh(excess / m_config.plateauSoftness);
            elevation = m_config.plateauElevation + compressedExcess;

            // Multi-scale peat-hag micro-relief across the high plateau
            const double peatHags = m_noise.Perlin(x * 0.08, z * 0.08) * 0.012 +
                                    m_noise.Perlin(x * 0.22, z * 0.22) * 0.005 +
                                    m_noise.Perlin(x * 0.55, z * 0.55) * 0.002;
            elevation += peatHags;
        }

        // Central river channel incision (River Clough traversing the dale floor)
        const double riverCenter = meander + CalculateRiverOffset(u);
        const double distFromRiver = std::abs(v - riverCenter);
        if (m_config.riverIncidence > 0.0 && distFromRiver < m_config.riverWidth * 1.8)
        {
            const double riverT = distFromRiver / (m_config.riverWidth * 0.5);
            if (riverT < 1.0)
            {
                const double channelProfile = std::cos(riverT * Maths::HalfPi<double>);
                const double riverRiffles = m_noise.Perlin(u * 0.22, 19.84) * 0.15;
                elevation -= (channelProfile * (m_config.riverIncidence + riverRiffles)) / m_config.heightScale;
            }
        }

        // Cellular drumlins on lower slopes and passes
        if (m_config.cellularStrength > 0.0)
        {
            const double cosA = std::cos(m_config.cellularAngle);
            const double sinA = std::sin(m_config.cellularAngle);
            const double rotX = x * cosA - z * sinA;
            const double rotZ = (x * sinA + z * cosA) / m_config.cellularStretch;

            const double cellDist = m_noise.Worley(rotX * m_config.cellularFrequency, rotZ * m_config.cellularFrequency);
            const double drumlin = std::max(0.0, 1.0 - cellDist * cellDist);
            const double drumlinMask = std::clamp(1.0 - troughFactor * 1.3, 0.0, 1.0);
            elevation += drumlin * drumlinMask * (m_config.cellularStrength * 0.12);
        }

        // High-frequency rocky scree and talus slope granularity
        if (m_config.screeGranularity > 0.0)
        {
            const double screeNoise = m_noise.Perlin(x * 0.18, z * 0.18) * 0.55 +
                                      m_noise.Perlin(x * 0.48, z * 0.48) * 0.32 +
                                      m_noise.Perlin(x * 1.20, z * 1.20) * 0.13;
            const double screeMask = troughFactor * std::clamp((distFromCenter - halfFloor * 0.8) / 30.0, 0.0, 1.0);
            elevation += screeNoise * screeMask * m_config.screeGranularity;
        }

        // Rescale normalised elevation to world units relative to configured origin
        return m_config.origin.y + (elevation - 0.18) * m_config.heightScale;
    }

    Vec3 TerrainGenerator::CalculateNormal(double x, double z, double sampleStep) const noexcept
    {
        const double hL = GenerateHeight(x - sampleStep, z);
        const double hR = GenerateHeight(x + sampleStep, z);
        const double hD = GenerateHeight(x, z - sampleStep);
        const double hU = GenerateHeight(x, z + sampleStep);

        const Vec3 normal(
            static_cast<float>(hL - hR),
            static_cast<float>(2.0 * sampleStep),
            static_cast<float>(hD - hU)
        );

        return normal.Normalised();
    }

    Vec4 TerrainGenerator::EvaluateColor(double x, double y, double z, const Vec3& normal) const noexcept
    {
        // Slope gradient factor: 0.0 = horizontal plane, 1.0 = vertical cliff face
        const float slope = 1.0f - std::clamp(normal.y, 0.0f, 1.0f);

        // Normalised altitude factor across the terrain vertical range
        const float altNorm = static_cast<float>(std::clamp((y - m_config.origin.y + 4.0) / (m_config.heightScale * 0.90), 0.0, 1.0));

        // Organic micro-variation mottling across multi-scale noise octaves
        const float mottling = m_noise.Perlin(static_cast<float>(x * 0.14), static_cast<float>(z * 0.14)) * 0.028f +
                               m_noise.Perlin(static_cast<float>(x * 0.52), static_cast<float>(z * 0.52)) * 0.016f +
                               m_noise.Perlin(static_cast<float>(x * 1.35), static_cast<float>(z * 1.35)) * 0.009f +
                               m_noise.Perlin(static_cast<float>(x * 3.40), static_cast<float>(z * 3.40)) * 0.004f;

        // Valley centerline distance calculation for riverbed and pasture identification
        constexpr double valleyAngle = 0.22;
        const double cosV = std::cos(valleyAngle);
        const double sinV = std::sin(valleyAngle);
        const double u = x * cosV + z * sinV;
        const double v = -x * sinV + z * cosV;
        const double meander = CalculateValleyCenterline(u);
        const double distFromCenter = std::abs(v - meander);
        const double riverCenter = meander + CalculateRiverOffset(u);
        const double distFromRiver = std::abs(v - riverCenter);

        // Base vegetation palette evaluated from altitude gradient
        Vec4 baseVegColor;
        if (altNorm < 0.22f)
        {
            // Valley floor alluvial pastures: lush emerald green
            const float t = altNorm / 0.22f;
            baseVegColor = m_config.valleyFloorColor.Lerp(m_config.lowSlopeColor, t);

            // Riverbed gravel ribbon along the meandering channel
            if (distFromRiver < m_config.riverWidth * 0.75)
            {
                const float riverFade = 1.0f - static_cast<float>(distFromRiver / (m_config.riverWidth * 0.75));
                baseVegColor = baseVegColor.Lerp(m_config.riverbedColor, riverFade * 0.85f);
            }
        }
        else if (altNorm < 0.58f)
        {
            // Lower and mid fell slopes: intake meadows, bracken, and rough moorland pasture
            const float t = (altNorm - 0.22f) / 0.36f;
            baseVegColor = m_config.lowSlopeColor.Lerp(m_config.midSlopeColor, t);
        }
        else
        {
            // Upper slopes and high fell plateau (Baugh Fell summit):
            // Sunlit golden-straw mat-grass and fescue turf
            const float t = std::clamp((altNorm - 0.58f) / 0.42f, 0.0f, 1.0f);
            baseVegColor = m_config.midSlopeColor.Lerp(m_config.highPlateauColor, t);

            // Heather moorland patches on upper slopes
            const float moorPatchNoise = static_cast<float>(m_noise.Perlin(x * 0.008, z * 0.008));
            if (altNorm > 0.52f && moorPatchNoise > 0.15f)
            {
                const float heatherWeight = std::clamp((moorPatchNoise - 0.15f) / 0.35f, 0.0f, 0.65f);
                baseVegColor = baseVegColor.Lerp(m_config.heatherColor, heatherWeight);
            }

            // Localized peat hags: restricted strictly to hollows and drainage channels
            // (never blanketing the whole summit)
            if (altNorm > 0.74f && moorPatchNoise < -0.32f && slope < 0.18f)
            {
                const float peatWeight = std::clamp((-moorPatchNoise - 0.32f) / 0.28f, 0.0f, 0.85f);
                baseVegColor = baseVegColor.Lerp(m_config.peatMoorColor, peatWeight);
            }
        }

        // Stepped cyclothem limestone scar and rocky crag exposure
        Vec4 finalColor;
        if (slope > 0.38f)
        {
            // Sheer cliff and steep crag face (> 45 degrees): dark wet rock with bedding laminations
            const float t = std::clamp((slope - 0.38f) / 0.22f, 0.0f, 1.0f);
            const float cragBanding = std::sin(static_cast<float>(y * 2.8)) * 0.035f +
                                      std::sin(static_cast<float>(y * 7.5)) * 0.020f;
            Vec4 cragTone = m_config.rockColor.Lerp(m_config.steepCragColor, t);
            cragTone.x = std::clamp(cragTone.x + cragBanding, 0.0f, 1.0f);
            cragTone.y = std::clamp(cragTone.y + cragBanding, 0.0f, 1.0f);
            cragTone.z = std::clamp(cragTone.z + cragBanding, 0.0f, 1.0f);
            finalColor = cragTone;
        }
        else if (slope > 0.24f)
        {
            // Stepped scar risers and scree benches (~35 to 45 degrees): exposed pale Yoredale limestone
            const float t = (slope - 0.24f) / 0.14f;
            const float stratumBanding = std::sin(static_cast<float>(y * 2.8)) * 0.040f +
                                         std::sin(static_cast<float>(y * 8.5)) * 0.022f;
            Vec4 scarTone = m_config.limestoneScarColor.Lerp(m_config.rockColor, 0.35f);
            scarTone.x = std::clamp(scarTone.x + stratumBanding, 0.0f, 1.0f);
            scarTone.y = std::clamp(scarTone.y + stratumBanding, 0.0f, 1.0f);
            scarTone.z = std::clamp(scarTone.z + stratumBanding, 0.0f, 1.0f);
            finalColor = baseVegColor.Lerp(scarTone, t);
        }
        else
        {
            // Fully vegetated fell flanks and pastures
            finalColor = baseVegColor;
        }

        // Apply subtle rush clump accents on lower and mid slopes
        const float rushNoise = static_cast<float>(m_noise.Perlin(x * 0.10, z * 0.10));
        if (altNorm < 0.65f && rushNoise > 0.38f && slope < 0.20f)
        {
            const Vec4 rushColor(0.26f, 0.38f, 0.16f, 1.0f);
            finalColor = finalColor.Lerp(rushColor, std::clamp((rushNoise - 0.38f) / 0.30f, 0.0f, 0.35f));
        }

        // Apply subtle organic luminance variation
        finalColor.x = std::clamp(finalColor.x + mottling, 0.0f, 1.0f);
        finalColor.y = std::clamp(finalColor.y + mottling, 0.0f, 1.0f);
        finalColor.z = std::clamp(finalColor.z + mottling, 0.0f, 1.0f);

        return finalColor;
    }
}

