// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Mesh.h"
#include "Maths/Maths.h"

#include <memory>
#include <string>

namespace Sandbox3D::Renderer
{
    using Maths::Mat4x4D;
    using Maths::Vec3D;
    using Maths::BoundingBox;

    // Represents an active instance of a Mesh placed in the 3D world with a 64-bit transform
    struct RenderItem
    {
        std::shared_ptr<Mesh> mesh;
        Mat4x4D               worldMatrix{ Mat4x4D::Identity() };
        bool                  isVisible{ true };
        std::string           name{};

        [[nodiscard]] BoundingBox GetWorldBoundingBox() const noexcept
        {
            if (!mesh)
            {
                return BoundingBox();
            }
            // Transform local bounding box by 32-bit cast of world matrix
            return mesh->GetBoundingBox().Transformed(Maths::Mat4x4(worldMatrix));
        }
    };
}

