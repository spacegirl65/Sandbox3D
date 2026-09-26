// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

namespace Sandbox3D::Engine::TerrainConstants
{
    // Primary dale trough geometry and meandering axis harmonics
    namespace Valley
    {
        inline constexpr double PrimaryMeanderWavelength    = 480.0;
        inline constexpr double SecondaryMeanderWavelength  = 240.0;
        inline constexpr double SecondaryMeanderPhaseOffset = 0.65;
        inline constexpr double SecondaryMeanderAmplitude   = 0.22;
        inline constexpr double PrimaryMeanderWeight        = 0.75;
        inline constexpr double ValleyInclinationAngle      = 0.22; // ~12.6 degrees inclination
        inline constexpr double ValleyHalfWidthFactor       = 0.44;
        inline constexpr double NorthernMassifAsymmetry     = 0.06;
        inline constexpr double SouthernRidgeAsymmetry      = -0.03;
        inline constexpr double MacroConvexityDamping       = 0.45;
        inline constexpr double FlankMaskWidth              = 28.0;
        inline constexpr double FlankFloorMargin            = 0.6;
    }

    // Carboniferous Yoredale cyclothem stepped benches and limestone scars
    namespace Stratum
    {
        inline constexpr double ScarSharpnessScale      = 0.45;
        inline constexpr double TerraceShelfFraction    = 0.22;
        inline constexpr double SubTerraceThreshold     = 0.70;
        inline constexpr double SubTerraceShelfFraction = 0.32;
        inline constexpr double SubTerraceDamping       = 0.45;
    }

    // Incised lateral gills, stream gullies, and drainage ravines
    namespace Gills
    {
        inline constexpr double LateralWarpFrequency           = 0.035;
        inline constexpr double LateralWarpAmplitude           = 4.2;
        inline constexpr double FlankGullyWidth                = 35.0;
        inline constexpr double GullyDepthScale                = 0.18;
        inline constexpr double TributaryFrequencyMultiplier   = 2.85;
        inline constexpr double TributaryWarpMultiplier        = 0.6;
        inline constexpr double TributaryDepthScale            = 0.12;
    }

    // Alluvial river channel, gravel ribbons, and riffles
    namespace River
    {
        inline constexpr double ValleyFloorFadeMargin   = 1.8;
        inline constexpr double MeanderNoiseWeight1     = 0.70;
        inline constexpr double MeanderNoiseWeight2     = 0.30;
        inline constexpr double RiverbedWidthFactor     = 0.75;
        inline constexpr float  RiverbedMaxFade         = 0.85f;
    }

    // High plateau peat hags, cellular drumlins, and talus scree
    namespace Moorland
    {
        inline constexpr double DrumlinTroughDamping   = 1.3;
        inline constexpr double DrumlinHeightScale     = 0.12;
        inline constexpr double ScreeDistanceMargin    = 0.8;
        inline constexpr double ScreeTransitionWidth   = 30.0;
        inline constexpr double ElevationDatumOffset   = 0.18;
    }

    // Surface biome shading, multi-frequency noise, and crevice detailing
    namespace Detailing
    {
        // Discrete 3x3 Laplacian concavity stencil weights
        inline constexpr float LaplaceCrossWeight          = 0.60f;
        inline constexpr float LaplaceDiagWeight           = 0.40f;

        // Concavity evaluation sampling step in metres
        inline constexpr double ConcavitySampleStep        = 2.0;

        // Heather moorland accents
        inline constexpr float HeatherNoiseThreshold       = 0.08f;
        inline constexpr float HeatherNoiseRamp            = 0.45f;
        inline constexpr float HeatherMaxWeight            = 0.22f;

        // Upland peat hollows
        inline constexpr float PeatNoiseThreshold          = -0.18f;
        inline constexpr float PeatNoiseRamp               = 0.40f;
        inline constexpr float PeatMaxWeight               = 0.20f;

        // Cyclothem limestone scar gating and tones
        inline constexpr float ScarGateRatio               = 0.75f;
        inline constexpr float ScarGateCeiling             = 0.16f;
        inline constexpr float ScarToneRockRatio           = 0.35f;

        // Slope transition ramps
        inline constexpr float SheerCragSlopeRamp          = 0.22f;
        inline constexpr float LimestoneScarSlopeRamp      = 0.14f;

        // Altitude normalisation datum offset and span factor
        inline constexpr double AltitudeDatumOffset        = 4.0;
        inline constexpr double AltitudeSpanFactor         = 0.90;

        // Wetland rush clumps
        inline constexpr float RushNoiseThreshold          = 0.38f;
        inline constexpr float RushNoiseRamp               = 0.35f;
        inline constexpr float RushMaxWeight               = 0.18f;

        // Weathered rock and gravel shade ratios
        inline constexpr float LightShadeLimestoneRatio    = 0.55f;
        inline constexpr float MidShadeRockRatio           = 0.50f;
        inline constexpr float DarkShadeSteepCragRatio     = 0.40f;

        // Crevice steepness bias and stone noise
        inline constexpr float SteepBiasSlopeThreshold     = 0.18f;
        inline constexpr float SteepBiasSlopeRamp          = 0.18f;
        inline constexpr float SteepBiasMaxWeight          = 0.35f;
        inline constexpr float StoneNoiseRamp              = 0.45f;
    }
}
