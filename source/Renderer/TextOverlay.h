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
#include <vector>
#include <cstdint>

namespace Sandbox3D::Renderer
{
    using Microsoft::WRL::ComPtr;
    using Maths::Vec3;
    using Maths::Vec4;

    // Render diagnostic statistics passed to the overlay
    struct OverlayStatistics
    {
        std::string gpuName{};
        float       fps{ 0.0f };
        float       frameTimeMs{ 0.0f };
        size_t      triangleCount{ 0 };
        size_t      vertexCount{ 0 };
    };

    struct OverlayGlyph
    {
        std::vector<uint32_t> columns{};
    };

    // Encapsulates a high-performance 2D screen-space text overlay rendering engine metrics
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

        // Updates dynamic vertex/index buffers with current diagnostic strings
        void Update(
            UINT frameIndex,
            const OverlayStatistics& stats,
            uint32_t screenWidth,
            uint32_t screenHeight
        );

        // Issues draw calls to render the overlay in the top-right corner
        void Render(
            ID3D12GraphicsCommandList* commandList,
            UINT frameIndex,
            uint32_t screenWidth,
            uint32_t screenHeight,
            D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle
        );

        [[nodiscard]] bool IsInitialised() const noexcept { return m_isInitialised; }
        [[nodiscard]] float GetScale() const noexcept { return m_scale; }
        void SetScale(float scale) noexcept { m_scale = scale; }

        [[nodiscard]] const Vec4& GetTextColor() const noexcept { return m_textColor; }
        void SetTextColor(const Vec4& color) noexcept { m_textColor = color; }

        [[nodiscard]] const Vec4& GetBackgroundColor() const noexcept { return m_backgroundColor; }
        void SetBackgroundColor(const Vec4& color) noexcept { m_backgroundColor = color; }

        [[nodiscard]] float GetMarginX() const noexcept { return m_marginX; }
        [[nodiscard]] float GetMarginY() const noexcept { return m_marginY; }
        void SetMargin(float marginX, float marginY) noexcept { m_marginX = marginX; m_marginY = marginY; }

    private:
        void RasteriseFont();

        void BuildGeometry(
            const OverlayStatistics& stats,
            uint32_t screenWidth,
            uint32_t screenHeight,
            std::vector<Vertex>& outVertices,
            std::vector<uint32_t>& outIndices
        );

        static void AppendQuad(
            std::vector<Vertex>& vertices,
            std::vector<uint32_t>& indices,
            float x,
            float y,
            float z,
            float width,
            float height,
            const Vec4& color
        );

        void AppendCharacter(
            std::vector<Vertex>& vertices,
            std::vector<uint32_t>& indices,
            char character,
            float x,
            float y,
            float z,
            float scale,
            const Vec4& color
        ) const;

    private:
        ComPtr<ID3D12Resource>              m_vertexBuffer[BufferCount];
        ComPtr<ID3D12Resource>              m_indexBuffer[BufferCount];
        D3D12_VERTEX_BUFFER_VIEW            m_vertexBufferView[BufferCount]{};
        D3D12_INDEX_BUFFER_VIEW             m_indexBufferView[BufferCount]{};
        uint32_t                            m_indexCount[BufferCount]{};
        ConstantBuffer<SceneConstantBuffer> m_constantBuffer;

        OverlayGlyph                        m_glyphs[95]{};
        uint32_t                            m_glyphWidth{ 8 };
        uint32_t                            m_glyphHeight{ 14 };

        float                               m_scale{ 1.15f };
        float                               m_marginX{ 16.0f };
        float                               m_marginY{ 28.0f };

        float                               m_boundsMinX{ 0.0f };
        float                               m_boundsMinY{ 0.0f };
        float                               m_boundsMaxX{ 0.0f };
        float                               m_boundsMaxY{ 0.0f };

        Vec4                                m_textColor{ 1.0f, 0.0f, 0.0f, 1.0f };
        Vec4                                m_backgroundColor{ 0.0f, 0.0f, 0.0f, 0.0f };

        bool                                m_isInitialised{ false };
    };
}

