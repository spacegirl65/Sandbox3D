// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Maths/Maths.h"
#include <cstdint>

namespace Sandbox3D::Engine
{
    using Maths::Vec3D;
    using Maths::Vec4;

    // Generic, re-usable configuration parameters for procedural terrain generation
    struct TerrainConfig
    {
        // Grid spatial bounds and resolution
        double   width{ 520.0 };          // Total width along X axis in metres (doubled from 260m)
        double   depth{ 520.0 };          // Total depth along Z axis in metres (doubled from 260m)
        uint32_t resolutionX{ 1024 };     // Number of vertex samples along X (~0.5m grid step)
        uint32_t resolutionZ{ 1024 };     // Number of vertex samples along Z (~0.5m grid step)
        Vec3D    origin{ 0.0, 0.0, 0.0 }; // Centre/origin in world space

        // Topographical elevation scaling and frequency
        double   heightScale{ 104.0 };    // Maximum vertical elevation scale in metres (doubled from 52m)
        double   baseFrequency{ 0.00325 };// Base spatial frequency for macro landscape (scaled for 2x bounds)
        int      octaves{ 11 };           // Number of octaves for fine-grained fractal synthesis
        float    persistence{ 0.49f };    // Roughness persistence between octaves
        float    lacunarity{ 2.05f };     // Frequency multiplier between octaves

        // Dale trough and valley profile
        double   valleyDepth{ 0.65 };          // Primary dale trough depression depth
        double   valleyFloorWidth{ 52.0 };     // Width of gently graded alluvial valley floor in metres (doubled from 26m)
        double   valleyFlankSteepness{ 1.85 }; // Curvature/steepness exponent of the valley walls
        double   valleyMeander{ 48.0 };        // Naturalistic meandering amplitude of valley axis in metres
        double   riverIncidence{ 3.6 };        // Central riverbed incision depth in metres (doubled from 1.8m)
        double   riverWidth{ 13.0 };           // Central riverbed width in metres (doubled from 6.5m)

        // Stepped cyclothem stratification (geological benches and scars)
        double   terracingStrength{ 0.48 };     // Stepped scar prominence
        double   cyclothemStepHeight{ 14.4 };   // Vertical interval between stepped geological benches in metres (doubled from 7.2m)
        double   cyclothemScarSharpness{ 0.85 };// Crispness of vertical scar transition
        double   subTerracingStrength{ 0.22 };  // Secondary geological bedding plane granularity
        double   subTerracingStepHeight{ 4.8 }; // Vertical interval between thin limestone bedding layers in metres (doubled from 2.4m)

        // High fell plateau and summit capping
        double   plateauElevation{ 0.78 };     // Normalised altitude where summits level off into plateaus
        double   plateauSoftness{ 0.12 };      // Transition factor for plateau levelling
        double   convexity{ 1.25 };            // Fell convexity exponent

        // Incised lateral gills (drainage ravines)
        double   gullyStrength{ 0.22 };        // Stream gully / gill incision strength (reduced from 0.35)
        double   gullyFrequency{ 0.024 };      // Spatial frequency of lateral stream gullies
        double   tributaryStrength{ 0.12 };    // High-frequency tributary ravine incision strength (reduced from 0.20)
        float    creviceThreshold{ 0.16f };    // Concavity threshold for hillside crevice and gill detailing in metres

        // Surface scree and rock grain micro-roughness
        double   screeGranularity{ 0.035 };    // High-frequency rock scree / talus surface roughness

        // Cellular drumlin hillocks
        double   cellularStrength{ 0.08 };     // Cellular / drumlin hillock prominence
        double   cellularFrequency{ 0.035 };   // Cellular feature density
        double   cellularStretch{ 2.8 };       // Anisotropic elongation factor along flow axis
        double   cellularAngle{ 1.12 };        // Primary flow orientation in radians

        // Deterministic pseudo-random seed
        uint32_t seed{ 1337u };

        // Surface biome and slope palette colours
        Vec4     valleyFloorColor{ 0.44f, 0.58f, 0.20f, 1.0f };   // Lush pasture emerald green
        Vec4     riverbedColor{ 0.50f, 0.51f, 0.48f, 1.0f };      // River gravel and limestone cobbles
        Vec4     lowSlopeColor{ 0.38f, 0.54f, 0.195f, 1.0f };      // Lower pasture and bracken
        Vec4     midSlopeColor{ 0.44f, 0.52f, 0.23f, 1.0f };      // Rough moorland grass, rush pasture, and moss
        Vec4     limestoneScarColor{ 0.56f, 0.56f, 0.54f, 1.0f }; // Pale cool grey Yoredale limestone scar
        Vec4     rockColor{ 0.46f, 0.45f, 0.42f, 1.0f };          // Weathered gritstone benches
        Vec4     steepCragColor{ 0.28f, 0.29f, 0.28f, 1.0f };     // Wet rock / dark cliff faces
        Vec4     peatMoorColor{ 0.38f, 0.34f, 0.26f, 1.0f };      // Weathered upland peat hollows and dry heath
        Vec4     highPlateauColor{ 0.48f, 0.50f, 0.28f, 1.0f };   // Sunlit summit plateau (golden-straw mat-grass and fescue)
        Vec4     heatherColor{ 0.45f, 0.42f, 0.33f, 1.0f };       // Dusky heather and moorland grass blend

        // Preset configured for the full 15km x 15km Garsdale & Dentdale LiDAR study area
        // Bounded by British National Grid Easting [365000, 380000] and Northing [480000, 495000]
        [[nodiscard]] static TerrainConfig CreateDentdale15km() noexcept
        {
            TerrainConfig cfg;
            cfg.width = 15000.0;
            cfg.depth = 15000.0;
            cfg.resolutionX = 1000;
            cfg.resolutionZ = 1000;
            cfg.origin = Vec3D(372500.0, 0.0, 487500.0);
            cfg.heightScale = 620.0;
            cfg.baseFrequency = 0.00018;
            cfg.valleyFloorWidth = 1200.0;
            cfg.cyclothemStepHeight = 35.0;
            return cfg;
        }
    };
}

