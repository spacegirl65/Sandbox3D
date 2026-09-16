// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "TerrainConfig.h"
#include "Maths/Noise.h"
#include "Maths/Maths.h"

namespace Sandbox3D::Terrain
{
    using Maths::Noise;
    using Maths::Vec3;
    using Maths::Vec3D;
    using Maths::Vec4;

    // Generic, re-usable procedural terrain synthesiser evaluating continuous 64-bit coordinates
    class TerrainGenerator final
    {
    public:
        explicit TerrainGenerator(const TerrainConfig& config = TerrainConfig{});
        ~TerrainGenerator() = default;

        TerrainGenerator(const TerrainGenerator&) = default;
        TerrainGenerator& operator=(const TerrainGenerator&) = default;
        TerrainGenerator(TerrainGenerator&&) noexcept = default;
        TerrainGenerator& operator=(TerrainGenerator&&) noexcept = default;

        void SetConfig(const TerrainConfig& config);
        [[nodiscard]] const TerrainConfig& GetConfig() const noexcept { return m_config; }

        // Continuous elevation and finite-difference surface normal generation
        [[nodiscard]] double GenerateHeight(double x, double z) const noexcept;
        [[nodiscard]] Vec3 CalculateNormal(double x, double z, double sampleStep = 0.5) const noexcept;

        // Evaluates surface biome colour based on altitude, slope gradient, and curvature
        [[nodiscard]] Vec4 EvaluateColor(double x, double y, double z, const Vec3& normal) const noexcept;

    private:
        [[nodiscard]] double CalculateValleyCenterline(double u) const noexcept;
        [[nodiscard]] double CalculateRiverOffset(double u) const noexcept;

    private:
        TerrainConfig m_config;
        Noise         m_noise;
    };
}

