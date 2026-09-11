// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "MathsCommon.h"
#include "Vec2.h"
#include "Vec3.h"

#include <array>
#include <cstdint>

namespace Sandbox3D::Maths
{
    // Procedural noise generator supporting Perlin, Simplex, Worley/Cellular, and Fractal synthesis
    class Noise final
    {
    public:
        // Initialise noise generator with deterministic seed
        explicit Noise(uint32_t seed = 1337u);
        ~Noise() = default;

        Noise(const Noise&) = default;
        Noise& operator=(const Noise&) = default;
        Noise(Noise&&) noexcept = default;
        Noise& operator=(Noise&&) noexcept = default;

        // Re-seed the permutation table
        void SetSeed(uint32_t seed);
        [[nodiscard]] uint32_t GetSeed() const noexcept { return m_seed; }

        // --- 1. Ken Perlin's Improved Noise (range: [-1.0, 1.0]) ---
        [[nodiscard]] float Perlin(float x) const noexcept;
        [[nodiscard]] float Perlin(float x, float y) const noexcept;
        [[nodiscard]] float Perlin(const Vec2& p) const noexcept { return Perlin(p.x, p.y); }
        [[nodiscard]] float Perlin(float x, float y, float z) const noexcept;
        [[nodiscard]] float Perlin(const Vec3& p) const noexcept { return Perlin(p.x, p.y, p.z); }

        // Perlin noise mapped to [0.0, 1.0]
        [[nodiscard]] float Perlin01(float x) const noexcept { return Perlin(x) * 0.5f + 0.5f; }
        [[nodiscard]] float Perlin01(float x, float y) const noexcept { return Perlin(x, y) * 0.5f + 0.5f; }
        [[nodiscard]] float Perlin01(const Vec2& p) const noexcept { return Perlin(p) * 0.5f + 0.5f; }
        [[nodiscard]] float Perlin01(float x, float y, float z) const noexcept { return Perlin(x, y, z) * 0.5f + 0.5f; }
        [[nodiscard]] float Perlin01(const Vec3& p) const noexcept { return Perlin(p) * 0.5f + 0.5f; }

        // --- 2. Simplex Noise (range: [-1.0, 1.0]) ---
        [[nodiscard]] float Simplex(float x, float y) const noexcept;
        [[nodiscard]] float Simplex(const Vec2& p) const noexcept { return Simplex(p.x, p.y); }
        [[nodiscard]] float Simplex(float x, float y, float z) const noexcept;
        [[nodiscard]] float Simplex(const Vec3& p) const noexcept { return Simplex(p.x, p.y, p.z); }

        // Simplex noise mapped to [0.0, 1.0]
        [[nodiscard]] float Simplex01(float x, float y) const noexcept { return Simplex(x, y) * 0.5f + 0.5f; }
        [[nodiscard]] float Simplex01(const Vec2& p) const noexcept { return Simplex(p) * 0.5f + 0.5f; }
        [[nodiscard]] float Simplex01(float x, float y, float z) const noexcept { return Simplex(x, y, z) * 0.5f + 0.5f; }
        [[nodiscard]] float Simplex01(const Vec3& p) const noexcept { return Simplex(p) * 0.5f + 0.5f; }

        // --- 3. Worley / Cellular Noise (Voronoi distance metric) ---
        // Returns F1 distance (distance to closest feature point, roughly [0.0, 1.0])
        [[nodiscard]] float Worley(float x, float y) const noexcept;
        [[nodiscard]] float Worley(const Vec2& p) const noexcept { return Worley(p.x, p.y); }
        [[nodiscard]] float Worley(float x, float y, float z) const noexcept;
        [[nodiscard]] float Worley(const Vec3& p) const noexcept { return Worley(p.x, p.y, p.z); }

        // Returns { F1, F2 } where F1 is distance to closest and F2 is distance to 2nd closest feature point
        // Useful for cellular/stone borders (e.g. F2 - F1)
        [[nodiscard]] Vec2 WorleyF1F2(float x, float y) const noexcept;
        [[nodiscard]] Vec2 WorleyF1F2(const Vec2& p) const noexcept { return WorleyF1F2(p.x, p.y); }
        [[nodiscard]] Vec2 WorleyF1F2(float x, float y, float z) const noexcept;
        [[nodiscard]] Vec2 WorleyF1F2(const Vec3& p) const noexcept { return WorleyF1F2(p.x, p.y, p.z); }

        // --- 4. Value Noise (range: [-1.0, 1.0]) ---
        [[nodiscard]] float Value(float x, float y) const noexcept;
        [[nodiscard]] float Value(const Vec2& p) const noexcept { return Value(p.x, p.y); }
        [[nodiscard]] float Value(float x, float y, float z) const noexcept;
        [[nodiscard]] float Value(const Vec3& p) const noexcept { return Value(p.x, p.y, p.z); }

