// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "TextOverlay.h"
#include "DxCheck.h"

#include <windows.h>
#include <algorithm>
#include <cstring>

namespace Sandbox3D::Renderer
{
    TextOverlay::~TextOverlay()
    {
        Shutdown();
    }

    void TextOverlay::RasteriseFont()
    {
        HDC hdc = CreateCompatibleDC(nullptr);
        if (!hdc)
        {
            return;
        }

        constexpr int fontPixelHeight = -14;
        HFONT hFont = CreateFontW(
            fontPixelHeight,            // Character height in pixels
            0, 0, 0,
            FW_NORMAL,
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET,
            OUT_OUTLINE_PRECIS,
            CLIP_DEFAULT_PRECIS,
            NONANTIALIASED_QUALITY,     // Crisp single-bit grid fit
            FIXED_PITCH | FF_MODERN,
            L"Lucida Console"
        );

        HGDIOBJ oldFont = SelectObject(hdc, hFont);

        TEXTMETRICW tm{};
        GetTextMetricsW(hdc, &tm);
        constexpr uint32_t fallbackGlyphWidth  = 8;
        constexpr uint32_t fallbackGlyphHeight = 14;
        m_glyphWidth  = static_cast<uint32_t>(tm.tmAveCharWidth > 0 ? tm.tmAveCharWidth : fallbackGlyphWidth);
        m_glyphHeight = static_cast<uint32_t>(tm.tmHeight > 0 ? tm.tmHeight : fallbackGlyphHeight);

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = static_cast<LONG>(m_glyphWidth);
        bmi.bmiHeader.biHeight      = -static_cast<LONG>(m_glyphHeight); // Top-down DIB
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* dibPixels = nullptr;
        HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &dibPixels, nullptr, 0);
        HGDIOBJ oldBmp = SelectObject(hdc, hBitmap);

        ::SetBkColor(hdc, RGB(0, 0, 0));
        ::SetTextColor(hdc, RGB(255, 255, 255));

        constexpr char firstAscii = ' ';
        constexpr char lastAscii  = '~';

        for (char ch = firstAscii; ch <= lastAscii; ++ch)
        {
            const size_t glyphIndex = static_cast<size_t>(ch - firstAscii);
            OverlayGlyph& glyph = m_glyphs[glyphIndex];
            glyph.quads.clear();

            const RECT clearRect{ 0, 0, static_cast<LONG>(m_glyphWidth), static_cast<LONG>(m_glyphHeight) };
            FillRect(hdc, &clearRect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

            const wchar_t wch = static_cast<wchar_t>(ch);
            TextOutW(hdc, 0, 0, &wch, 1);
            GdiFlush();

            const auto* pixels = static_cast<const uint32_t*>(dibPixels);
            if (!pixels)
            {
                continue;
            }

            constexpr uint32_t luminanceThreshold = 0x40;

            struct VerticalRun
            {
                uint32_t col{ 0 };
                uint32_t startRow{ 0 };
                uint32_t length{ 0 };
                bool     used{ false };
            };
            std::vector<VerticalRun> runs;

            for (uint32_t col = 0; col < m_glyphWidth; ++col)
            {
                uint32_t row = 0;
                while (row < m_glyphHeight)
                {
                    const uint32_t pixel = pixels[row * m_glyphWidth + col];
                    const uint32_t red   = (pixel >> 16) & 0xFF;
                    const uint32_t green = (pixel >> 8) & 0xFF;
                    const uint32_t blue  = pixel & 0xFF;

                    if (red > luminanceThreshold || green > luminanceThreshold || blue > luminanceThreshold)
                    {
                        const uint32_t startRow = row;
                        while (row < m_glyphHeight)
                        {
                            const uint32_t p = pixels[row * m_glyphWidth + col];
                            const uint32_t r = (p >> 16) & 0xFF;
                            const uint32_t g = (p >> 8) & 0xFF;
                            const uint32_t b = p & 0xFF;
                            if (r > luminanceThreshold || g > luminanceThreshold || b > luminanceThreshold)
                            {
                                ++row;
                            }
                            else
                            {
                                break;
                            }
                        }
                        runs.push_back({ col, startRow, row - startRow, false });
                    }
                    else
                    {
                        ++row;
                    }
                }
            }

            // Horizontally coalesce adjacent identical vertical runs into wider quads
            for (size_t i = 0; i < runs.size(); ++i)
            {
                if (runs[i].used)
                {
                    continue;
                }
                runs[i].used = true;

                const uint32_t runCol   = runs[i].col;
                const uint32_t startRow = runs[i].startRow;
                const uint32_t length   = runs[i].length;
                uint32_t width          = 1;

                for (size_t j = i + 1; j < runs.size(); ++j)
                {
                    if (!runs[j].used && runs[j].col == runCol + width && runs[j].startRow == startRow && runs[j].length == length)
                    {
                        runs[j].used = true;
                        ++width;
                    }
                }

                glyph.quads.push_back({
                    static_cast<float>(runCol),
                    static_cast<float>(startRow),
                    static_cast<float>(width),
                    static_cast<float>(length)
                });
            }
        }

        SelectObject(hdc, oldBmp);
        DeleteObject(hBitmap);
        SelectObject(hdc, oldFont);
        DeleteObject(hFont);
        DeleteDC(hdc);
    }

