// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "BoundingFrustum.h"
#include "BoundingBox.h"
#include "BoundingSphere.h"

namespace Sandbox3D::Maths
{
    template <std::floating_point T>
    _BoundingFrustum<T>::_BoundingFrustum(const _Mat4x4<T>& viewProjectionMatrix) noexcept
    {
        ExtractFromMatrix(viewProjectionMatrix);
    }

    template <std::floating_point T>
    void _BoundingFrustum<T>::ExtractFromMatrix(const _Mat4x4<T>& m) noexcept
    {
        // Gribb-Hartmann plane extraction for row-vector convention (v * M)
        // Clip volume: -w <= x <= w, -w <= y <= w, 0 <= z <= w (DirectX [0, 1] depth)
        // Planes point inwards so DotCoordinate(p) >= 0 indicates the point is inside the frustum.

        // Near: z' >= 0  =>  col2 >= 0
        planes[Near] = _Plane<T>(
            m.m[0][2],
            m.m[1][2],
            m.m[2][2],
            m.m[3][2]
        ).Normalised();

        // Far: z' <= w'  =>  col3 - col2 >= 0
        planes[Far] = _Plane<T>(
            m.m[0][3] - m.m[0][2],
            m.m[1][3] - m.m[1][2],
            m.m[2][3] - m.m[2][2],
            m.m[3][3] - m.m[3][2]
        ).Normalised();

        // Left: x' >= -w'  =>  col3 + col0 >= 0
        planes[Left] = _Plane<T>(
            m.m[0][3] + m.m[0][0],
            m.m[1][3] + m.m[1][0],
            m.m[2][3] + m.m[2][0],
            m.m[3][3] + m.m[3][0]
        ).Normalised();

        // Right: x' <= w'  =>  col3 - col0 >= 0
        planes[Right] = _Plane<T>(
            m.m[0][3] - m.m[0][0],
            m.m[1][3] - m.m[1][0],
            m.m[2][3] - m.m[2][0],
            m.m[3][3] - m.m[3][0]
        ).Normalised();

        // Top: y' <= w'  =>  col3 - col1 >= 0
        planes[Top] = _Plane<T>(
            m.m[0][3] - m.m[0][1],
            m.m[1][3] - m.m[1][1],
            m.m[2][3] - m.m[2][1],
            m.m[3][3] - m.m[3][1]
        ).Normalised();

        // Bottom: y' >= -w'  =>  col3 + col1 >= 0
        planes[Bottom] = _Plane<T>(
            m.m[0][3] + m.m[0][1],
            m.m[1][3] + m.m[1][1],
            m.m[2][3] + m.m[2][1],
            m.m[3][3] + m.m[3][1]
        ).Normalised();
    }

    template <std::floating_point T>
    bool _BoundingFrustum<T>::Contains(const _Vec3<T>& point) const noexcept
    {
        for (size_t i = 0; i < PlaneCount; ++i)
        {
            if (planes[i].DotCoordinate(point) < -DefaultEpsilon<T>)
            {
                return false;
            }
        }
        return true;
    }

    template <std::floating_point T>
    bool _BoundingFrustum<T>::Intersects(const _BoundingBox<T>& box) const noexcept
    {
        for (size_t i = 0; i < PlaneCount; ++i)
        {
            const auto& plane = planes[i];

            // Compute positive vertex (the corner furthest along the inward normal)
            const _Vec3<T> pVertex(
                (plane.normal.x > static_cast<T>(0)) ? box.max.x : box.min.x,
                (plane.normal.y > static_cast<T>(0)) ? box.max.y : box.min.y,
                (plane.normal.z > static_cast<T>(0)) ? box.max.z : box.min.z
            );

            if (plane.DotCoordinate(pVertex) < static_cast<T>(0))
            {
                return false; // Entire box is outside this plane
            }
        }
        return true;
    }

    template <std::floating_point T>
    bool _BoundingFrustum<T>::Intersects(const _BoundingSphere<T>& sphere) const noexcept
    {
        for (size_t i = 0; i < PlaneCount; ++i)
        {
            if (planes[i].DotCoordinate(sphere.center) < -sphere.radius)
            {
                return false; // Entire sphere is outside this plane
            }
        }
        return true;
    }

    template <std::floating_point T>
    bool _BoundingFrustum<T>::operator==(const _BoundingFrustum& rhs) const noexcept
    {
        for (size_t i = 0; i < PlaneCount; ++i)
        {
            if (planes[i] != rhs.planes[i])
            {
                return false;
            }
        }
        return true;
    }

    // Explicit template instantiations for dual-tier precision
    template struct _BoundingFrustum<float>;
    template struct _BoundingFrustum<double>;
}