        // --- 5. Fractal & Spectral Noise Synthesizers ---
        // Fractal Brownian Motion (fBm) summing multiple octaves (range: [-1.0, 1.0])
        [[nodiscard]] float FBM(float x, float y, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) const noexcept;
        [[nodiscard]] float FBM(const Vec2& p, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) const noexcept
        {
            return FBM(p.x, p.y, octaves, persistence, lacunarity);
        }
        [[nodiscard]] float FBM(float x, float y, float z, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) const noexcept;
        [[nodiscard]] float FBM(const Vec3& p, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) const noexcept
        {
            return FBM(p.x, p.y, p.z, octaves, persistence, lacunarity);
        }

        // fBm mapped to [0.0, 1.0]
        [[nodiscard]] float FBM01(const Vec2& p, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) const noexcept
        {
            return FBM(p, octaves, persistence, lacunarity) * 0.5f + 0.5f;
        }
        [[nodiscard]] float FBM01(const Vec3& p, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) const noexcept
        {
            return FBM(p, octaves, persistence, lacunarity) * 0.5f + 0.5f;
        }

        // Turbulence summing absolute octave values (range: [0.0, 1.0], ideal for clouds, marble, fire)
        [[nodiscard]] float Turbulence(float x, float y, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) const noexcept;
        [[nodiscard]] float Turbulence(const Vec2& p, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) const noexcept
        {
            return Turbulence(p.x, p.y, octaves, persistence, lacunarity);
        }
        [[nodiscard]] float Turbulence(float x, float y, float z, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) const noexcept;
        [[nodiscard]] float Turbulence(const Vec3& p, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) const noexcept
        {
            return Turbulence(p.x, p.y, p.z, octaves, persistence, lacunarity);
        }

        // Ridged Multifractal (range: [0.0, 1.0], ideal for sharp mountain ridges, canyons)
        [[nodiscard]] float RidgedMulti(float x, float y, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) const noexcept;
        [[nodiscard]] float RidgedMulti(const Vec2& p, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) const noexcept
        {
            return RidgedMulti(p.x, p.y, octaves, persistence, lacunarity);
        }
        [[nodiscard]] float RidgedMulti(float x, float y, float z, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) const noexcept;
        [[nodiscard]] float RidgedMulti(const Vec3& p, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) const noexcept
        {
            return RidgedMulti(p.x, p.y, p.z, octaves, persistence, lacunarity);
        }

        // --- 6. Static Convenience API (zero-allocation default instance) ---
        [[nodiscard]] static float GetPerlin(float x) noexcept { return s_defaultInstance.Perlin(x); }
        [[nodiscard]] static float GetPerlin(float x, float y) noexcept { return s_defaultInstance.Perlin(x, y); }
        [[nodiscard]] static float GetPerlin(const Vec2& p) noexcept { return s_defaultInstance.Perlin(p); }
        [[nodiscard]] static float GetPerlin(float x, float y, float z) noexcept { return s_defaultInstance.Perlin(x, y, z); }
        [[nodiscard]] static float GetPerlin(const Vec3& p) noexcept { return s_defaultInstance.Perlin(p); }

        [[nodiscard]] static float GetPerlin01(float x) noexcept { return s_defaultInstance.Perlin01(x); }
        [[nodiscard]] static float GetPerlin01(float x, float y) noexcept { return s_defaultInstance.Perlin01(x, y); }
        [[nodiscard]] static float GetPerlin01(const Vec2& p) noexcept { return s_defaultInstance.Perlin01(p); }
        [[nodiscard]] static float GetPerlin01(float x, float y, float z) noexcept { return s_defaultInstance.Perlin01(x, y, z); }
        [[nodiscard]] static float GetPerlin01(const Vec3& p) noexcept { return s_defaultInstance.Perlin01(p); }

        [[nodiscard]] static float GetSimplex(float x, float y) noexcept { return s_defaultInstance.Simplex(x, y); }
        [[nodiscard]] static float GetSimplex(const Vec2& p) noexcept { return s_defaultInstance.Simplex(p); }
        [[nodiscard]] static float GetSimplex(float x, float y, float z) noexcept { return s_defaultInstance.Simplex(x, y, z); }
        [[nodiscard]] static float GetSimplex(const Vec3& p) noexcept { return s_defaultInstance.Simplex(p); }

