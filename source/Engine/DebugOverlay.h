// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Renderer/TextOverlay.h"
#include "Maths/Maths.h"

#include <d3d12.h>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace Sandbox3D::Engine
{
    using Maths::Vec3D;
    using Maths::Vec4;

    // Render diagnostic statistics passed to the overlay
    struct OverlayStatistics
    {
        std::string  gpuName{};
        float        fps{ 0.0f };
        float        ups{ 0.0f };
        float        targetFps{ 120.0f };
        float        targetUps{ 60.0f };
        float        frameTimeMs{ 0.0f };
        uint32_t     sampleCount{ 1 };
        size_t       objectCount{ 0 };
        size_t       itemCount{ 0 };
        size_t       triangleCount{ 0 };
        size_t       vertexCount{ 0 };
        uint32_t     lightCount{ 0 };

        // Dynamic render pipeline, memory, and dispatch metrics
        uint64_t     vramLocalUsedBytes{ 0 };
        uint64_t     vramLocalBudgetBytes{ 0 };
        uint64_t     uploadUsedBytes{ 0 };
        uint32_t     drawCallCount{ 0 };
        uint32_t     batchCount{ 0 };
        size_t       culledItemCount{ 0 };

        // Window presentation mode, anti-aliasing technique, and camera spatial coordinates
        std::string  windowMode{ "Windowed" };
        std::string  aaDescription{ "MSAA 4x" };
        Maths::Vec3D cameraPosition{ 0.0, 0.0, 0.0 };
    };

    // Encapsulates the high-level engine diagnostics overlay, delegating text geometry rendering to TextOverlay
    class DebugOverlay final
    {
    public:
        DebugOverlay();
        ~DebugOverlay();

        DebugOverlay(const DebugOverlay&) = delete;
        DebugOverlay& operator=(const DebugOverlay&) = delete;
        DebugOverlay(DebugOverlay&&) noexcept = default;
        DebugOverlay& operator=(DebugOverlay&&) noexcept = default;

        void Initialise(ID3D12Device* device);
        void Shutdown() noexcept;

        // Formats engine telemetry statistics and buffers dual-block text geometry
        void Update(
            UINT frameIndex,
            const OverlayStatistics& stats,
            uint32_t screenWidth,
            uint32_t screenHeight
        );

        // Issues draw calls to render the diagnostic overlay
        void Render(
            ID3D12GraphicsCommandList* commandList,
            UINT frameIndex,
            uint32_t screenWidth,
            uint32_t screenHeight
        );

        [[nodiscard]] bool IsInitialised() const noexcept;
        [[nodiscard]] bool IsVisible() const noexcept;
        void SetVisible(bool visible) noexcept;
        void ToggleVisibility() noexcept;

        [[nodiscard]] float GetScale() const noexcept { return m_scale; }
        void SetScale(float scale) noexcept;

        [[nodiscard]] const Vec4& GetTextColor() const noexcept { return m_textColor; }
        void SetTextColor(const Vec4& color) noexcept;

        [[nodiscard]] const Vec4& GetBackgroundColor() const noexcept { return m_backgroundColor; }
        void SetBackgroundColor(const Vec4& color) noexcept;

        [[nodiscard]] float GetMarginX() const noexcept { return m_marginX; }
        [[nodiscard]] float GetMarginY() const noexcept { return m_marginY; }
        void SetMargin(float marginX, float marginY) noexcept { m_marginX = marginX; m_marginY = marginY; }

        [[nodiscard]] Renderer::TextOverlay& GetTextOverlay() noexcept { return *m_textOverlay; }
        [[nodiscard]] const Renderer::TextOverlay& GetTextOverlay() const noexcept { return *m_textOverlay; }

    private:
        void FormatMetrics(
            const OverlayStatistics& stats,
            uint32_t screenWidth,
            uint32_t screenHeight,
            std::vector<std::string>& outLeftLines,
            std::vector<std::string>& outRightLines
        ) const;

        static std::string FormatWithCommas(size_t value);
        static std::string TruncateDeviceName(std::string name, size_t maxLength = 24);

    private:
        std::unique_ptr<Renderer::TextOverlay> m_textOverlay;

        float                                  m_scale{ 1.30f };
        float                                  m_marginX{ 16.0f };
        float                                  m_marginY{ 28.0f };
        Vec4                                   m_textColor{ 0.92f, 0.50f, 0.08f, 1.0f }; // Deep orange aerospace amber
        Vec4                                   m_backgroundColor{ 0.0f, 0.0f, 0.0f, 0.0f };

        std::vector<std::string>               m_cachedLeftLines{};
        std::vector<std::string>               m_cachedRightLines{};
        uint32_t                               m_cachedScreenWidth{ 0 };
        uint32_t                               m_cachedScreenHeight{ 0 };
        bool                                   m_dirtyBuffer[Renderer::TextOverlay::BufferCount]{ true, true, true };
    };
}

