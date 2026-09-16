// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Maths/Maths.h"

namespace Sandbox3D::Renderer
{
    // Direct3D 12 Scene Constant Buffer containing transformation matrices, camera vectors, and lighting data
    struct SceneConstantBuffer
    {
        Maths::Mat4x4 mvp;            // combined model-view-projection matrix
        Maths::Mat4x4 world;          // world transformation matrix
        Maths::Vec4   lightDirection; // xyz = normalized direction to directional light, w = unused
        Maths::Vec4   lightColor;     // rgb = diffuse intensity, a = 1.0f
        Maths::Vec4   ambientColor;   // rgb = ambient intensity, a = 1.0f
    };

    using ModelViewProjectionBuffer = SceneConstantBuffer;
}