    void TextOverlay::AllocateBufferForFrame(UINT frameIndex, size_t vertexCount, size_t indexCount)
    {
        if (!m_device || frameIndex >= BufferCount)
        {
            return;
        }

        const size_t vbSizeBytes = vertexCount * sizeof(Vertex);
        const size_t ibSizeBytes = indexCount * sizeof(uint32_t);

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type                 = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask     = 1;
        heapProps.VisibleNodeMask      = 1;

        D3D12_RESOURCE_DESC vbDesc = {};
        vbDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
        vbDesc.Alignment          = 0;
        vbDesc.Width              = vbSizeBytes;
        vbDesc.Height             = 1;
        vbDesc.DepthOrArraySize   = 1;
        vbDesc.MipLevels          = 1;
        vbDesc.Format             = DXGI_FORMAT_UNKNOWN;
        vbDesc.SampleDesc.Count   = 1;
        vbDesc.SampleDesc.Quality = 0;
        vbDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        vbDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

        D3D12_RESOURCE_DESC ibDesc = vbDesc;
        ibDesc.Width = ibSizeBytes;

        if (m_mappedVertexBuffer[frameIndex])
        {
            m_vertexBuffer[frameIndex]->Unmap(0, nullptr);
            m_mappedVertexBuffer[frameIndex] = nullptr;
        }
        if (m_mappedIndexBuffer[frameIndex])
        {
            m_indexBuffer[frameIndex]->Unmap(0, nullptr);
            m_mappedIndexBuffer[frameIndex] = nullptr;
        }

        HR_CHECK(m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &vbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffer[frameIndex])
        ));

        m_vertexBufferView[frameIndex].BufferLocation = m_vertexBuffer[frameIndex]->GetGPUVirtualAddress();
        m_vertexBufferView[frameIndex].StrideInBytes  = sizeof(Vertex);
        m_vertexBufferView[frameIndex].SizeInBytes    = static_cast<UINT>(vbSizeBytes);

        HR_CHECK(m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &ibDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_indexBuffer[frameIndex])
        ));

        m_indexBufferView[frameIndex].BufferLocation = m_indexBuffer[frameIndex]->GetGPUVirtualAddress();
        m_indexBufferView[frameIndex].Format         = DXGI_FORMAT_R32_UINT;
        m_indexBufferView[frameIndex].SizeInBytes    = static_cast<UINT>(ibSizeBytes);

        // Persistently map per-frame upload buffers to eliminate runtime Map/Unmap overhead
        const D3D12_RANGE readRange{ 0, 0 };
        HR_CHECK(m_vertexBuffer[frameIndex]->Map(0, &readRange, &m_mappedVertexBuffer[frameIndex]));
        HR_CHECK(m_indexBuffer[frameIndex]->Map(0, &readRange, &m_mappedIndexBuffer[frameIndex]));

        m_indexCount[frameIndex]        = 0;
        m_allocatedVertices[frameIndex] = vertexCount;
        m_allocatedIndices[frameIndex]  = indexCount;
    }

    void TextOverlay::Initialise(ID3D12Device* device)
    {
        if (m_isInitialised)
        {
            return;
        }

        m_device = device;
        RasteriseFont();

        // 65536 vertices and 131072 indices accommodates dual-block diagnostic overlays comfortably with dynamic growth fallback
        constexpr size_t initialMaxVertices = 65536;
        constexpr size_t initialMaxIndices  = 131072;

        for (UINT i = 0; i < BufferCount; ++i)
        {
            AllocateBufferForFrame(i, initialMaxVertices, initialMaxIndices);
        }

        m_constantBuffer.Initialise(device, BufferCount);
        m_pendingVertices.reserve(initialMaxVertices);
        m_pendingIndices.reserve(initialMaxIndices);

        m_isInitialised = true;
    }

    void TextOverlay::Shutdown() noexcept
    {
        if (!m_isInitialised)
        {
            return;
        }

        m_constantBuffer.Shutdown();

        for (size_t i = 0; i < BufferCount; ++i)
        {
            if (m_mappedVertexBuffer[i])
            {
                m_vertexBuffer[i]->Unmap(0, nullptr);
                m_mappedVertexBuffer[i] = nullptr;
            }
            if (m_mappedIndexBuffer[i])
            {
                m_indexBuffer[i]->Unmap(0, nullptr);
                m_mappedIndexBuffer[i] = nullptr;
            }
            m_vertexBuffer[i].Reset();
            m_indexBuffer[i].Reset();
            m_vertexBufferView[i]  = {};
            m_indexBufferView[i]   = {};
            m_indexCount[i]        = 0;
            m_allocatedVertices[i] = 0;
            m_allocatedIndices[i]  = 0;
        }

        m_pendingVertices.clear();
        m_pendingIndices.clear();
        m_device = nullptr;
        m_isInitialised = false;
    }

    void TextOverlay::ResetGeometry() noexcept
    {
        m_pendingVertices.clear();
        m_pendingIndices.clear();
        m_boundsMinX = 0.0f;
        m_boundsMinY = 0.0f;
        m_boundsMaxX = 0.0f;
        m_boundsMaxY = 0.0f;
    }

    void TextOverlay::DrawQuad(
        float x,
        float y,
        float z,
        float width,
        float height,
        const Vec4& color
    )
    {
        const uint32_t baseIndex = static_cast<uint32_t>(m_pendingVertices.size());

        // Four quad vertices
        Vertex v0{};
        v0.position = Vec3(x, y, z);
        v0.color    = color;
        v0.normal   = Vec3(0.0f, 0.0f, -1.0f);

        Vertex v1{};
        v1.position = Vec3(x + width, y, z);
        v1.color    = color;
        v1.normal   = Vec3(0.0f, 0.0f, -1.0f);

        Vertex v2{};
        v2.position = Vec3(x + width, y + height, z);
        v2.color    = color;
        v2.normal   = Vec3(0.0f, 0.0f, -1.0f);

        Vertex v3{};
        v3.position = Vec3(x, y + height, z);
        v3.color    = color;
        v3.normal   = Vec3(0.0f, 0.0f, -1.0f);

        m_pendingVertices.push_back(v0);
        m_pendingVertices.push_back(v1);
        m_pendingVertices.push_back(v2);
        m_pendingVertices.push_back(v3);

        m_pendingIndices.push_back(baseIndex + 0);
        m_pendingIndices.push_back(baseIndex + 1);
        m_pendingIndices.push_back(baseIndex + 2);
        m_pendingIndices.push_back(baseIndex + 0);
        m_pendingIndices.push_back(baseIndex + 2);
        m_pendingIndices.push_back(baseIndex + 3);
    }

    void TextOverlay::DrawCharacter(
        char character,
        float x,
        float y,
        float z,
        float scale,
        const Vec4& color
    )
    {
        constexpr char firstAscii = ' ';
        constexpr char lastAscii  = '~';

        if (character < firstAscii || character > lastAscii)
        {
            return;
        }

        const size_t glyphIndex = static_cast<size_t>(character - firstAscii);
        const OverlayGlyph& glyph = m_glyphs[glyphIndex];

        for (const auto& quad : glyph.quads)
        {
            DrawQuad(
                x + quad.relX * scale,
                y + quad.relY * scale,
                z,
                quad.relWidth * scale,
                quad.relHeight * scale,
                color
            );
        }
    }

    void TextOverlay::DrawString(
        std::string_view text,
        float x,
        float y,
        const Vec4& color,
        float scale,
        float depth
    )
    {
        const float advance = static_cast<float>(m_glyphWidth) * scale;

        // Render drop shadow glyph characters if enabled
        if (m_dropShadowEnabled && m_shadowColor.w > 0.0f)
        {
            constexpr float shadowDepth = 0.22f;
            const float shadowOffsetX   = m_shadowOffset.x * scale;
            const float shadowOffsetY   = m_shadowOffset.y * scale;

            float currentX = x + shadowOffsetX;
            for (char ch : text)
            {
                if (ch != ' ')
                {
                    DrawCharacter(ch, currentX, y + shadowOffsetY, shadowDepth, scale, m_shadowColor);
                }
                currentX += advance;
            }
        }

        // Render primary glyph characters
        float currentX = x;
        for (char ch : text)
        {
            if (ch != ' ')
            {
                DrawCharacter(ch, currentX, y, depth, scale, color);
            }
            currentX += advance;
        }
    }

    void TextOverlay::DrawBlock(
        std::span<const std::string> lines,
        float startX,
        float startY,
        const Vec4& textColor,
        const Vec4& backgroundColor,
        float scale
    )
    {
        const float charAdvance = static_cast<float>(m_glyphWidth) * scale;
        const float lineHeight  = static_cast<float>(m_glyphHeight) * scale;
        const float padY        = 1.0f * scale;
        constexpr float bgDepth     = 0.25f;
        constexpr float shadowDepth = 0.22f;
        constexpr float textDepth   = 0.20f;

        // Render background quads for non-empty character tokens
        if (backgroundColor.w > 0.0f)
        {
            float currentY = startY;
            for (const auto& line : lines)
            {
                size_t col = 0;
                const size_t lineLen = line.length();
                while (col < lineLen)
                {
                    if (line[col] != ' ')
                    {
                        const size_t startCol = col;
                        while (col < lineLen && line[col] != ' ')
                        {
                            ++col;
                        }
                        const size_t runLength = col - startCol;

                        const float quadX      = startX + static_cast<float>(startCol) * charAdvance;
                        const float quadY      = currentY + padY;
                        const float quadWidth  = static_cast<float>(runLength) * charAdvance;
                        const float quadHeight = lineHeight - 2.0f * padY;

                        DrawQuad(quadX, quadY, bgDepth, quadWidth, quadHeight, backgroundColor);
                    }
                    else
                    {
                        ++col;
                    }
                }
                currentY += lineHeight;
            }
        }

        // Render drop shadow glyph characters if enabled
        if (m_dropShadowEnabled && m_shadowColor.w > 0.0f)
        {
            const float shadowOffsetX = m_shadowOffset.x * scale;
            const float shadowOffsetY = m_shadowOffset.y * scale;

            float currentY = startY + shadowOffsetY;
            for (const auto& line : lines)
            {
                float currentX = startX + shadowOffsetX;
                for (char ch : line)
                {
                    if (ch != ' ')
                    {
                        DrawCharacter(ch, currentX, currentY, shadowDepth, scale, m_shadowColor);
                    }
                    currentX += charAdvance;
                }
                currentY += lineHeight;
            }
        }

        // Render primary glyph characters
        float currentY = startY;
        for (const auto& line : lines)
        {
            float currentX = startX;
            for (char ch : line)
            {
                if (ch != ' ')
                {
                    DrawCharacter(ch, currentX, currentY, textDepth, scale, textColor);
                }
                currentX += charAdvance;
            }
            currentY += lineHeight;
        }
    }

    void TextOverlay::SetScissorBounds(float minX, float minY, float maxX, float maxY) noexcept
    {
        m_boundsMinX = minX;
        m_boundsMinY = minY;
        m_boundsMaxX = maxX;
        m_boundsMaxY = maxY;
    }

    float TextOverlay::MeasureString(std::string_view text, float scale) const noexcept
    {
        return static_cast<float>(text.length()) * static_cast<float>(m_glyphWidth) * scale;
    }

    Vec2 TextOverlay::MeasureBlock(std::span<const std::string> lines, float scale) const noexcept
    {
        size_t maxLen = 0;
        for (const auto& line : lines)
        {
            maxLen = std::max(maxLen, line.length());
        }

        const float width  = static_cast<float>(maxLen) * static_cast<float>(m_glyphWidth) * scale;
        const float height = static_cast<float>(lines.size()) * static_cast<float>(m_glyphHeight) * scale;
        return Vec2(width, height);
    }

    void TextOverlay::UploadBuffers(UINT frameIndex)
    {
        if (!m_isInitialised || frameIndex >= BufferCount)
        {
            return;
        }

        if (m_pendingVertices.empty() || m_pendingIndices.empty())
        {
            m_indexCount[frameIndex] = 0;
            return;
        }

        // Dynamically grow per-frame geometry buffers if accumulated text exceeds current capacity
        if (m_pendingVertices.size() > m_allocatedVertices[frameIndex] ||
            m_pendingIndices.size() > m_allocatedIndices[frameIndex])
        {
            const size_t newVertexCap = std::max(m_allocatedVertices[frameIndex] * 2, m_pendingVertices.size());
            const size_t newIndexCap  = std::max(m_allocatedIndices[frameIndex] * 2, m_pendingIndices.size());
            AllocateBufferForFrame(frameIndex, newVertexCap, newIndexCap);
        }

        const size_t uploadVertexCount = std::min(m_pendingVertices.size(), m_allocatedVertices[frameIndex]);
        const size_t uploadIndexCount  = std::min(m_pendingIndices.size(), m_allocatedIndices[frameIndex]);

        const size_t vbBytes = uploadVertexCount * sizeof(Vertex);
        const size_t ibBytes = uploadIndexCount * sizeof(uint32_t);

        if (m_mappedVertexBuffer[frameIndex] && m_mappedIndexBuffer[frameIndex])
        {
            std::memcpy(m_mappedVertexBuffer[frameIndex], m_pendingVertices.data(), vbBytes);
            std::memcpy(m_mappedIndexBuffer[frameIndex], m_pendingIndices.data(), ibBytes);
        }

        m_indexCount[frameIndex] = static_cast<uint32_t>(uploadIndexCount);
    }

    void TextOverlay::Render(
        ID3D12GraphicsCommandList* commandList,
        UINT frameIndex,
        uint32_t screenWidth,
        uint32_t screenHeight
    )
    {
        if (!m_isVisible || !m_isInitialised || frameIndex >= BufferCount || m_indexCount[frameIndex] == 0)
        {
            return;
        }

        // Update Constant Buffer for 2D screen-space pixel projection
        SceneConstantBuffer cb{};
        cb.mvp          = Maths::Mat4x4::OrthographicPixelSpace(static_cast<float>(screenWidth), static_cast<float>(screenHeight));
        cb.world        = Maths::Mat4x4::Identity();
        cb.ambientColor = Maths::Vec4::One(); // Unlit, 100% vertex colour
        cb.lightCount   = 0;
        m_constantBuffer.Update(cb, frameIndex);

        // Set viewport covering full screen and scissor rect clamped to overlay bounds
        D3D12_VIEWPORT viewport{};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width    = static_cast<float>(screenWidth);
        viewport.Height   = static_cast<float>(screenHeight);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        D3D12_RECT scissor{};
        scissor.left   = static_cast<LONG>(std::clamp(m_boundsMinX - 1.0f, 0.0f, static_cast<float>(screenWidth)));
        scissor.top    = static_cast<LONG>(std::clamp(m_boundsMinY - 1.0f, 0.0f, static_cast<float>(screenHeight)));
        scissor.right  = static_cast<LONG>(std::clamp(m_boundsMaxX + 1.0f, 0.0f, static_cast<float>(screenWidth)));
        scissor.bottom = static_cast<LONG>(std::clamp(m_boundsMaxY + 1.0f, 0.0f, static_cast<float>(screenHeight)));

        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissor);

        // Bind resources and issue draw call
        commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffer.GetGpuVirtualAddress(frameIndex));
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView[frameIndex]);
        commandList->IASetIndexBuffer(&m_indexBufferView[frameIndex]);
        commandList->DrawIndexedInstanced(m_indexCount[frameIndex], 1, 0, 0, 0);
    }
}
