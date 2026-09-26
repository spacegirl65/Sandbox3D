// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Character.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace Sandbox3D::Engine
{
    using Renderer::Vertex;
    using Maths::Vec3;
    using Maths::Vec4;
    using Maths::Vec3D;
    using Maths::Mat4x4D;

    Character::Character(std::string_view name)
        : Body(name)
        , m_camera(std::make_unique<Camera>(std::string(name) + "EyeCamera"))
    {
        constexpr double colliderRadius    = 0.35;
        constexpr double colliderCylHeight = 1.07;
        const Vec3D colliderOffset(0.0, 0.885, 0.0);
        SetCapsuleCollider(colliderRadius, colliderCylHeight, colliderOffset);

        SynchroniseCamera();
    }

    Character::Character(ID3D12Device* device, std::string_view name)
        : Body(name)
        , m_camera(std::make_unique<Camera>(std::string(name) + "EyeCamera"))
    {
        constexpr double colliderRadius    = 0.35;
        constexpr double colliderCylHeight = 1.07;
        const Vec3D colliderOffset(0.0, 0.885, 0.0);
        SetCapsuleCollider(colliderRadius, colliderCylHeight, colliderOffset);

        if (device)
        {
            m_mesh = CreateCharacterMesh(device);
            SynchroniseRenderItem();
        }
        SynchroniseCamera();
    }

    void Character::Update([[maybe_unused]] float deltaTime)
    {
        // Future player locomotion and physics will execute here
        SynchroniseCamera();
    }

    void Character::SetPosition(const Vec3D& position)
    {
        Body::SetPosition(position);
        SynchroniseCamera();
    }

    void Character::SetWorldMatrix(const Mat4x4D& worldMatrix)
    {
        Body::SetWorldMatrix(worldMatrix);
        SynchroniseCamera();
    }

    void Character::SetEyeOffset(const Vec3D& offset) noexcept
    {
        m_eyeOffset = offset;
        SynchroniseCamera();
    }

    void Character::SetYaw(double yaw) noexcept
    {
        m_yaw = yaw;
        SynchroniseCamera();
    }

    void Character::SetPitch(double pitch) noexcept
    {
        constexpr double maxPitch = 1.55; // ~89 degrees
        m_pitch = std::clamp(pitch, -maxPitch, maxPitch);
        SynchroniseCamera();
    }

    void Character::SynchroniseCamera() noexcept
    {
        if (!m_camera)
        {
            return;
        }

        const Vec3D eyePosition = m_position + m_eyeOffset;

        const double cosPitch = std::cos(m_pitch);
        const double sinPitch = std::sin(m_pitch);
        const double cosYaw   = std::cos(m_yaw);
        const double sinYaw   = std::sin(m_yaw);

        const Vec3D forward(
            cosPitch * sinYaw,
            sinPitch,
            cosPitch * cosYaw
        );

        const Vec3D right(
            cosYaw,
            0.0,
            -sinYaw
        );

        const Vec3D cameraUp = right.Cross(forward).Normalised();
        const Vec3D target   = eyePosition + forward;

        m_camera->SetLookAt(eyePosition, target, cameraUp);
    }

    std::shared_ptr<Renderer::Mesh> Character::CreateCharacterMesh(
        ID3D12Device* device,
        float capsuleRadius,
        float capsuleCylinderHeight,
        float headRadius,
        const Vec4& bodyColor,
        const Vec4& headColor
    )
    {
        if (!device)
        {
            return nullptr;
        }

        auto mesh = std::make_shared<Renderer::Mesh>();

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        constexpr uint32_t sliceCount   = 24u;
        constexpr uint32_t halfStacks   = 8u;
        constexpr float pi = std::numbers::pi_v<float>;
        constexpr float twoPi = 2.0f * pi;

        // =====================================================================
        // Part 1: Capsule Body (Base at Y = 0.0, Top at Y = 2*r + H)
        // =====================================================================
        const float r = capsuleRadius;
        const float h = capsuleCylinderHeight;
        const float topHemisphereCenterY    = r + h;
        const float bottomHemisphereCenterY = r;

        // Top pole of capsule body
        const uint32_t bodyTopPoleIdx = static_cast<uint32_t>(vertices.size());
        vertices.push_back({
            Vec3(0.0f, topHemisphereCenterY + r, 0.0f),
            Vec3(0.0f, 1.0f, 0.0f),
            bodyColor
        });

        // Top hemisphere rings (excluding pole and equator)
        for (uint32_t i = 1; i < halfStacks; ++i)
        {
            const float phi = (static_cast<float>(i) / static_cast<float>(halfStacks)) * (pi * 0.5f);
            const float cosPhi = std::cos(phi);
            const float sinPhi = std::sin(phi);
            const float y = topHemisphereCenterY + r * cosPhi;
            const float ringR = r * sinPhi;

            for (uint32_t j = 0; j < sliceCount; ++j)
            {
                const float theta = (static_cast<float>(j) / static_cast<float>(sliceCount)) * twoPi;
                const float cosTheta = std::cos(theta);
                const float sinTheta = std::sin(theta);

                const Vec3 normal(sinPhi * sinTheta, cosPhi, sinPhi * cosTheta);
                const Vec3 position(ringR * sinTheta, y, ringR * cosTheta);
                vertices.push_back({ position, normal.Normalised(), bodyColor });
            }
        }

        // Top cylinder ring (at top of cylinder)
        const uint32_t topCylinderRingStart = static_cast<uint32_t>(vertices.size());
        for (uint32_t j = 0; j < sliceCount; ++j)
        {
            const float theta = (static_cast<float>(j) / static_cast<float>(sliceCount)) * twoPi;
            const float cosTheta = std::cos(theta);
            const float sinTheta = std::sin(theta);

            const Vec3 normal(sinTheta, 0.0f, cosTheta);
            const Vec3 position(r * sinTheta, topHemisphereCenterY, r * cosTheta);
            vertices.push_back({ position, normal.Normalised(), bodyColor });
        }

        // Bottom cylinder ring (at bottom of cylinder)
        const uint32_t bottomCylinderRingStart = static_cast<uint32_t>(vertices.size());
        for (uint32_t j = 0; j < sliceCount; ++j)
        {
            const float theta = (static_cast<float>(j) / static_cast<float>(sliceCount)) * twoPi;
            const float cosTheta = std::cos(theta);
            const float sinTheta = std::sin(theta);

            const Vec3 normal(sinTheta, 0.0f, cosTheta);
            const Vec3 position(r * sinTheta, bottomHemisphereCenterY, r * cosTheta);
            vertices.push_back({ position, normal.Normalised(), bodyColor });
        }

        // Bottom hemisphere rings (from below equator to above bottom pole)
        for (uint32_t i = 1; i < halfStacks; ++i)
        {
            const float phi = (pi * 0.5f) + (static_cast<float>(i) / static_cast<float>(halfStacks)) * (pi * 0.5f);
            const float cosPhi = std::cos(phi);
            const float sinPhi = std::sin(phi);
            const float y = bottomHemisphereCenterY + r * cosPhi;
            const float ringR = r * sinPhi;

            for (uint32_t j = 0; j < sliceCount; ++j)
            {
                const float theta = (static_cast<float>(j) / static_cast<float>(sliceCount)) * twoPi;
                const float cosTheta = std::cos(theta);
                const float sinTheta = std::sin(theta);

                const Vec3 normal(sinPhi * sinTheta, cosPhi, sinPhi * cosTheta);
                const Vec3 position(ringR * sinTheta, y, ringR * cosTheta);
                vertices.push_back({ position, normal.Normalised(), bodyColor });
            }
        }

        // Bottom pole of capsule body
        const uint32_t bodyBottomPoleIdx = static_cast<uint32_t>(vertices.size());
        vertices.push_back({
            Vec3(0.0f, bottomHemisphereCenterY - r, 0.0f),
            Vec3(0.0f, -1.0f, 0.0f),
            bodyColor
        });

        // Top cap indices (top pole to first ring)
        for (uint32_t j = 0; j < sliceCount; ++j)
        {
            const uint32_t curr = 1u + j;
            const uint32_t next = 1u + ((j + 1u) % sliceCount);
            indices.push_back(bodyTopPoleIdx);
            indices.push_back(next);
            indices.push_back(curr);
        }

        // Intermediate rings indices on capsule body
        // Total ring count = (halfStacks - 1) + 2 (cylinder top and bottom) + (halfStacks - 1)
        const uint32_t totalBodyRings = (halfStacks - 1u) * 2u + 2u;
        for (uint32_t ring = 0; ring < totalBodyRings - 1u; ++ring)
        {
            const uint32_t rowA = 1u + ring * sliceCount;
            const uint32_t rowB = 1u + (ring + 1u) * sliceCount;

            for (uint32_t j = 0; j < sliceCount; ++j)
            {
                const uint32_t nextJ = (j + 1u) % sliceCount;
                const uint32_t a = rowA + j;
                const uint32_t b = rowA + nextJ;
                const uint32_t c = rowB + j;
                const uint32_t d = rowB + nextJ;

                // Triangle 1: a -> b -> d
                indices.push_back(a);
                indices.push_back(b);
                indices.push_back(d);

                // Triangle 2: a -> d -> c
                indices.push_back(a);
                indices.push_back(d);
                indices.push_back(c);
            }
        }

        // Bottom cap indices (last ring to bottom pole)
        const uint32_t lastBodyRingStart = 1u + (totalBodyRings - 1u) * sliceCount;
        for (uint32_t j = 0; j < sliceCount; ++j)
        {
            const uint32_t curr = lastBodyRingStart + j;
            const uint32_t next = lastBodyRingStart + ((j + 1u) % sliceCount);
            indices.push_back(curr);
            indices.push_back(next);
            indices.push_back(bodyBottomPoleIdx);
        }

        // =====================================================================
        // Part 2: Spherical Head (Centered on top of capsule)
        // =====================================================================
        constexpr float headOverlapFactor = 0.85f;
        const float headCenterY = topHemisphereCenterY + r + headRadius * headOverlapFactor;
        constexpr uint32_t headStackCount = 14u;
        constexpr uint32_t headSliceCount = 24u;

        // Top pole of head
        const uint32_t headTopPoleIdx = static_cast<uint32_t>(vertices.size());
        vertices.push_back({
            Vec3(0.0f, headCenterY + headRadius, 0.0f),
            Vec3(0.0f, 1.0f, 0.0f),
            headColor
        });

        // Intermediate rings of head sphere
        const uint32_t headFirstRingIdx = static_cast<uint32_t>(vertices.size());
        for (uint32_t i = 1; i < headStackCount; ++i)
        {
            const float phi = (static_cast<float>(i) / static_cast<float>(headStackCount)) * pi;
            const float cosPhi = std::cos(phi);
            const float sinPhi = std::sin(phi);
            const float y = headCenterY + headRadius * cosPhi;
            const float ringR = headRadius * sinPhi;

            for (uint32_t j = 0; j < headSliceCount; ++j)
            {
                const float theta = (static_cast<float>(j) / static_cast<float>(headSliceCount)) * twoPi;
                const float cosTheta = std::cos(theta);
                const float sinTheta = std::sin(theta);

                const Vec3 normal(sinPhi * sinTheta, cosPhi, sinPhi * cosTheta);
                const Vec3 position(ringR * sinTheta, y, ringR * cosTheta);
                vertices.push_back({ position, normal.Normalised(), headColor });
            }
        }

        // Bottom pole of head
        const uint32_t headBottomPoleIdx = static_cast<uint32_t>(vertices.size());
        vertices.push_back({
            Vec3(0.0f, headCenterY - headRadius, 0.0f),
            Vec3(0.0f, -1.0f, 0.0f),
            headColor
        });

        // Head top cap
        for (uint32_t j = 0; j < headSliceCount; ++j)
        {
            const uint32_t curr = headFirstRingIdx + j;
            const uint32_t next = headFirstRingIdx + ((j + 1u) % headSliceCount);
            indices.push_back(headTopPoleIdx);
            indices.push_back(next);
            indices.push_back(curr);
        }

        // Head intermediate quads
        for (uint32_t i = 0; i < headStackCount - 2u; ++i)
        {
            const uint32_t rowA = headFirstRingIdx + i * headSliceCount;
            const uint32_t rowB = headFirstRingIdx + (i + 1u) * headSliceCount;

            for (uint32_t j = 0; j < headSliceCount; ++j)
            {
                const uint32_t nextJ = (j + 1u) % headSliceCount;
                const uint32_t a = rowA + j;
                const uint32_t b = rowA + nextJ;
                const uint32_t c = rowB + j;
                const uint32_t d = rowB + nextJ;

                indices.push_back(a);
                indices.push_back(b);
                indices.push_back(d);

                indices.push_back(a);
                indices.push_back(d);
                indices.push_back(c);
            }
        }

        // Head bottom cap
        const uint32_t headLastRowStart = headFirstRingIdx + (headStackCount - 2u) * headSliceCount;
        for (uint32_t j = 0; j < headSliceCount; ++j)
        {
            const uint32_t curr = headLastRowStart + j;
            const uint32_t next = headLastRowStart + ((j + 1u) % headSliceCount);
            indices.push_back(curr);
            indices.push_back(next);
            indices.push_back(headBottomPoleIdx);
        }

        mesh->Initialise(device, vertices, indices);
        return mesh;
    }
}

