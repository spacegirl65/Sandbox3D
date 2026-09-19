// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Maths/Maths.h"

#include <cmath>
#include <cstdint>
#include <functional>

namespace Sandbox3D::Engine
{
    using Maths::Vec3D;
    using Maths::BoundingBoxD;

    // Discrete 3D cubic cell coordinate within a multi-scale spatial hierarchy
    struct CellCoord
    {
        int64_t  x{ 0 };
        int64_t  y{ 0 };
        int64_t  z{ 0 };
        uint32_t level{ 0 }; // 0 = finest base cell size, incrementing powers of two

        constexpr CellCoord() noexcept = default;
        constexpr CellCoord(int64_t inX, int64_t inY, int64_t inZ, uint32_t inLevel = 0) noexcept
            : x(inX)
            , y(inY)
            , z(inZ)
            , level(inLevel)
        {
        }

        // Discrete coordinate equality
        [[nodiscard]] constexpr bool operator==(const CellCoord& rhs) const noexcept
        {
            return x == rhs.x && y == rhs.y && z == rhs.z && level == rhs.level;
        }

        [[nodiscard]] constexpr bool operator!=(const CellCoord& rhs) const noexcept
        {
            return !(*this == rhs);
        }

        // Parent cell calculation in O(1) bit-shift
        [[nodiscard]] constexpr CellCoord Parent() const noexcept
        {
            return CellCoord(x >> 1, y >> 1, z >> 1, level + 1);
        }

        // Child octant calculation (octantIndex in [0, 7]) in O(1) bit-shift
        [[nodiscard]] constexpr CellCoord Child(uint8_t octantIndex) const noexcept
        {
            const int64_t dx = octantIndex & 1;
            const int64_t dy = (octantIndex >> 1) & 1;
            const int64_t dz = (octantIndex >> 2) & 1;
            return CellCoord(
                (x << 1) + dx,
                (y << 1) + dy,
                (z << 1) + dz,
                level > 0 ? level - 1 : 0
            );
        }

        // Evaluates cubic cell dimension at this hierarchy level
        [[nodiscard]] double GetCellSize(double baseCellSize) const noexcept
        {
            const double multiplier = std::ldexp(1.0, static_cast<int>(level));
            return baseCellSize * multiplier;
        }

        // Evaluates cubic bounding box in 64-bit world coordinates
        [[nodiscard]] BoundingBoxD GetBounds(double baseCellSize) const noexcept
        {
            const double size = GetCellSize(baseCellSize);
            const Vec3D min(
                static_cast<double>(x) * size,
                static_cast<double>(y) * size,
                static_cast<double>(z) * size
            );
            const Vec3D max = min + Vec3D(size, size, size);
            return BoundingBoxD(min, max);
        }

        // Evaluates center point of cubic cell in 64-bit world coordinates
        [[nodiscard]] Vec3D GetCenter(double baseCellSize) const noexcept
        {
            const double size = GetCellSize(baseCellSize);
            const double halfSize = size * 0.5;
            return Vec3D(
                static_cast<double>(x) * size + halfSize,
                static_cast<double>(y) * size + halfSize,
                static_cast<double>(z) * size + halfSize
            );
        }

        // Determines discrete cell coordinates enclosing the specified continuous 64-bit world position
        [[nodiscard]] static CellCoord FromWorldPosition(const Vec3D& position, uint32_t level, double baseCellSize) noexcept
        {
            const double multiplier = std::ldexp(1.0, static_cast<int>(level));
            const double size = baseCellSize * multiplier;
            const double invSize = 1.0 / size;

            return CellCoord(
                static_cast<int64_t>(std::floor(position.x * invSize)),
                static_cast<int64_t>(std::floor(position.y * invSize)),
                static_cast<int64_t>(std::floor(position.z * invSize)),
                level
            );
        }
    };
}

// 64-bit spatial hash specialization for CellCoord
template <>
struct std::hash<Sandbox3D::Engine::CellCoord>
{
    [[nodiscard]] size_t operator()(const Sandbox3D::Engine::CellCoord& coord) const noexcept
    {
        // High-entropy 64-bit splitmix/Murmur-style prime mixer
        constexpr uint64_t kPrime1 = 0xbf58476d1ce4e5b9ULL;
        constexpr uint64_t kPrime2 = 0x94d049bb133111ebULL;

        uint64_t h = static_cast<uint64_t>(coord.level);
        h ^= (static_cast<uint64_t>(coord.x) * kPrime1);
        h = (h << 31) | (h >> 33);
        h ^= (static_cast<uint64_t>(coord.y) * kPrime2);
        h = (h << 27) | (h >> 37);
        h ^= (static_cast<uint64_t>(coord.z) * kPrime1);
        h ^= h >> 30;
        h *= kPrime1;
        h ^= h >> 27;
        h *= kPrime2;
        h ^= h >> 31;
        return static_cast<size_t>(h);
    }
};

