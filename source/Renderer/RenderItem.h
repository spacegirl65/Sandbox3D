// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Mesh.h"
#include "Material.h"
#include "Engine/LightChannel.h"
#include "Maths/Maths.h"

#include <memory>
#include <string>

namespace Sandbox3D::Renderer
{
    using Engine::LightChannel;
    using Maths::Mat4x4D;
    using Maths::Vec3D;
    using Maths::BoundingBox;

    // Direct3D 12 per-instance payload structure for batched and instanced drawing
    struct InstanceData
    {
        Maths::Mat4x4 cameraRelativeMVP{ Maths::Mat4x4::Identity() };
        Maths::Mat4x4 cameraRelativeWorld{ Maths::Mat4x4::Identity() };
        Maths::Vec4   colorTint{ Maths::Vec4::One() };
    };

    using GpuInstanceData = InstanceData;
    static_assert(sizeof(InstanceData) == 144, "InstanceData must be exactly 144 bytes");

    // Represents an active instance of a Mesh placed in the 3D world with a 64-bit transform
    struct RenderItem
    {
        std::shared_ptr<Mesh>     mesh{};
        std::shared_ptr<Material> material{};
        Mat4x4D                   worldMatrix{ Mat4x4D::Identity() };
        Maths::Vec4               colorTint{ Maths::Vec4::One() };
        uint32_t                  lightChannels{ LightChannel::Default };
        bool                      isVisible{ true };
        std::string               name{};

        RenderItem() = default;

        RenderItem(std::shared_ptr<Mesh> m, const Mat4x4D& wm = Mat4x4D::Identity(), bool visible = true, std::string n = {})
            : mesh(std::move(m)), material(nullptr), worldMatrix(wm), colorTint(Maths::Vec4::One()), lightChannels(LightChannel::Default), isVisible(visible), name(std::move(n))
        {
        }

        RenderItem(std::shared_ptr<Mesh> m, std::shared_ptr<Material> mat, const Mat4x4D& wm = Mat4x4D::Identity(), bool visible = true, std::string n = {})
            : mesh(std::move(m)), material(std::move(mat)), worldMatrix(wm), colorTint(Maths::Vec4::One()), lightChannels(LightChannel::Default), isVisible(visible), name(std::move(n))
        {
        }

        RenderItem(std::shared_ptr<Mesh> m, std::shared_ptr<Material> mat, const Mat4x4D& wm, const Maths::Vec4& tint, bool visible = true, std::string n = {})
            : mesh(std::move(m)), material(std::move(mat)), worldMatrix(wm), colorTint(tint), lightChannels(LightChannel::Default), isVisible(visible), name(std::move(n))
        {
        }

        [[nodiscard]] BoundingBox GetWorldBoundingBox() const noexcept
        {
            if (!mesh)
            {
                return BoundingBox();
            }
            // Transform local bounding box by 32-bit cast of world matrix
            return mesh->GetBoundingBox().Transformed(Maths::Mat4x4(worldMatrix));
        }

        [[nodiscard]] Maths::BoundingBoxD GetWorldBoundingBoxD() const noexcept
        {
            if (!mesh)
            {
                return Maths::BoundingBoxD();
            }
            return Maths::BoundingBoxD(mesh->GetBoundingBox()).Transformed(worldMatrix);
        }

        [[nodiscard]] Maths::BoundingSphereD GetWorldBoundingSphereD() const noexcept
        {
            if (!mesh)
            {
                return Maths::BoundingSphereD();
            }
            return Maths::BoundingSphereD(mesh->GetBoundingSphere()).Transformed(worldMatrix);
        }
    };
}

