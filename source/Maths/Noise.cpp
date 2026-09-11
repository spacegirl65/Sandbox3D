// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Noise.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace Sandbox3D::Maths
{
    const Noise Noise::s_defaultInstance{ 1337u };

    Noise::Noise(uint32_t seed)
    {
        SetSeed(seed);
    }

    void Noise::SetSeed(uint32_t seed)
    {
        m_seed = seed;

        // Initialize permutation table with linear progression 0..255
        for (uint32_t i = 0; i < 256; ++i)
        {
            m_perm[i] = static_cast<uint8_t>(i);
        }

        // Deterministic linear congruential shuffle
        uint32_t state = (seed != 0) ? seed : 1337u;
        auto nextRand = [&state]() -> uint32_t {
            state = state * 1664525u + 1013904223u;
            return state;
        };

        for (int i = 255; i > 0; --i)
        {
            const int j = static_cast<int>(nextRand() % static_cast<uint32_t>(i + 1));
            std::swap(m_perm[i], m_perm[j]);
        }

        // Duplicate table for overflow without modulo
        for (uint32_t i = 0; i < 256; ++i)
        {
            m_perm[256 + i] = m_perm[i];
        }
    }

    float Noise::Grad2D(uint8_t hash, float x, float y) noexcept
    {
        switch (hash & 7)
        {
            case 0: return  x + y;
            case 1: return -x + y;
            case 2: return  x - y;
            case 3: return -x - y;
            case 4: return  x * 1.41421356f;
            case 5: return -x * 1.41421356f;
            case 6: return  y * 1.41421356f;
            case 7: return -y * 1.41421356f;
            default: return 0.0f;
        }
    }

    float Noise::Grad3D(uint8_t hash, float x, float y, float z) noexcept
    {
        const int h = hash & 15;
        const float u = (h < 8) ? x : y;
        const float v = (h < 4) ? y : ((h == 12 || h == 14) ? x : z);
        return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
    }

    uint32_t Noise::HashCoords(int32_t x, int32_t y) const noexcept
    {
        const uint8_t X = static_cast<uint8_t>(x & 255);
        const uint8_t Y = static_cast<uint8_t>(y & 255);
        return static_cast<uint32_t>(m_perm[m_perm[X] + Y]);
    }

    uint32_t Noise::HashCoords(int32_t x, int32_t y, int32_t z) const noexcept
    {
        const uint8_t X = static_cast<uint8_t>(x & 255);
        const uint8_t Y = static_cast<uint8_t>(y & 255);
        const uint8_t Z = static_cast<uint8_t>(z & 255);
        return static_cast<uint32_t>(m_perm[m_perm[m_perm[X] + Y] + Z]);
    }

    // --- 1. Ken Perlin's Improved Noise ---

    float Noise::Perlin(float x) const noexcept
    {
        const int32_t xi = static_cast<int32_t>(std::floor(x));
        const float xf = x - static_cast<float>(xi);
        const int32_t X = xi & 255;
        const float u = Fade(xf);

        const float g0 = Grad1D(m_perm[X], xf);
        const float g1 = Grad1D(m_perm[X + 1], xf - 1.0f);

        return Lerp(g0, g1, u);
    }

    float Noise::Perlin(float x, float y) const noexcept
    {
        const int32_t xi = static_cast<int32_t>(std::floor(x));
        const int32_t yi = static_cast<int32_t>(std::floor(y));
        const float xf = x - static_cast<float>(xi);
        const float yf = y - static_cast<float>(yi);
        const int32_t X = xi & 255;
        const int32_t Y = yi & 255;

        const float u = Fade(xf);
        const float v = Fade(yf);

        const int32_t A = m_perm[X] + Y;
        const int32_t B = m_perm[X + 1] + Y;

        const float x1 = Lerp(Grad2D(m_perm[A], xf, yf), Grad2D(m_perm[B], xf - 1.0f, yf), u);
        const float x2 = Lerp(Grad2D(m_perm[A + 1], xf, yf - 1.0f), Grad2D(m_perm[B + 1], xf - 1.0f, yf - 1.0f), u);

        return Lerp(x1, x2, v);
    }

    float Noise::Perlin(float x, float y, float z) const noexcept
    {
        const int32_t xi = static_cast<int32_t>(std::floor(x));
        const int32_t yi = static_cast<int32_t>(std::floor(y));
        const int32_t zi = static_cast<int32_t>(std::floor(z));

        const float xf = x - static_cast<float>(xi);
        const float yf = y - static_cast<float>(yi);
        const float zf = z - static_cast<float>(zi);

        const int32_t X = xi & 255;
        const int32_t Y = yi & 255;
        const int32_t Z = zi & 255;

        const float u = Fade(xf);
        const float v = Fade(yf);
        const float w = Fade(zf);

        const int32_t A  = m_perm[X] + Y;
        const int32_t AA = m_perm[A] + Z;
        const int32_t AB = m_perm[A + 1] + Z;
        const int32_t B  = m_perm[X + 1] + Y;
        const int32_t BA = m_perm[B] + Z;
        const int32_t BB = m_perm[B + 1] + Z;

        const float x11 = Grad3D(m_perm[AA], xf, yf, zf);
        const float x12 = Grad3D(m_perm[BA], xf - 1.0f, yf, zf);
        const float x21 = Grad3D(m_perm[AB], xf, yf - 1.0f, zf);
        const float x22 = Grad3D(m_perm[BB], xf - 1.0f, yf - 1.0f, zf);
        const float y1  = Lerp(x11, x12, u);
        const float y2  = Lerp(x21, x22, u);
        const float z1  = Lerp(y1, y2, v);

        const float x31 = Grad3D(m_perm[AA + 1], xf, yf, zf - 1.0f);
        const float x32 = Grad3D(m_perm[BA + 1], xf - 1.0f, yf, zf - 1.0f);
        const float x41 = Grad3D(m_perm[AB + 1], xf, yf - 1.0f, zf - 1.0f);
        const float x42 = Grad3D(m_perm[BB + 1], xf - 1.0f, yf - 1.0f, zf - 1.0f);
        const float y3  = Lerp(x31, x32, u);
        const float y4  = Lerp(x41, x42, u);
        const float z2  = Lerp(y3, y4, v);

        return Lerp(z1, z2, w);
    }

    // --- 2. Simplex Noise ---

    float Noise::Simplex(float x, float y) const noexcept
    {
        // Skewing and unskewing factors for 2D simplex
        constexpr float F2 = 0.36602540378f; // 0.5 * (sqrt(3.0) - 1.0)
        constexpr float G2 = 0.21132486540f; // (3.0 - sqrt(3.0)) / 6.0

        const float s = (x + y) * F2;
        const int32_t i = static_cast<int32_t>(std::floor(x + s));
        const int32_t j = static_cast<int32_t>(std::floor(y + s));

        const float t = static_cast<float>(i + j) * G2;
        const float X0 = static_cast<float>(i) - t;
        const float Y0 = static_cast<float>(j) - t;
        const float x0 = x - X0;
        const float y0 = y - Y0;

        // Determine which simplex triangle we are in
        int32_t i1 = 0;
        int32_t j1 = 0;
        if (x0 > y0)
        {
            i1 = 1;
            j1 = 0;
        }
        else
        {
            i1 = 0;
            j1 = 1;
        }

        const float x1 = x0 - static_cast<float>(i1) + G2;
        const float y1 = y0 - static_cast<float>(j1) + G2;
        const float x2 = x0 - 1.0f + 2.0f * G2;
        const float y2 = y0 - 1.0f + 2.0f * G2;

        const int32_t ii = i & 255;
        const int32_t jj = j & 255;

        // Calculate contribution from the three corners
        float n0 = 0.0f;
        float n1 = 0.0f;
        float n2 = 0.0f;

        float t0 = 0.5f - x0 * x0 - y0 * y0;
        if (t0 > 0.0f)
        {
            t0 *= t0;
            n0 = t0 * t0 * Grad2D(m_perm[ii + m_perm[jj]], x0, y0);
        }

        float t1 = 0.5f - x1 * x1 - y1 * y1;
        if (t1 > 0.0f)
        {
            t1 *= t1;
            n1 = t1 * t1 * Grad2D(m_perm[ii + i1 + m_perm[jj + j1]], x1, y1);
        }

        float t2 = 0.5f - x2 * x2 - y2 * y2;
        if (t2 > 0.0f)
        {
            t2 *= t2;
            n2 = t2 * t2 * Grad2D(m_perm[ii + 1 + m_perm[jj + 1]], x2, y2);
        }

        // Scale result to [-1.0, 1.0]
        return 70.0f * (n0 + n1 + n2);
    }

    float Noise::Simplex(float x, float y, float z) const noexcept
    {
        // Skewing and unskewing factors for 3D simplex
        constexpr float F3 = 1.0f / 3.0f;
        constexpr float G3 = 1.0f / 6.0f;

        const float s = (x + y + z) * F3;
        const int32_t i = static_cast<int32_t>(std::floor(x + s));
        const int32_t j = static_cast<int32_t>(std::floor(y + s));
        const int32_t k = static_cast<int32_t>(std::floor(z + s));

        const float t = static_cast<float>(i + j + k) * G3;
        const float X0 = static_cast<float>(i) - t;
        const float Y0 = static_cast<float>(j) - t;
        const float Z0 = static_cast<float>(k) - t;
        const float x0 = x - X0;
        const float y0 = y - Y0;
        const float z0 = z - Z0;

        int32_t i1, j1, k1;
        int32_t i2, j2, k2;

        if (x0 >= y0)
        {
            if (y0 >= z0)
            {
                i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 1; k2 = 0;
            }
            else if (x0 >= z0)
            {
                i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 0; k2 = 1;
            }
            else
            {
                i1 = 0; j1 = 0; k1 = 1; i2 = 1; j2 = 0; k2 = 1;
            }
        }
        else
        {
            if (y0 < z0)
            {
                i1 = 0; j1 = 0; k1 = 1; i2 = 0; j2 = 1; k2 = 1;
            }
            else if (x0 < z0)
            {
                i1 = 0; j1 = 1; k1 = 0; i2 = 0; j2 = 1; k2 = 1;
            }
            else
            {
                i1 = 0; j1 = 1; k1 = 0; i2 = 1; j2 = 1; k2 = 0;
            }
        }

        const float x1 = x0 - static_cast<float>(i1) + G3;
        const float y1 = y0 - static_cast<float>(j1) + G3;
        const float z1 = z0 - static_cast<float>(k1) + G3;

        const float x2 = x0 - static_cast<float>(i2) + 2.0f * G3;
        const float y2 = y0 - static_cast<float>(j2) + 2.0f * G3;
        const float z2 = z0 - static_cast<float>(k2) + 2.0f * G3;

        const float x3 = x0 - 1.0f + 3.0f * G3;
        const float y3 = y0 - 1.0f + 3.0f * G3;
        const float z3 = z0 - 1.0f + 3.0f * G3;

        const int32_t ii = i & 255;
        const int32_t jj = j & 255;
        const int32_t kk = k & 255;

        float n0 = 0.0f;
        float n1 = 0.0f;
        float n2 = 0.0f;
        float n3 = 0.0f;

        float t0 = 0.6f - x0 * x0 - y0 * y0 - z0 * z0;
        if (t0 > 0.0f)
        {
            t0 *= t0;
            n0 = t0 * t0 * Grad3D(m_perm[ii + m_perm[jj + m_perm[kk]]], x0, y0, z0);
        }

        float t1 = 0.6f - x1 * x1 - y1 * y1 - z1 * z1;
        if (t1 > 0.0f)
        {
            t1 *= t1;
            n1 = t1 * t1 * Grad3D(m_perm[ii + i1 + m_perm[jj + j1 + m_perm[kk + k1]]], x1, y1, z1);
        }

        float t2 = 0.6f - x2 * x2 - y2 * y2 - z2 * z2;
        if (t2 > 0.0f)
        {
            t2 *= t2;
            n2 = t2 * t2 * Grad3D(m_perm[ii + i2 + m_perm[jj + j2 + m_perm[kk + k2]]], x2, y2, z2);
        }

        float t3 = 0.6f - x3 * x3 - y3 * y3 - z3 * z3;
        if (t3 > 0.0f)
        {
            t3 *= t3;
            n3 = t3 * t3 * Grad3D(m_perm[ii + 1 + m_perm[jj + 1 + m_perm[kk + 1]]], x3, y3, z3);
        }

        return 32.0f * (n0 + n1 + n2 + n3);
    }

    // --- 3. Worley / Cellular Noise ---

    Vec2 Noise::WorleyF1F2(float x, float y) const noexcept
    {
        const int32_t xi = static_cast<int32_t>(std::floor(x));
        const int32_t yi = static_cast<int32_t>(std::floor(y));

        float f1 = 1e9f;
        float f2 = 1e9f;

        // Examine 3x3 surrounding neighbourhood cells
        for (int32_t dy = -1; dy <= 1; ++dy)
        {
            for (int32_t dx = -1; dx <= 1; ++dx)
            {
                const int32_t cx = xi + dx;
                const int32_t cy = yi + dy;

                const uint32_t hX = HashCoords(cx, cy);
                const uint32_t hY = HashCoords(cx + 41, cy + 97);

                const float px = static_cast<float>(cx) + HashToFloat(hX);
                const float py = static_cast<float>(cy) + HashToFloat(hY);

                const float diffX = px - x;
                const float diffY = py - y;
                const float dist = std::sqrt(diffX * diffX + diffY * diffY);

                if (dist < f1)
                {
                    f2 = f1;
                    f1 = dist;
                }
                else if (dist < f2)
                {
                    f2 = dist;
                }
            }
        }

        return Vec2(f1, f2);
    }

    float Noise::Worley(float x, float y) const noexcept
    {
        return WorleyF1F2(x, y).x;
    }

    Vec2 Noise::WorleyF1F2(float x, float y, float z) const noexcept
    {
        const int32_t xi = static_cast<int32_t>(std::floor(x));
        const int32_t yi = static_cast<int32_t>(std::floor(y));
        const int32_t zi = static_cast<int32_t>(std::floor(z));

        float f1 = 1e9f;
        float f2 = 1e9f;

        // Examine 3x3x3 surrounding neighbourhood cells
        for (int32_t dz = -1; dz <= 1; ++dz)
        {
            for (int32_t dy = -1; dy <= 1; ++dy)
            {
                for (int32_t dx = -1; dx <= 1; ++dx)
                {
                    const int32_t cx = xi + dx;
                    const int32_t cy = yi + dy;
                    const int32_t cz = zi + dz;

                    const uint32_t hX = HashCoords(cx, cy, cz);
                    const uint32_t hY = HashCoords(cx + 37, cy + 73, cz + 109);
                    const uint32_t hZ = HashCoords(cx + 61, cy + 127, cz + 179);

                    const float px = static_cast<float>(cx) + HashToFloat(hX);
                    const float py = static_cast<float>(cy) + HashToFloat(hY);
                    const float pz = static_cast<float>(cz) + HashToFloat(hZ);

                    const float diffX = px - x;
                    const float diffY = py - y;
                    const float diffZ = pz - z;
                    const float dist = std::sqrt(diffX * diffX + diffY * diffY + diffZ * diffZ);

                    if (dist < f1)
                    {
                        f2 = f1;
                        f1 = dist;
                    }
                    else if (dist < f2)
                    {
                        f2 = dist;
                    }
                }
            }
        }

        return Vec2(f1, f2);
    }

    float Noise::Worley(float x, float y, float z) const noexcept
    {
        return WorleyF1F2(x, y, z).x;
    }

    // --- 4. Value Noise ---

    float Noise::Value(float x, float y) const noexcept
    {
        const int32_t xi = static_cast<int32_t>(std::floor(x));
        const int32_t yi = static_cast<int32_t>(std::floor(y));
        const float xf = x - static_cast<float>(xi);
        const float yf = y - static_cast<float>(yi);

        const float u = Fade(xf);
        const float v = Fade(yf);

        const float v00 = HashToFloat(HashCoords(xi, yi)) * 2.0f - 1.0f;
        const float v10 = HashToFloat(HashCoords(xi + 1, yi)) * 2.0f - 1.0f;
        const float v01 = HashToFloat(HashCoords(xi, yi + 1)) * 2.0f - 1.0f;
        const float v11 = HashToFloat(HashCoords(xi + 1, yi + 1)) * 2.0f - 1.0f;

        const float x1 = Lerp(v00, v10, u);
        const float x2 = Lerp(v01, v11, u);

        return Lerp(x1, x2, v);
    }

    float Noise::Value(float x, float y, float z) const noexcept
    {
        const int32_t xi = static_cast<int32_t>(std::floor(x));
        const int32_t yi = static_cast<int32_t>(std::floor(y));
        const int32_t zi = static_cast<int32_t>(std::floor(z));

        const float xf = x - static_cast<float>(xi);
        const float yf = y - static_cast<float>(yi);
        const float zf = z - static_cast<float>(zi);

        const float u = Fade(xf);
        const float v = Fade(yf);
        const float w = Fade(zf);

        const float v000 = HashToFloat(HashCoords(xi,     yi,     zi    )) * 2.0f - 1.0f;
        const float v100 = HashToFloat(HashCoords(xi + 1, yi,     zi    )) * 2.0f - 1.0f;
        const float v010 = HashToFloat(HashCoords(xi,     yi + 1, zi    )) * 2.0f - 1.0f;
        const float v110 = HashToFloat(HashCoords(xi + 1, yi + 1, zi    )) * 2.0f - 1.0f;
        const float v001 = HashToFloat(HashCoords(xi,     yi,     zi + 1)) * 2.0f - 1.0f;
        const float v101 = HashToFloat(HashCoords(xi + 1, yi,     zi + 1)) * 2.0f - 1.0f;
        const float v011 = HashToFloat(HashCoords(xi,     yi + 1, zi + 1)) * 2.0f - 1.0f;
        const float v111 = HashToFloat(HashCoords(xi + 1, yi + 1, zi + 1)) * 2.0f - 1.0f;

        const float x1 = Lerp(v000, v100, u);
        const float x2 = Lerp(v010, v110, u);
        const float y1 = Lerp(x1, x2, v);

        const float x3 = Lerp(v001, v101, u);
        const float x4 = Lerp(v011, v111, u);
        const float y2 = Lerp(x3, x4, v);

        return Lerp(y1, y2, w);
    }

    // --- 5. Fractal & Spectral Noise Synthesizers ---

    float Noise::FBM(float x, float y, int octaves, float persistence, float lacunarity) const noexcept
    {
        float value     = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float maxAmp    = 0.0f;

        for (int i = 0; i < octaves; ++i)
        {
            value += Perlin(x * frequency, y * frequency) * amplitude;
            maxAmp += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }

        return (maxAmp > 0.0f) ? (value / maxAmp) : 0.0f;
    }

    float Noise::FBM(float x, float y, float z, int octaves, float persistence, float lacunarity) const noexcept
    {
        float value     = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float maxAmp    = 0.0f;

        for (int i = 0; i < octaves; ++i)
        {
            value += Perlin(x * frequency, y * frequency, z * frequency) * amplitude;
            maxAmp += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }

        return (maxAmp > 0.0f) ? (value / maxAmp) : 0.0f;
    }

    float Noise::Turbulence(float x, float y, int octaves, float persistence, float lacunarity) const noexcept
    {
        float value     = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float maxAmp    = 0.0f;

        for (int i = 0; i < octaves; ++i)
        {
            value += std::abs(Perlin(x * frequency, y * frequency)) * amplitude;
            maxAmp += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }

        return (maxAmp > 0.0f) ? (value / maxAmp) : 0.0f;
    }

    float Noise::Turbulence(float x, float y, float z, int octaves, float persistence, float lacunarity) const noexcept
    {
        float value     = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float maxAmp    = 0.0f;

        for (int i = 0; i < octaves; ++i)
        {
            value += std::abs(Perlin(x * frequency, y * frequency, z * frequency)) * amplitude;
            maxAmp += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }

        return (maxAmp > 0.0f) ? (value / maxAmp) : 0.0f;
    }

    float Noise::RidgedMulti(float x, float y, int octaves, float persistence, float lacunarity) const noexcept
    {
        float value     = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float weight    = 1.0f;
        float maxAmp    = 0.0f;

        for (int i = 0; i < octaves; ++i)
        {
            float signal = 1.0f - std::abs(Perlin(x * frequency, y * frequency));
            signal *= signal;
            signal *= weight;
            weight = Clamp(signal * 2.0f, 0.0f, 1.0f);

            value += signal * amplitude;
            maxAmp += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }

        return (maxAmp > 0.0f) ? (value / maxAmp) : 0.0f;
    }

    float Noise::RidgedMulti(float x, float y, float z, int octaves, float persistence, float lacunarity) const noexcept
    {
        float value     = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float weight    = 1.0f;
        float maxAmp    = 0.0f;

        for (int i = 0; i < octaves; ++i)
        {
            float signal = 1.0f - std::abs(Perlin(x * frequency, y * frequency, z * frequency));
            signal *= signal;
            signal *= weight;
            weight = Clamp(signal * 2.0f, 0.0f, 1.0f);

            value += signal * amplitude;
            maxAmp += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }

        return (maxAmp > 0.0f) ? (value / maxAmp) : 0.0f;
    }
}

