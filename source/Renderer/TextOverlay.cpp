// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "TextOverlay.h"
#include "DxCheck.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <stdexcept>

namespace Sandbox3D::Renderer
{
    namespace
    {
        // Formats large integer numbers with comma thousands separators (e.g. 131072 -> "131,072")
        std::string FormatWithCommas(size_t value)
        {
            std::string s = std::to_string(value);
            int insertPos = static_cast<int>(s.length()) - 3;
            while (insertPos > 0)
            {
                s.insert(static_cast<size_t>(insertPos), ",");
                insertPos -= 3;
            }
            return s;
        }

        // Shortens and truncates verbose GPU device descriptions for compact display
        std::string TruncateDeviceName(std::string name, size_t maxLength = 24)
        {
            // Remove common redundant marketing / legal tokens (preserving "Laptop")
            for (const std::string& token : { "(R)", "(TM)", "Corporation", " Graphics" })
            {
                size_t pos = 0;
                while ((pos = name.find(token, pos)) != std::string::npos)
                {
                    name.erase(pos, token.length());
                }
            }

            // Remove verbose vendor prefixes if known sub-brand follows
            if (name.starts_with("NVIDIA GeForce "))
            {
                name.erase(0, 15);
            }
            else if (name.starts_with("NVIDIA "))
            {
                name.erase(0, 7);
            }
            else if (name.starts_with("AMD Radeon "))
            {
                name.erase(0, 11);
            }
            else if (name.starts_with("Intel "))
            {
                name.erase(0, 6);
            }
            else if (name.starts_with("Microsoft "))
            {
                name.erase(0, 10);
            }

            // Collapse multiple consecutive spaces and trim leading/trailing whitespace
            size_t doubleSpace = 0;
            while ((doubleSpace = name.find("  ")) != std::string::npos)
            {
                name.erase(doubleSpace, 1);
            }

            while (!name.empty() && name.front() == ' ')
            {
                name.erase(name.begin());
            }
            while (!name.empty() && name.back() == ' ')
            {
                name.pop_back();
            }

            // If still exceeding max display length, truncate cleanly without cutting off "Laptop"
            if (name.length() > maxLength)
            {
                const std::string lower = [](std::string s) {
                    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    return s;
                }(name);

                const size_t laptopPos = lower.find("laptop");
                if (laptopPos != std::string::npos && laptopPos < maxLength + 6)
                {
                    const size_t endOfLaptop = laptopPos + 6;
                    name = name.substr(0, std::max(maxLength, endOfLaptop));
                }
                else
                {
                    name = name.substr(0, maxLength);
                }
            }

            return name;
        }
    }

    TextOverlay::~TextOverlay()
    {
        Shutdown();
    }

