// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "TerrainGenerator.h"

#include <algorithm>
#include <cmath>

namespace Sandbox3D::Terrain
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

    double TerrainGenerator::GenerateHeight(double x, double z) const noexcept
    {
        // 1. Primary dale valley trough orientation and coordinate transform
        // Valley axis runs along an east-west orientation with natural meandering warp
        constexpr double valleyAngle = 0.22; // ~12 degrees inclination
        const double cosV = std::cos(valleyAngle);
        const double sinV = std::sin(valleyAngle);
        const double u = x * cosV + z * sinV;  // Along-valley coordinate
        const double v = -x * sinV + z * cosV; // Across-valley coordinate

        // Low-frequency wandering warp of the valley centerline
        const double meander = m_noise.Perlin(u * 0.0075, 13.37) * m_config.valleyMeander;
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

        // 2. Macro fell topography using multi-octave Fractal Brownian Motion
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

        // 3. Stepped cyclothem stratification (Carboniferous Yoredale limestone benches and scars)
        if (m_config.terracingStrength > 0.0 && m_config.cyclothemStepHeight > 0.0)
        {
            const double worldH = elevation * m_config.heightScale;
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
            const double flankMask = std::clamp((distFromCenter - halfFloor * 0.6) / 28.0, 0.0, 1.0);
            elevation = Maths::Lerp(elevation, steppedNormElev, m_config.terracingStrength * flankMask);
        }

        // 4. Incised lateral gills (drainage ravines cutting down the fell flanks into the dale)
        if (m_config.gullyStrength > 0.0)
        {
            const double gu = u * m_config.gullyFrequency;
            const double gullyNoise = m_noise.Perlin(gu, 42.17);
            const double crease = 1.0 - std::abs(gullyNoise);
            const double gullyMask = crease * crease * crease;

            const double flankGully = std::clamp((distFromCenter - halfFloor) / 35.0, 0.0, 1.0);
            elevation -= gullyMask * flankGully * (m_config.gullyStrength * 0.22);
        }

        // 5. High fell plateau levelling (modelling the sprawling, peaty summit plateau of Baugh Fell)
        if (elevation > m_config.plateauElevation)
        {
            const double excess = elevation - m_config.plateauElevation;
            const double compressedExcess = m_config.plateauSoftness * std::tanh(excess / m_config.plateauSoftness);
            elevation = m_config.plateauElevation + compressedExcess;

            // Peat-hag micro-relief across the high plateau
            const double peatHags = m_noise.Perlin(x * 0.07, z * 0.07) * 0.012;
            elevation += peatHags;
        }

        // 6. Central river channel incision (River Clough traversing the dale floor)
        if (m_config.riverIncidence > 0.0 && distFromCenter < m_config.riverWidth * 1.8)
        {
            const double riverT = distFromCenter / (m_config.riverWidth * 0.5);
            if (riverT < 1.0)
            {
                const double channelProfile = std::cos(riverT * Maths::HalfPi<double>);
                elevation -= (channelProfile * m_config.riverIncidence) / m_config.heightScale;
            }
        }

        // 7. Cellular drumlins on lower slopes and passes
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
                               m_noise.Perlin(static_cast<float>(x * 0.52), static_cast<float>(z * 0.52)) * 0.015f;

        // Valley centerline distance calculation for riverbed identification
        constexpr double valleyAngle = 0.22;
        const double cosV = std::cos(valleyAngle);
        const double sinV = std::sin(valleyAngle);
        const double u = x * cosV + z * sinV;
        const double v = -x * sinV + z * cosV;
        const double meander = m_noise.Perlin(u * 0.0075, 13.37) * m_config.valleyMeander;
        const double distFromCenter = std::abs(v - meander);

        // Base vegetation palette evaluated from altitude gradient
        Vec4 baseVegColor;
        if (altNorm < 0.22f)
        {
            // Valley floor alluvial pastures: lush emerald green
            const float t = altNorm / 0.22f;
            baseVegColor = m_config.valleyFloorColor.Lerp(m_config.lowSlopeColor, t);

            // Riverbed gravel ribbon along the meandering channel
            if (distFromCenter < m_config.riverWidth * 0.75)
            {
                const float riverFade = 1.0f - static_cast<float>(distFromCenter / (m_config.riverWidth * 0.75));
                baseVegColor = baseVegColor.Lerp(m_config.riverbedColor, riverFade * 0.85f);
            }
        }
        else if (altNorm < 0.58f)
        {
            // Lower and mid fell slopes: intake meadows, bracken, and rough moorland pasture
            const float t = (altNorm - 0.22f) / 0.36f;
            baseVegColor = m_config.lowSlopeColor.Lerp(m_config.midSlopeColor, t);
        }
        else if (altNorm < 0.82f)
        {
            // Upper fell slopes: moorland grass, bent grass, and heather fringe
            const float t = (altNorm - 0.58f) / 0.24f;
            baseVegColor = m_config.midSlopeColor.Lerp(m_config.highPlateauColor, t);
        }
        else
        {
            // High fell plateau (Baugh Fell summit): dark peat bogs and heather moorland
            const float t = std::clamp((altNorm - 0.82f) / 0.18f, 0.0f, 1.0f);
            baseVegColor = m_config.highPlateauColor.Lerp(m_config.peatMoorColor, t);
        }

        // Stepped cyclothem limestone scar and rocky crag exposure
        Vec4 finalColor;
        if (slope > 0.44f)
        {
            // Sheer cliff / steep crag face: dark wet rock
            const float t = std::clamp((slope - 0.44f) / 0.25f, 0.0f, 1.0f);
            finalColor = m_config.rockColor.Lerp(m_config.steepCragColor, t);
        }
        else if (slope > 0.26f)
        {
            // Stepped scar risers: exposed pale Yoredale limestone benches
            const float t = (slope - 0.26f) / 0.18f;
            const Vec4 scarTone = m_config.limestoneScarColor.Lerp(m_config.rockColor, 0.35f);
            finalColor = baseVegColor.Lerp(scarTone, t);
        }
        else
        {
            // Gentle structural shelves and flat valley pastures
            finalColor = baseVegColor;
        }

        // Apply subtle organic luminance variation
        finalColor.x = std::clamp(finalColor.x + mottling, 0.0f, 1.0f);
        finalColor.y = std::clamp(finalColor.y + mottling, 0.0f, 1.0f);
        finalColor.z = std::clamp(finalColor.z + mottling, 0.0f, 1.0f);

        return finalColor;
    }
}

