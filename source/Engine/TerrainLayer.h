// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Renderer/Texture.h"

#include <memory>
#include <string>

namespace Sandbox3D::Engine
{
    // Encapsulates a landscape surface biome layer using composite BC5/BC7 texture maps
    struct TerrainLayer
    {
        std::string name;
        std::string assetId;            // e.g. "oilpt20"
        std::string albedoHeightFile;   // BC7 sRGB (RGB = Albedo, A = Height/POM Displacement)
        std::string normalFile;         // BC5 UNORM (RG = Normal X/Y, Z reconstructed in shader)
        std::string ormFile;            // BC7 Linear (R = AO, G = Roughness, B = Metallic/Cavity/0)
        float       uvScale{ 0.50f };

        // Loaded GPU texture resources (retained to ensure GPU lifetime)
        std::shared_ptr<Renderer::Texture> albedoHeight;
        std::shared_ptr<Renderer::Texture> normal;
        std::shared_ptr<Renderer::Texture> orm;
    };
}

