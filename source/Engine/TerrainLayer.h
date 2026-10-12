// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Renderer/Texture.h"

#include <memory>
#include <string>

namespace Sandbox3D::Engine
{
    // Encapsulates a single landscape surface biome layer and its associated PBR texture maps
    struct TerrainLayer
    {
        std::string name;
        std::string assetId;        // e.g. "oilpt20"
        std::string albedoFile;
        std::string normalFile;
        std::string roughnessFile;
        std::string aoFile;
        std::string dispFile;       // POM displacement heightmap
        float       uvScale{ 1.0f };

        // Loaded GPU texture resources (retained to ensure GPU lifetime)
        std::shared_ptr<Renderer::Texture> albedo;
        std::shared_ptr<Renderer::Texture> normal;
        std::shared_ptr<Renderer::Texture> roughness;
        std::shared_ptr<Renderer::Texture> ao;
        std::shared_ptr<Renderer::Texture> displacement;
    };
}