    void TextOverlay::RasteriseCascadiaFont()
    {
        HDC hdc = CreateCompatibleDC(nullptr);
        if (!hdc)
        {
            return;
        }

        HFONT hFont = CreateFontW(
            -14,                        // 14px character height
            0, 0, 0,
            FW_NORMAL,
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET,
            OUT_OUTLINE_PRECIS,
            CLIP_DEFAULT_PRECIS,
            NONANTIALIASED_QUALITY,     // Crisp single-bit grid fit
            FIXED_PITCH | FF_MODERN,
            L"Consolas"
        );

        HGDIOBJ oldFont = SelectObject(hdc, hFont);

        TEXTMETRICW tm{};
        GetTextMetricsW(hdc, &tm);
        m_glyphWidth  = static_cast<uint32_t>(tm.tmAveCharWidth > 0 ? tm.tmAveCharWidth : 8);
        m_glyphHeight = static_cast<uint32_t>(tm.tmHeight > 0 ? tm.tmHeight : 18);

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
        ::SetBkMode(hdc, OPAQUE);

        const auto* pixels = static_cast<const uint32_t*>(dibPixels);

        for (int charCode = 32; charCode <= 126; ++charCode)
        {
            const wchar_t wch = static_cast<wchar_t>(charCode);
            RECT rc = { 0, 0, static_cast<LONG>(m_glyphWidth), static_cast<LONG>(m_glyphHeight) };
            FillRect(hdc, &rc, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            TextOutW(hdc, 0, 0, &wch, 1);

            auto& glyph = m_glyphs[charCode - 32];
            glyph.columns.assign(m_glyphWidth, 0u);

            for (uint32_t col = 0; col < m_glyphWidth; ++col)
            {
                uint32_t colBits = 0;
                for (uint32_t row = 0; row < m_glyphHeight && row < 32; ++row)
                {
                    const uint32_t px = pixels[row * m_glyphWidth + col];
                    if ((px & 0xFF) > 64)
                    {
                        colBits |= (1u << row);
                    }
                }
                glyph.columns[col] = colBits;
            }
        }

        SelectObject(hdc, oldBmp);
        DeleteObject(hBitmap);
        SelectObject(hdc, oldFont);
        DeleteObject(hFont);
        DeleteDC(hdc);
    }

    void TextOverlay::Initialise(ID3D12Device* device)
    {
        if (m_isInitialised)
        {
            return;
        }

        RasteriseCascadiaFont();

        constexpr size_t maxVertices = 16384;
        constexpr size_t maxIndices  = 32768;

        const size_t vbSize = maxVertices * sizeof(Vertex);
        const size_t ibSize = maxIndices * sizeof(uint32_t);

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type                 = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask     = 1;
        heapProps.VisibleNodeMask      = 1;

        D3D12_RESOURCE_DESC vbDesc{};
        vbDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
        vbDesc.Width              = vbSize;
        vbDesc.Height             = 1;
        vbDesc.DepthOrArraySize   = 1;
        vbDesc.MipLevels          = 1;
        vbDesc.Format             = DXGI_FORMAT_UNKNOWN;
        vbDesc.SampleDesc.Count   = 1;
        vbDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        D3D12_RESOURCE_DESC ibDesc = vbDesc;
        ibDesc.Width               = ibSize;

        for (size_t i = 0; i < BufferCount; ++i)
        {
            HR_CHECK(device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &vbDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_vertexBuffer[i])
            ));

            HR_CHECK(device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &ibDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_indexBuffer[i])
            ));

            m_vertexBufferView[i].BufferLocation = m_vertexBuffer[i]->GetGPUVirtualAddress();
            m_vertexBufferView[i].SizeInBytes    = static_cast<UINT>(vbSize);
            m_vertexBufferView[i].StrideInBytes  = sizeof(Vertex);

            m_indexBufferView[i].BufferLocation  = m_indexBuffer[i]->GetGPUVirtualAddress();
            m_indexBufferView[i].SizeInBytes     = static_cast<UINT>(ibSize);
            m_indexBufferView[i].Format          = DXGI_FORMAT_R32_UINT;

            m_indexCount[i] = 0;
        }

        m_constantBuffer.Initialise(device, BufferCount);
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
            m_indexBuffer[i].Reset();
            m_vertexBuffer[i].Reset();
            m_indexCount[i] = 0;
        }

        m_isInitialised = false;
    }

    void TextOverlay::AppendQuad(
        std::vector<Vertex>& vertices,
        std::vector<uint32_t>& indices,
        float x,
        float y,
        float z,
        float width,
        float height,
        const Vec4& color
    )
    {
        const uint32_t base = static_cast<uint32_t>(vertices.size());
        const Vec3 normal(0.0f, 0.0f, -1.0f);

        vertices.push_back({ Vec3(x,         y,          z), normal, color });
        vertices.push_back({ Vec3(x + width, y,          z), normal, color });
        vertices.push_back({ Vec3(x + width, y + height, z), normal, color });
        vertices.push_back({ Vec3(x,         y + height, z), normal, color });

        // Clockwise in DirectX NDC space (Y points down, Z forward)
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    void TextOverlay::AppendCharacter(
        std::vector<Vertex>& vertices,
        std::vector<uint32_t>& indices,
        char character,
        float x,
        float y,
        float z,
        float scale,
        const Vec4& color
    ) const
    {
        const uint8_t uChar = static_cast<uint8_t>(character);
        if (uChar < 32 || uChar > 126)
        {
            return;
        }

        const auto& glyph = m_glyphs[uChar - 32];

        for (uint32_t col = 0; col < m_glyphWidth && col < glyph.columns.size(); ++col)
        {
            const uint32_t colBits = glyph.columns[col];
            if (colBits == 0)
            {
                continue;
            }

            uint32_t row = 0;
            while (row < m_glyphHeight)
            {
                if (colBits & (1u << row))
                {
                    const uint32_t startRow = row;
                    while (row < m_glyphHeight && (colBits & (1u << row)))
                    {
                        ++row;
                    }
                    const uint32_t runLength = row - startRow;

                    AppendQuad(
                        vertices,
                        indices,
                        x + static_cast<float>(col) * scale,
                        y + static_cast<float>(startRow) * scale,
                        z,
                        scale,
                        static_cast<float>(runLength) * scale,
                        color
                    );
                }
                else
                {
                    ++row;
                }
            }
        }
    }

    void TextOverlay::BuildGeometry(
        const OverlayStatistics& stats,
        uint32_t screenWidth,
        uint32_t screenHeight,
        std::vector<Vertex>& outVertices,
        std::vector<uint32_t>& outIndices
    )
    {
        // Construct display string lines
        std::vector<std::string> lines;
        if (!stats.gpuName.empty())
        {
            lines.push_back(std::format("GPU:  {}", TruncateDeviceName(stats.gpuName)));
        }
        lines.push_back(std::format("FPS:  {:.1f}", stats.fps));
        lines.push_back(std::format("Tris: {}", FormatWithCommas(stats.triangleCount)));
        lines.push_back(std::format("Vert: {}", FormatWithCommas(stats.vertexCount)));
        lines.push_back(std::format("Res:  {} x {}", screenWidth, screenHeight));

        const float charAdvance = static_cast<float>(m_glyphWidth) * m_scale;
        const float lineHeight  = static_cast<float>(m_glyphHeight) * m_scale;

        size_t maxLineLength = 0;
        for (const auto& line : lines)
        {
            maxLineLength = std::max(maxLineLength, line.length());
        }

        const float contentWidth  = static_cast<float>(maxLineLength) * charAdvance;
        const float contentHeight = static_cast<float>(lines.size()) * lineHeight;

        const float textStartX = static_cast<float>(screenWidth) - m_marginX - contentWidth;
        const float textStartY = m_marginY;

        m_boundsMinX = textStartX;
        m_boundsMinY = textStartY;
        m_boundsMaxX = textStartX + contentWidth;
        m_boundsMaxY = textStartY + contentHeight;

        constexpr float textDepth = 0.20f;

        // 1. Transparent background generation (commented out for evaluation)
        /*
        const float padY = 1.0f * m_scale;
        constexpr float bgDepth = 0.25f;
        float currentY = textStartY;
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

                    const float quadX      = textStartX + static_cast<float>(startCol) * charAdvance;
                    const float quadY      = currentY + padY;
                    const float quadWidth  = static_cast<float>(runLength) * charAdvance;
                    const float quadHeight = lineHeight - 2.0f * padY;

                    AppendQuad(outVertices, outIndices, quadX, quadY, bgDepth, quadWidth, quadHeight, m_backgroundColor);
                }
                else
                {
                    ++col;
                }
            }
            currentY += lineHeight;
        }
        */

        // 2. Generate character glyph quads
        float currentY = textStartY;
        for (const auto& line : lines)
        {
            float currentX = textStartX;
            for (char ch : line)
            {
                if (ch != ' ')
                {
                    AppendCharacter(outVertices, outIndices, ch, currentX, currentY, textDepth, m_scale, m_textColor);
                }
                currentX += charAdvance;
            }
            currentY += lineHeight;
        }
    }

    void TextOverlay::Update(
        UINT frameIndex,
        const OverlayStatistics& stats,
        uint32_t screenWidth,
        uint32_t screenHeight
    )
    {
        if (!m_isInitialised || frameIndex >= BufferCount)
        {
            return;
        }

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        vertices.reserve(4096);
        indices.reserve(8192);

        BuildGeometry(stats, screenWidth, screenHeight, vertices, indices);

        if (vertices.empty() || indices.empty())
        {
            m_indexCount[frameIndex] = 0;
            return;
        }

        const size_t vbBytes = vertices.size() * sizeof(Vertex);
        const size_t ibBytes = indices.size() * sizeof(uint32_t);

        // Upload to per-frame vertex and index buffers
        D3D12_RANGE readRange{ 0, 0 };
        void* mappedVb = nullptr;
        HR_CHECK(m_vertexBuffer[frameIndex]->Map(0, &readRange, &mappedVb));
        std::memcpy(mappedVb, vertices.data(), vbBytes);
        m_vertexBuffer[frameIndex]->Unmap(0, nullptr);

        void* mappedIb = nullptr;
        HR_CHECK(m_indexBuffer[frameIndex]->Map(0, &readRange, &mappedIb));
        std::memcpy(mappedIb, indices.data(), ibBytes);
        m_indexBuffer[frameIndex]->Unmap(0, nullptr);

        m_indexCount[frameIndex] = static_cast<uint32_t>(indices.size());
    }

    void TextOverlay::Render(
        ID3D12GraphicsCommandList* commandList,
        UINT frameIndex,
        uint32_t screenWidth,
        uint32_t screenHeight,
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle
    )
    {
        if (!m_isInitialised || frameIndex >= BufferCount || m_indexCount[frameIndex] == 0)
        {
            return;
        }

        // 1. Update Constant Buffer for 2D screen-space pixel projection
        SceneConstantBuffer cb{};
        cb.mvp            = Maths::Mat4x4::OrthographicPixelSpace(static_cast<float>(screenWidth), static_cast<float>(screenHeight));
        cb.world          = Maths::Mat4x4::Identity();
        cb.lightDirection = Maths::Vec4::Zero();
        cb.lightColor     = Maths::Vec4::Zero();
        cb.ambientColor   = Maths::Vec4::One(); // Unlit, 100% vertex colour
        m_constantBuffer.Update(cb, frameIndex);

        // 2. Set viewport covering full screen and scissor rect clamped to overlay bounds
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

        // 3. Clear depth stencil only within the text overlay scissor bounds
        commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 1, &scissor);

        // 4. Bind resources and issue draw call
        commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffer.GetGpuVirtualAddress(frameIndex));
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView[frameIndex]);
        commandList->IASetIndexBuffer(&m_indexBufferView[frameIndex]);
        commandList->DrawIndexedInstanced(m_indexCount[frameIndex], 1, 0, 0, 0);
    }
}

