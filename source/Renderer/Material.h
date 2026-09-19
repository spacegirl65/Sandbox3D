// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Maths/Maths.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace Sandbox3D::Renderer
{
    // Represents optical surface reflectance, shading attributes, and texture mapping for rendered entities
    class Material
    {
    public:
        explicit Material(std::string_view name = "DefaultMaterial");
        Material(const Maths::Vec4& albedo, float roughness = 0.5f, float metallic = 0.0f, std::string_view name = "Material");
        virtual ~Material() = default;

        Material(const Material&) = default;
        Material& operator=(const Material&) = default;
        Material(Material&&) noexcept = default;
        Material& operator=(Material&&) noexcept = default;

        // Identification & metadata
        [[nodiscard]] uint32_t GetId() const noexcept { return m_id; }
        [[nodiscard]] const std::string& GetName() const noexcept { return m_name; }
        void SetName(std::string_view name) { m_name = name; }

        // Surface reflectance properties
        [[nodiscard]] const Maths::Vec4& GetAlbedo() const noexcept { return m_albedo; }
        void SetAlbedo(const Maths::Vec4& albedo) noexcept { m_albedo = albedo; }
        void SetAlbedo(float r, float g, float b, float a = 1.0f) noexcept { m_albedo = Maths::Vec4(r, g, b, a); }

        [[nodiscard]] float GetRoughness() const noexcept { return m_roughness; }
        void SetRoughness(float roughness) noexcept { m_roughness = std::clamp(roughness, 0.0f, 1.0f); }

        [[nodiscard]] float GetMetallic() const noexcept { return m_metallic; }
        void SetMetallic(float metallic) noexcept { m_metallic = std::clamp(metallic, 0.0f, 1.0f); }

        [[nodiscard]] float GetSpecular() const noexcept { return m_specular; }
        void SetSpecular(float specular) noexcept { m_specular = std::clamp(specular, 0.0f, 1.0f); }

        [[nodiscard]] const Maths::Vec3& GetEmissive() const noexcept { return m_emissive; }
        void SetEmissive(const Maths::Vec3& emissive) noexcept { m_emissive = emissive; }
        void SetEmissive(float r, float g, float b) noexcept { m_emissive = Maths::Vec3(r, g, b); }

        // Texture resource paths
        [[nodiscard]] const std::string& GetAlbedoTexturePath() const noexcept { return m_albedoTexturePath; }
        void SetAlbedoTexturePath(std::string_view path) { m_albedoTexturePath = path; }
        [[nodiscard]] bool HasAlbedoTexture() const noexcept { return !m_albedoTexturePath.empty(); }

        [[nodiscard]] const std::string& GetNormalTexturePath() const noexcept { return m_normalTexturePath; }
        void SetNormalTexturePath(std::string_view path) { m_normalTexturePath = path; }
        [[nodiscard]] bool HasNormalTexture() const noexcept { return !m_normalTexturePath.empty(); }

        [[nodiscard]] const std::string& GetRoughnessTexturePath() const noexcept { return m_roughnessTexturePath; }
        void SetRoughnessTexturePath(std::string_view path) { m_roughnessTexturePath = path; }
        [[nodiscard]] bool HasRoughnessTexture() const noexcept { return !m_roughnessTexturePath.empty(); }

        // Shading flags
        [[nodiscard]] bool IsWireframe() const noexcept { return m_isWireframe; }
        void SetWireframe(bool wireframe) noexcept { m_isWireframe = wireframe; }

        [[nodiscard]] bool IsTransparent() const noexcept { return m_isTransparent || m_albedo.w < 1.0f; }
        void SetTransparent(bool transparent) noexcept { m_isTransparent = transparent; }

        // Preset factory methods
        [[nodiscard]] static std::shared_ptr<Material> CreateDefault();
        [[nodiscard]] static std::shared_ptr<Material> CreateTerrain();
        [[nodiscard]] static std::shared_ptr<Material> CreateMetal(const Maths::Vec4& albedo, float roughness = 0.2f, std::string_view name = "MetalMaterial");
        [[nodiscard]] static std::shared_ptr<Material> CreateDielectric(const Maths::Vec4& albedo, float roughness = 0.5f, std::string_view name = "DielectricMaterial");
        [[nodiscard]] static std::shared_ptr<Material> CreateEmissive(const Maths::Vec3& emissive, float intensity = 1.0f, std::string_view name = "EmissiveMaterial");

    protected:
        uint32_t     m_id{ GenerateNextId() };
        std::string  m_name;
        Maths::Vec4  m_albedo{ 1.0f, 1.0f, 1.0f, 1.0f };
        float        m_roughness{ 0.5f };
        float        m_metallic{ 0.0f };
        float        m_specular{ 0.5f };
        Maths::Vec3  m_emissive{ 0.0f, 0.0f, 0.0f };
        std::string  m_albedoTexturePath{};
        std::string  m_normalTexturePath{};
        std::string  m_roughnessTexturePath{};
        bool         m_isWireframe{ false };
        bool         m_isTransparent{ false };

    private:
        static uint32_t GenerateNextId() noexcept;
    };
}

