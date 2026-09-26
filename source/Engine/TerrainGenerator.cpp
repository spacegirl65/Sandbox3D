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

    namespace TC = TerrainConstants;

    double TerrainGenerator::CalculateValleyCenterline(double u) const noexcept
    {
        // Gentle, naturalistic meandering curve across the dale
        const double phase1 = u * (Maths::TwoPi<double> / TC::Valley::PrimaryMeanderWavelength);
        const double primaryBend = std::sin(phase1);

        // Soft secondary harmonic for natural asymmetry
        const double phase2 = u * (Maths::TwoPi<double> / TC::Valley::SecondaryMeanderWavelength) + TC::Valley::SecondaryMeanderPhaseOffset;
        const double secondaryBend = std::sin(phase2) * TC::Valley::SecondaryMeanderAmplitude;

        // Subtle organic wandering from Perlin noise
        const double organicWarp = m_noise.Perlin(u * 0.009, 13.37) * 0.20 +
                                   m_noise.Perlin(u * 0.020, 47.19) * 0.08;

        const double normalizedMeander = primaryBend * TC::Valley::PrimaryMeanderWeight + secondaryBend + organicWarp;
        return normalizedMeander * m_config.valleyMeander;
    }

    double TerrainGenerator::CalculateRiverOffset(double u) const noexcept
    {
        // High-frequency natural wandering of the river channel within the alluvial valley floor
        return (m_noise.Perlin(u * 0.018, 31.41) * TC::River::MeanderNoiseWeight1 +
                m_noise.Perlin(u * 0.045, 78.19) * TC::River::MeanderNoiseWeight2) * (m_config.riverWidth * 0.50);
    }

    double TerrainGenerator::GenerateHeight(double x, double z) const noexcept
    {
        // Primary dale valley trough orientation and coordinate transform
        // Valley axis runs along an east-west orientation with natural meandering warp
        const double cosV = std::cos(TC::Valley::ValleyInclinationAngle);
        const double sinV = std::sin(TC::Valley::ValleyInclinationAngle);
        const double u = x * cosV + z * sinV;  // Along-valley coordinate
        const double v = -x * sinV + z * cosV; // Across-valley coordinate

        // Multi-harmonic curved wandering of the valley centerline
        const double meander = CalculateValleyCenterline(u);
        const double distFromCenter = std::abs(v - meander);

        // Glaciated U/V dale trough profile
        const double halfWidth = m_config.width * TC::Valley::ValleyHalfWidthFactor;
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
        const double fellAsymmetry = (v > meander) ? TC::Valley::NorthernMassifAsymmetry : TC::Valley::SouthernRidgeAsymmetry;

        // Base landscape elevation combining glaciated dale trough with macro fell relief
        double elevation = (troughFactor * m_config.valleyDepth) +
                           (std::pow(normMacro, m_config.convexity) * (1.0 - m_config.valleyDepth * TC::Valley::MacroConvexityDamping)) +
                           (fellAsymmetry * troughFactor);

        // Stepped cyclothem stratification (Carboniferous Yoredale limestone benches and scars)
        const double worldH = elevation * m_config.heightScale;
        const double flankMask = std::clamp((distFromCenter - halfFloor * TC::Valley::FlankFloorMargin) / TC::Valley::FlankMaskWidth, 0.0, 1.0);

        if (m_config.terracingStrength > 0.0 && m_config.cyclothemStepHeight > 0.0)
        {
            const double step = m_config.cyclothemStepHeight;
            const double layerPhase = worldH / step;
            const double layerIdx = std::floor(layerPhase);
            const double layerFrac = layerPhase - layerIdx; // [0, 1)

            // Non-linear bench profile: broad flat structural shelf transitioning to a steep vertical scar riser
            const double scarThreshold = 1.0 - m_config.cyclothemScarSharpness * TC::Stratum::ScarSharpnessScale;
            double steppedFrac = 0.0;
            if (layerFrac < scarThreshold)
            {
                // Gentle structural terrace shelf
                steppedFrac = (layerFrac / scarThreshold) * TC::Stratum::TerraceShelfFraction;
            }
            else
            {
                // Steep rocky scar riser (using smoothstep)
                const double t = (layerFrac - scarThreshold) / (1.0 - scarThreshold);
                const double s = t * t * (3.0 - 2.0 * t);
                steppedFrac = TC::Stratum::TerraceShelfFraction + s * (1.0 - TC::Stratum::TerraceShelfFraction);
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
                const double subThreshold = TC::Stratum::SubTerraceThreshold;
                double subFracStepped = 0.0;
                if (subFrac < subThreshold)
                {
                    subFracStepped = (subFrac / subThreshold) * TC::Stratum::SubTerraceShelfFraction;
                }
                else
                {
                    const double st = (subFrac - subThreshold) / (1.0 - subThreshold);
                    subFracStepped = TC::Stratum::SubTerraceShelfFraction + (st * st * (3.0 - 2.0 * st)) * (1.0 - TC::Stratum::SubTerraceShelfFraction);
                }
                const double subSteppedWorldH = (std::floor(subPhase) + subFracStepped) * subStep;
                const double subNormElev = subSteppedWorldH / m_config.heightScale;
                elevation = Maths::Lerp(elevation, subNormElev, m_config.subTerracingStrength * flankMask * TC::Stratum::SubTerraceDamping);
            }
        }

        // Incised lateral gills (drainage ravines cutting down the fell flanks into the dale)
        if (m_config.gullyStrength > 0.0)
        {
            // Primary stream gill channels with natural meandering warp
            const double lateralWarp = m_noise.Perlin(u * TC::Gills::LateralWarpFrequency, v * TC::Gills::LateralWarpFrequency) * TC::Gills::LateralWarpAmplitude;
            const double gu1 = (u + lateralWarp) * m_config.gullyFrequency;
            const double gullyNoise1 = m_noise.Perlin(gu1, 42.17);
            const double crease1 = 1.0 - std::abs(gullyNoise1);
            const double gullyMask1 = crease1 * crease1 * crease1 * crease1;

            const double flankGully = std::clamp((distFromCenter - halfFloor) / TC::Gills::FlankGullyWidth, 0.0, 1.0);
            elevation -= gullyMask1 * flankGully * (m_config.gullyStrength * TC::Gills::GullyDepthScale);

            // High-frequency secondary ravines and tributary incisions
            if (m_config.tributaryStrength > 0.0)
            {
                const double gu2 = (u - lateralWarp * TC::Gills::TributaryWarpMultiplier) * (m_config.gullyFrequency * TC::Gills::TributaryFrequencyMultiplier);
                const double gullyNoise2 = m_noise.Perlin(gu2, 87.31);
                const double crease2 = 1.0 - std::abs(gullyNoise2);
                const double gullyMask2 = crease2 * crease2 * crease2 * crease2;
                elevation -= gullyMask2 * flankGully * (m_config.tributaryStrength * TC::Gills::TributaryDepthScale);
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
        if (m_config.riverIncidence > 0.0 && distFromRiver < m_config.riverWidth * TC::River::ValleyFloorFadeMargin)
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
            const double drumlinMask = std::clamp(1.0 - troughFactor * TC::Moorland::DrumlinTroughDamping, 0.0, 1.0);
            elevation += drumlin * drumlinMask * (m_config.cellularStrength * TC::Moorland::DrumlinHeightScale);
        }

        // High-frequency rocky scree and talus slope granularity
        if (m_config.screeGranularity > 0.0)
        {
            const double screeNoise = m_noise.Perlin(x * 0.18, z * 0.18) * 0.55 +
                                      m_noise.Perlin(x * 0.48, z * 0.48) * 0.32 +
                                      m_noise.Perlin(x * 1.20, z * 1.20) * 0.13;
            const double screeMask = troughFactor * std::clamp((distFromCenter - halfFloor * TC::Moorland::ScreeDistanceMargin) / TC::Moorland::ScreeTransitionWidth, 0.0, 1.0);
            elevation += screeNoise * screeMask * m_config.screeGranularity;
        }

        // Rescale normalised elevation to world units relative to configured origin
        return m_config.origin.y + (elevation - TC::Moorland::ElevationDatumOffset) * m_config.heightScale;
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
        const float altNorm = static_cast<float>(std::clamp(
            (y - m_config.origin.y + TC::Detailing::AltitudeDatumOffset) / (m_config.heightScale * TC::Detailing::AltitudeSpanFactor),
            0.0,
            1.0
        ));

        // Organic micro-variation mottling across multi-scale noise octaves
        const float mottling = m_noise.Perlin(static_cast<float>(x * 0.14), static_cast<float>(z * 0.14)) * 0.028f +
                               m_noise.Perlin(static_cast<float>(x * 0.52), static_cast<float>(z * 0.52)) * 0.016f +
                               m_noise.Perlin(static_cast<float>(x * 1.35), static_cast<float>(z * 1.35)) * 0.009f +
                               m_noise.Perlin(static_cast<float>(x * 3.40), static_cast<float>(z * 3.40)) * 0.004f;

        // Valley centerline distance calculation for riverbed and pasture identification
        const double cosV = std::cos(TC::Valley::ValleyInclinationAngle);
        const double sinV = std::sin(TC::Valley::ValleyInclinationAngle);
        const double u = x * cosV + z * sinV;
        const double v = -x * sinV + z * cosV;
        const double meander = CalculateValleyCenterline(u);
        const double distFromCenter = std::abs(v - meander);
        const double riverCenter = meander + CalculateRiverOffset(u);
        const double distFromRiver = std::abs(v - riverCenter);

        // Base vegetation palette evaluated from altitude gradient
        Vec4 baseVegColor;
        if (altNorm < m_config.valleyPastureMaxAlt)
        {
            // Valley floor alluvial pastures: lush emerald green
            const float t = altNorm / m_config.valleyPastureMaxAlt;
            baseVegColor = m_config.valleyFloorColor.Lerp(m_config.lowSlopeColor, t);

            // Riverbed gravel ribbon along the meandering channel
            const double riverbedLimit = m_config.riverWidth * TC::River::RiverbedWidthFactor;
            if (distFromRiver < riverbedLimit)
            {
                const float riverFade = 1.0f - static_cast<float>(distFromRiver / riverbedLimit);
                baseVegColor = baseVegColor.Lerp(m_config.riverbedColor, riverFade * TC::River::RiverbedMaxFade);
            }
        }
        else if (altNorm < m_config.lowerSlopeMaxAlt)
        {
            // Lower and mid fell slopes: intake meadows, bracken, and rough moorland pasture
            const float t = (altNorm - m_config.valleyPastureMaxAlt) / (m_config.lowerSlopeMaxAlt - m_config.valleyPastureMaxAlt);
            baseVegColor = m_config.lowSlopeColor.Lerp(m_config.midSlopeColor, t);
        }
        else
        {
            // Upper slopes and high fell plateau (Baugh Fell summit):
            // Sunlit golden-straw mat-grass and fescue turf
            const float t = std::clamp((altNorm - m_config.lowerSlopeMaxAlt) / (1.0f - m_config.lowerSlopeMaxAlt), 0.0f, 1.0f);
            baseVegColor = m_config.midSlopeColor.Lerp(m_config.highPlateauColor, t);

            // Heather moorland accents on upper slopes: subtly and smoothly blended into fescues
            const float moorPatchNoise = static_cast<float>(m_noise.Perlin(x * 0.006, z * 0.006) * TC::River::MeanderNoiseWeight1 +
                                                            m_noise.Perlin(x * 0.018, z * 0.018) * TC::River::MeanderNoiseWeight2);
            if (altNorm > m_config.heatherAltitudeThreshold && moorPatchNoise > TC::Detailing::HeatherNoiseThreshold)
            {
                const float rawT = std::clamp((moorPatchNoise - TC::Detailing::HeatherNoiseThreshold) / TC::Detailing::HeatherNoiseRamp, 0.0f, 1.0f);
                const float smoothT = rawT * rawT * (3.0f - 2.0f * rawT);
                const float heatherWeight = smoothT * TC::Detailing::HeatherMaxWeight;
                baseVegColor = baseVegColor.Lerp(m_config.heatherColor, heatherWeight);
            }

            // Localized peat hollows: gentle, softly blended depressions
            // (restricted strictly to hollows, never forming harsh dark blotches)
            if (altNorm > m_config.peatAltitudeThreshold && moorPatchNoise < TC::Detailing::PeatNoiseThreshold && slope < m_config.peatMoorMaxSlope)
            {
                const float rawT = std::clamp((-moorPatchNoise + TC::Detailing::PeatNoiseThreshold) / TC::Detailing::PeatNoiseRamp, 0.0f, 1.0f);
                const float smoothT = rawT * rawT * (3.0f - 2.0f * rawT);
                const float peatWeight = smoothT * TC::Detailing::PeatMaxWeight;
                baseVegColor = baseVegColor.Lerp(m_config.peatMoorColor, peatWeight);
            }
        }

        // Topographical concavity evaluation: detects hillside crevices, ravines, and gills
        const double concavityStep = TC::Detailing::ConcavitySampleStep;
        const double yL = GenerateHeight(x - concavityStep, z);
        const double yR = GenerateHeight(x + concavityStep, z);
        const double yD = GenerateHeight(x, z - concavityStep);
        const double yU = GenerateHeight(x, z + concavityStep);
        const float concavity = static_cast<float>((yL + yR + yD + yU) * 0.25 - y);

        // Scale crevice threshold linearly upwards to creviceMaxThreshold between creviceScalingStartAlt and creviceScalingEndAlt
        float creviceThresh = static_cast<float>(m_config.creviceThreshold);
        if (altNorm < m_config.creviceScalingStartAlt)
        {
            const float altSpan = m_config.creviceScalingStartAlt - m_config.creviceScalingEndAlt;
            const float t = (altSpan > 0.0f)
                ? std::clamp((altNorm - m_config.creviceScalingEndAlt) / altSpan, 0.0f, 1.0f)
                : 1.0f;
            creviceThresh = std::lerp(m_config.creviceMaxThreshold, static_cast<float>(m_config.creviceThreshold), t);
        }
        const float scarGate = std::min(creviceThresh * TC::Detailing::ScarGateRatio, TC::Detailing::ScarGateCeiling);

        // Stepped cyclothem limestone scar and rocky crag exposure
        Vec4 finalColor;
        if (slope > m_config.sheerCragSlope)
        {
            // Sheer cliff and steep crag face (> 45 degrees): dark wet rock with bedding laminations
            const float t = std::clamp((slope - m_config.sheerCragSlope) / TC::Detailing::SheerCragSlopeRamp, 0.0f, 1.0f);
            const float cragBanding = std::sin(static_cast<float>(y * 2.8)) * 0.035f +
                                      std::sin(static_cast<float>(y * 7.5)) * 0.020f;
            Vec4 cragTone = m_config.rockColor.Lerp(m_config.steepCragColor, t);
            cragTone.x = std::clamp(cragTone.x + cragBanding, 0.0f, 1.0f);
            cragTone.y = std::clamp(cragTone.y + cragBanding, 0.0f, 1.0f);
            cragTone.z = std::clamp(cragTone.z + cragBanding, 0.0f, 1.0f);
            finalColor = cragTone;
        }
        else if (slope > m_config.limestoneScarSlope && concavity < scarGate)
        {
            // Stepped scar risers and scree benches (~35 to 45 degrees): exposed pale Yoredale limestone
            // Restricted to structural benches where concavity is low (not inside drainage furrows)
            const float rawT = std::clamp((slope - m_config.limestoneScarSlope) / TC::Detailing::LimestoneScarSlopeRamp, 0.0f, 1.0f);
            const float factor = rawT * rawT * (3.0f - 2.0f * rawT);
            const float stratumBanding = std::sin(static_cast<float>(y * 2.8)) * 0.040f +
                                         std::sin(static_cast<float>(y * 8.5)) * 0.022f;
            Vec4 scarTone = m_config.limestoneScarColor.Lerp(m_config.rockColor, TC::Detailing::ScarToneRockRatio);
            scarTone.x = std::clamp(scarTone.x + stratumBanding, 0.0f, 1.0f);
            scarTone.y = std::clamp(scarTone.y + stratumBanding, 0.0f, 1.0f);
            scarTone.z = std::clamp(scarTone.z + stratumBanding, 0.0f, 1.0f);
            finalColor = baseVegColor.Lerp(scarTone, factor);
        }
        else
        {
            // Fully vegetated fell flanks and pastures
            finalColor = baseVegColor;
        }

        // Crevice bed detailing: subtle weathered rock accents along incised hillside furrows and gills
        if (concavity > creviceThresh)
        {
            const float bedRaw = std::clamp((concavity - creviceThresh) / m_config.creviceConcavityRamp, 0.0f, 1.0f);
            const float bedFactor = bedRaw * bedRaw * (3.0f - 2.0f * bedRaw);

            // Variegated rocky bed: spatial noise creates a natural mixture of lighter cobbles and darker wet stone
            const float stoneNoise = static_cast<float>(
                m_noise.Perlin(x * 0.055, z * 0.055) * TC::River::MeanderNoiseWeight1 +
                m_noise.Perlin(x * 0.150, z * 0.150) * TC::River::MeanderNoiseWeight2
            );

            // Lighter shade: pale limestone scar cobbles and riverbed gravel
            const Vec4 lightShade = m_config.riverbedColor.Lerp(m_config.limestoneScarColor, TC::Detailing::LightShadeLimestoneRatio);

            // Medium shade: weathered gritstone rock and gravel
            const Vec4 midShade = m_config.riverbedColor.Lerp(m_config.rockColor, TC::Detailing::MidShadeRockRatio);

            // Darker shade: damp stone and shadowed crag
            const Vec4 darkShade = m_config.rockColor.Lerp(m_config.steepCragColor, TC::Detailing::DarkShadeSteepCragRatio);

            // Select shade based on local stone noise and channel steepness
            const float steepBias = std::clamp((slope - TC::Detailing::SteepBiasSlopeThreshold) / TC::Detailing::SteepBiasSlopeRamp, 0.0f, 1.0f);
            Vec4 creviceBedColor;
            if (stoneNoise > 0.0f)
            {
                const float lightFactor = std::clamp(stoneNoise / TC::Detailing::StoneNoiseRamp, 0.0f, 1.0f);
                creviceBedColor = midShade.Lerp(lightShade, lightFactor);
            }
            else
            {
                const float darkFactor = std::clamp(-stoneNoise / TC::Detailing::StoneNoiseRamp, 0.0f, 1.0f);
                creviceBedColor = midShade.Lerp(darkShade, darkFactor);
            }

            // Steeper sections expose slightly darker stone
            creviceBedColor = creviceBedColor.Lerp(darkShade, steepBias * TC::Detailing::SteepBiasMaxWeight);

            // Soft, well-balanced blending into the hillside vegetation
            const float blendStrength = bedFactor * m_config.creviceMaxBlendStrength;
            finalColor = finalColor.Lerp(creviceBedColor, blendStrength);
        }

        // Apply subtle rush clump accents on lower and mid slopes
        const float rushNoise = static_cast<float>(m_noise.Perlin(x * 0.10, z * 0.10));
        if (altNorm < m_config.rushAltitudeMax && rushNoise > TC::Detailing::RushNoiseThreshold && slope < m_config.rushMaxSlope)
        {
            const float rawT = std::clamp((rushNoise - TC::Detailing::RushNoiseThreshold) / TC::Detailing::RushNoiseRamp, 0.0f, 1.0f);
            const float smoothT = rawT * rawT * (3.0f - 2.0f * rawT);
            finalColor = finalColor.Lerp(m_config.lowSlopeColor, smoothT * TC::Detailing::RushMaxWeight);
        }

        // Apply subtle organic luminance variation
        finalColor.x = std::clamp(finalColor.x + mottling, 0.0f, 1.0f);
        finalColor.y = std::clamp(finalColor.y + mottling, 0.0f, 1.0f);
        finalColor.z = std::clamp(finalColor.z + mottling, 0.0f, 1.0f);

        return finalColor;
    }
}

