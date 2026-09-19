// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Material.h"

#include <atomic>

namespace Sandbox3D::Engine
{
    Material::Material(std::string_view name)
        : m_name(name)
    {
    }

    Material::Material(const Maths::Vec4& albedo, float roughness, float metallic, std::string_view name)
        : m_name(name)
        , m_albedo(albedo)
        , m_roughness(std::clamp(roughness, 0.0f, 1.0f))
        , m_metallic(std::clamp(metallic, 0.0f, 1.0f))
    {
    }

    uint32_t Material::GenerateNextId() noexcept
    {
        static std::atomic<uint32_t> s_nextId{ 1 };
        return s_nextId.fetch_add(1, std::memory_order_relaxed);
    }

    std::shared_ptr<Material> Material::CreateDefault()
    {
        return std::make_shared<Material>("DefaultMaterial");
    }

    std::shared_ptr<Material> Material::CreateTerrain()
    {
        auto mat = std::make_shared<Material>("TerrainMaterial");
        mat->SetAlbedo(0.35f, 0.55f, 0.25f, 1.0f);
        mat->SetRoughness(0.85f);
        mat->SetMetallic(0.0f);
        mat->SetSpecular(0.2f);
        return mat;
    }

    std::shared_ptr<Material> Material::CreateMetal(const Maths::Vec4& albedo, float roughness, std::string_view name)
    {
        auto mat = std::make_shared<Material>(albedo, roughness, 1.0f, name);
        mat->SetSpecular(0.9f);
        return mat;
    }

    std::shared_ptr<Material> Material::CreateDielectric(const Maths::Vec4& albedo, float roughness, std::string_view name)
    {
        auto mat = std::make_shared<Material>(albedo, roughness, 0.0f, name);
        mat->SetSpecular(0.5f);
        return mat;
    }

    std::shared_ptr<Material> Material::CreateEmissive(const Maths::Vec3& emissive, float intensity, std::string_view name)
    {
        auto mat = std::make_shared<Material>(name);
        mat->SetAlbedo(0.0f, 0.0f, 0.0f, 1.0f);
        mat->SetEmissive(emissive * intensity);
        mat->SetRoughness(1.0f);
        mat->SetMetallic(0.0f);
        return mat;
    }
}

