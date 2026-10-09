// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "ConstantBuffer.h"
#include "SceneConstantBuffer.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Maths/Maths.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <cstdint>

namespace Sandbox3D::Renderer
{
    using Microsoft::WRL::ComPtr;
    using Maths::Vec2;
    using Maths::Vec3;
    using Maths::Vec4;

    struct OverlayGlyph
    {
        std::vector<uint32_t> columns{};
    };

    // Encapsulates a reusable high-performance 2D screen-space monospaced text rendering engine
    class TextOverlay final
    {
    public:
        static constexpr size_t BufferCount = 3;

        TextOverlay() = default;
        ~TextOverlay();

        TextOverlay(const TextOverlay&) = delete;
        TextOverlay& operator=(const TextOverlay&) = delete;
        TextOverlay(TextOverlay&&) noexcept = default;
        TextOverlay& operator=(TextOverlay&&) noexcept = default;

        void Initialise(ID3D12Device* device);
        void Shutdown() noexcept;

        // Clears accumulated geometry before buffering a new frame of text
        void ResetGeometry() noexcept;

        // Adds a single character glyph to the geometry stream
        void DrawCharacter(
            char character,
            float x,
            float y,
            float z,
            float scale,
            const Vec4& color
        );

        // Appends a coloured quad primitive to the geometry stream
        void DrawQuad(
            float x,
            float y,
            float z,
            float width,
            float height,
            const Vec4& color
        );

        // Submits a single-line string of text
        void DrawString(
            std::string_view text,
            float x,
            float y,
            const Vec4& color,
            float scale = 1.0f,
            float depth = 0.20f
        );

        // Submits a multiline text block with optional background quads
        void DrawBlock(
            std::span<const std::string> lines,
            float startX,
            float startY,
            const Vec4& textColor,
            const Vec4& backgroundColor,
            float scale = 1.0f
        );

        // Explicitly sets scissor bounding box extents for screen-space clipping
        void SetScissorBounds(float minX, float minY, float maxX, float maxY) noexcept;

        // Uploads accumulated vertex and index geometry to the per-frame GPU buffers
        void UploadBuffers(UINT frameIndex);

        // Issues draw calls to render the accumulated text geometry
        void Render(
            ID3D12GraphicsCommandList* commandList,
            UINT frameIndex,
            uint32_t screenWidth,
            uint32_t screenHeight,
            D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle
        );

        // Font and layout metrics accessors
        [[nodiscard]] uint32_t GetGlyphWidth() const noexcept { return m_glyphWidth; }
        [[nodiscard]] uint32_t GetGlyphHeight() const noexcept { return m_glyphHeight; }
        [[nodiscard]] float GetCharAdvance(float scale = 1.0f) const noexcept { return static_cast<float>(m_glyphWidth) * scale; }
        [[nodiscard]] float GetLineHeight(float scale = 1.0f) const noexcept { return static_cast<float>(m_glyphHeight) * scale; }
        [[nodiscard]] float MeasureString(std::string_view text, float scale = 1.0f) const noexcept;
        [[nodiscard]] Vec2 MeasureBlock(std::span<const std::string> lines, float scale = 1.0f) const noexcept;

        // State and visibility queries
        [[nodiscard]] bool IsInitialised() const noexcept { return m_isInitialised; }
        [[nodiscard]] bool IsVisible() const noexcept { return m_isVisible; }
        void SetVisible(bool visible) noexcept { m_isVisible = visible; }
        void ToggleVisibility() noexcept { m_isVisible = !m_isVisible; }

        [[nodiscard]] float GetScale() const noexcept { return m_scale; }
        void SetScale(float scale) noexcept { m_scale = scale; }

        [[nodiscard]] const Vec4& GetTextColor() const noexcept { return m_textColor; }
        void SetTextColor(const Vec4& color) noexcept { m_textColor = color; }

        [[nodiscard]] const Vec4& GetBackgroundColor() const noexcept { return m_backgroundColor; }
        void SetBackgroundColor(const Vec4& color) noexcept { m_backgroundColor = color; }

        [[nodiscard]] float GetBoundsMinX() const noexcept { return m_boundsMinX; }
        [[nodiscard]] float GetBoundsMinY() const noexcept { return m_boundsMinY; }
        [[nodiscard]] float GetBoundsMaxX() const noexcept { return m_boundsMaxX; }
        [[nodiscard]] float GetBoundsMaxY() const noexcept { return m_boundsMaxY; }

    private:
        void RasteriseFont();
        void AllocateBufferForFrame(UINT frameIndex, size_t vertexCount, size_t indexCount);

    private:
        ID3D12Device*                       m_device{ nullptr };
        ComPtr<ID3D12Resource>              m_vertexBuffer[BufferCount];
        ComPtr<ID3D12Resource>              m_indexBuffer[BufferCount];
        D3D12_VERTEX_BUFFER_VIEW            m_vertexBufferView[BufferCount]{};
        D3D12_INDEX_BUFFER_VIEW             m_indexBufferView[BufferCount]{};
        uint32_t                            m_indexCount[BufferCount]{};
        size_t                              m_allocatedVertices[BufferCount]{};
        size_t                              m_allocatedIndices[BufferCount]{};
        ConstantBuffer<SceneConstantBuffer> m_constantBuffer;

        std::vector<Vertex>                 m_pendingVertices;
        std::vector<uint32_t>               m_pendingIndices;

        OverlayGlyph                        m_glyphs[95]{};
        uint32_t                            m_glyphWidth{ 8 };
        uint32_t                            m_glyphHeight{ 14 };

        float                               m_scale{ 1.30f };
        float                               m_boundsMinX{ 0.0f };
        float                               m_boundsMinY{ 0.0f };
        float                               m_boundsMaxX{ 0.0f };
        float                               m_boundsMaxY{ 0.0f };

        Vec4                                m_textColor{ 0.92f, 0.50f, 0.08f, 1.0f }; // Deep orange aerospace amber
        Vec4                                m_backgroundColor{ 0.0f, 0.0f, 0.0f, 0.0f };

        bool                                m_isInitialised{ false };
        bool                                m_isVisible{ false };
    };
}