        [[nodiscard]] static float GetSimplex01(float x, float y) noexcept { return s_defaultInstance.Simplex01(x, y); }
        [[nodiscard]] static float GetSimplex01(const Vec2& p) noexcept { return s_defaultInstance.Simplex01(p); }
        [[nodiscard]] static float GetSimplex01(float x, float y, float z) noexcept { return s_defaultInstance.Simplex01(x, y, z); }
        [[nodiscard]] static float GetSimplex01(const Vec3& p) noexcept { return s_defaultInstance.Simplex01(p); }

        [[nodiscard]] static float GetWorley(float x, float y) noexcept { return s_defaultInstance.Worley(x, y); }
        [[nodiscard]] static float GetWorley(const Vec2& p) noexcept { return s_defaultInstance.Worley(p); }
        [[nodiscard]] static float GetWorley(float x, float y, float z) noexcept { return s_defaultInstance.Worley(x, y, z); }
        [[nodiscard]] static float GetWorley(const Vec3& p) noexcept { return s_defaultInstance.Worley(p); }

        [[nodiscard]] static Vec2 GetWorleyF1F2(float x, float y) noexcept { return s_defaultInstance.WorleyF1F2(x, y); }
        [[nodiscard]] static Vec2 GetWorleyF1F2(const Vec2& p) noexcept { return s_defaultInstance.WorleyF1F2(p); }
        [[nodiscard]] static Vec2 GetWorleyF1F2(float x, float y, float z) noexcept { return s_defaultInstance.WorleyF1F2(x, y, z); }
        [[nodiscard]] static Vec2 GetWorleyF1F2(const Vec3& p) noexcept { return s_defaultInstance.WorleyF1F2(p); }

        [[nodiscard]] static float GetValue(float x, float y) noexcept { return s_defaultInstance.Value(x, y); }
        [[nodiscard]] static float GetValue(const Vec2& p) noexcept { return s_defaultInstance.Value(p); }
        [[nodiscard]] static float GetValue(float x, float y, float z) noexcept { return s_defaultInstance.Value(x, y, z); }
        [[nodiscard]] static float GetValue(const Vec3& p) noexcept { return s_defaultInstance.Value(p); }

        [[nodiscard]] static float GetFBM(const Vec2& p, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) noexcept
        {
            return s_defaultInstance.FBM(p, octaves, persistence, lacunarity);
        }
        [[nodiscard]] static float GetFBM(const Vec3& p, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) noexcept
        {
            return s_defaultInstance.FBM(p, octaves, persistence, lacunarity);
        }

        [[nodiscard]] static float GetFBM01(const Vec2& p, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) noexcept
        {
            return s_defaultInstance.FBM01(p, octaves, persistence, lacunarity);
        }
        [[nodiscard]] static float GetFBM01(const Vec3& p, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) noexcept
        {
            return s_defaultInstance.FBM01(p, octaves, persistence, lacunarity);
        }

        [[nodiscard]] static float GetTurbulence(const Vec2& p, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) noexcept
        {
            return s_defaultInstance.Turbulence(p, octaves, persistence, lacunarity);
        }
        [[nodiscard]] static float GetTurbulence(const Vec3& p, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) noexcept
        {
            return s_defaultInstance.Turbulence(p, octaves, persistence, lacunarity);
        }

        [[nodiscard]] static float GetRidgedMulti(const Vec2& p, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) noexcept
        {
            return s_defaultInstance.RidgedMulti(p, octaves, persistence, lacunarity);
        }
        [[nodiscard]] static float GetRidgedMulti(const Vec3& p, int octaves = 6, float persistence = 0.5f, float lacunarity = 2.0f) noexcept
        {
            return s_defaultInstance.RidgedMulti(p, octaves, persistence, lacunarity);
        }

    private:
        [[nodiscard]] static float Fade(float t) noexcept
        {
            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        }

        [[nodiscard]] static float Grad1D(uint8_t hash, float x) noexcept
        {
            return (hash & 1) ? -x : x;
        }

        [[nodiscard]] static float Grad2D(uint8_t hash, float x, float y) noexcept;
        [[nodiscard]] static float Grad3D(uint8_t hash, float x, float y, float z) noexcept;

        [[nodiscard]] static float HashToFloat(uint32_t hash) noexcept
        {
            // Maps integer hash to [0.0, 1.0)
            return static_cast<float>(hash & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
        }

        [[nodiscard]] uint32_t HashCoords(int32_t x, int32_t y) const noexcept;
        [[nodiscard]] uint32_t HashCoords(int32_t x, int32_t y, int32_t z) const noexcept;

    private:
        std::array<uint8_t, 512> m_perm{};
        uint32_t                 m_seed{ 1337u };

        static const Noise       s_defaultInstance;
    };
}

