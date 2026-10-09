// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "DebugOverlay.h"

#include <format>
#include <algorithm>
#include <cctype>

namespace Sandbox3D::Engine
{
    DebugOverlay::DebugOverlay()
        : m_textOverlay(std::make_unique<Renderer::TextOverlay>())
    {
    }

    DebugOverlay::~DebugOverlay()
    {
        Shutdown();
    }

    void DebugOverlay::Initialise(ID3D12Device* device)
    {
        if (m_textOverlay)
        {
            m_textOverlay->Initialise(device);
            m_textOverlay->SetScale(m_scale);
            m_textOverlay->SetTextColor(m_textColor);
            m_textOverlay->SetBackgroundColor(m_backgroundColor);
            m_textOverlay->SetDropShadow(m_dropShadowEnabled);
            m_textOverlay->SetShadowColor(m_shadowColor);
            m_textOverlay->SetShadowOffset(m_shadowOffset);
        }
    }

    void DebugOverlay::Shutdown() noexcept
    {
        if (m_textOverlay)
        {
            m_textOverlay->Shutdown();
        }
    }

    bool DebugOverlay::IsInitialised() const noexcept
    {
        return m_textOverlay && m_textOverlay->IsInitialised();
    }

    bool DebugOverlay::IsVisible() const noexcept
    {
        return m_textOverlay && m_textOverlay->IsVisible();
    }

    void DebugOverlay::SetVisible(bool visible) noexcept
    {
        if (m_textOverlay)
        {
            m_textOverlay->SetVisible(visible);
        }
        for (bool& dirty : m_dirtyBuffer)
        {
            dirty = true;
        }
        m_cachedLeftLines.clear();
        m_cachedRightLines.clear();
    }

    void DebugOverlay::ToggleVisibility() noexcept
    {
        if (m_textOverlay)
        {
            m_textOverlay->ToggleVisibility();
        }
        for (bool& dirty : m_dirtyBuffer)
        {
            dirty = true;
        }
        m_cachedLeftLines.clear();
        m_cachedRightLines.clear();
    }

    void DebugOverlay::SetScale(float scale) noexcept
    {
        m_scale = scale;
        if (m_textOverlay)
        {
            m_textOverlay->SetScale(scale);
        }
        for (bool& dirty : m_dirtyBuffer)
        {
            dirty = true;
        }
        m_cachedLeftLines.clear();
        m_cachedRightLines.clear();
    }

    void DebugOverlay::SetTextColor(const Vec4& color) noexcept
    {
        m_textColor = color;
        if (m_textOverlay)
        {
            m_textOverlay->SetTextColor(color);
        }
        for (bool& dirty : m_dirtyBuffer)
        {
            dirty = true;
        }
        m_cachedLeftLines.clear();
        m_cachedRightLines.clear();
    }

    void DebugOverlay::SetBackgroundColor(const Vec4& color) noexcept
    {
        m_backgroundColor = color;
        if (m_textOverlay)
        {
            m_textOverlay->SetBackgroundColor(color);
        }
        for (bool& dirty : m_dirtyBuffer)
        {
            dirty = true;
        }
        m_cachedLeftLines.clear();
        m_cachedRightLines.clear();
    }

    void DebugOverlay::SetDropShadow(bool enabled) noexcept
    {
        m_dropShadowEnabled = enabled;
        if (m_textOverlay)
        {
            m_textOverlay->SetDropShadow(enabled);
        }
        for (bool& dirty : m_dirtyBuffer)
        {
            dirty = true;
        }
        m_cachedLeftLines.clear();
        m_cachedRightLines.clear();
    }

    void DebugOverlay::SetShadowColor(const Vec4& color) noexcept
    {
        m_shadowColor = color;
        if (m_textOverlay)
        {
            m_textOverlay->SetShadowColor(color);
        }
        for (bool& dirty : m_dirtyBuffer)
        {
            dirty = true;
        }
        m_cachedLeftLines.clear();
        m_cachedRightLines.clear();
    }

    void DebugOverlay::SetShadowOffset(const Vec2& offset) noexcept
    {
        m_shadowOffset = offset;
        if (m_textOverlay)
        {
            m_textOverlay->SetShadowOffset(offset);
        }
        for (bool& dirty : m_dirtyBuffer)
        {
            dirty = true;
        }
        m_cachedLeftLines.clear();
        m_cachedRightLines.clear();
    }

    std::string DebugOverlay::FormatWithCommas(size_t value)
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

    std::string DebugOverlay::TruncateDeviceName(std::string name, size_t maxLength)
    {
        // Strip redundant legal, marketing, and hardware suffixes whilst preserving mobile qualifiers
        for (const std::string& token : { "(R)", "(TM)", "Corporation", " Graphics", " GPU" })
        {
            size_t pos = 0;
            while ((pos = name.find(token, pos)) != std::string::npos)
            {
                name.erase(pos, token.length());
            }
        }

        // Strip verbose vendor and series prefixes to display concise model identifier
        constexpr std::string_view nvidiaGeforce = "NVIDIA GeForce ";
        constexpr std::string_view nvidia        = "NVIDIA ";
        constexpr std::string_view amdRadeon     = "AMD Radeon ";
        constexpr std::string_view intel         = "Intel ";
        constexpr std::string_view microsoft     = "Microsoft ";

        if (name.starts_with(nvidiaGeforce))
        {
            name.erase(0, nvidiaGeforce.length());
        }
        else if (name.starts_with(nvidia))
        {
            name.erase(0, nvidia.length());
        }
        else if (name.starts_with(amdRadeon))
        {
            name.erase(0, amdRadeon.length());
        }
        else if (name.starts_with(intel))
        {
            name.erase(0, intel.length());
        }
        else if (name.starts_with(microsoft))
        {
            name.erase(0, microsoft.length());
        }

        // Collapse multiple consecutive spaces and trim whitespace
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

        // Enforce maximum length whilst preserving laptop designation if present
        if (name.length() > maxLength)
        {
            const std::string lower = [](std::string s) {
                for (char& c : s)
                {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                return s;
            }(name);

            constexpr size_t laptopTokenLength = 6;
            const size_t laptopPos = lower.find("laptop");
            if (laptopPos != std::string::npos && laptopPos < maxLength + laptopTokenLength)
            {
                const size_t endOfLaptop = laptopPos + laptopTokenLength;
                name = name.substr(0, std::max(maxLength, endOfLaptop));
            }
            else
            {
                name = name.substr(0, maxLength);
            }

            while (!name.empty() && name.back() == ' ')
            {
                name.pop_back();
            }
        }

        return name;
    }

    void DebugOverlay::FormatMetrics(
        const OverlayStatistics& stats,
        uint32_t screenWidth,
        uint32_t screenHeight,
        std::vector<std::string>& outLeftLines,
        std::vector<std::string>& outRightLines
    ) const
    {
        outLeftLines.clear();
        outRightLines.clear();

        // Left block: hardware, presentation, execution frequencies, and scene complexity
        if (!stats.gpuName.empty())
        {
            outLeftLines.push_back(std::format("GPU:    {}", TruncateDeviceName(stats.gpuName)));
        }
        else
        {
            outLeftLines.push_back("GPU:    Unknown");
        }
        outLeftLines.push_back("API:    DirectX 12");
        outLeftLines.push_back(std::format("Res:    {} x {}", screenWidth, screenHeight));
        outLeftLines.push_back(std::format("Mode:   {}", stats.windowMode.empty() ? "Windowed" : stats.windowMode));

        outLeftLines.push_back("");

        outLeftLines.push_back(std::format("FPS:    {:.1f}", stats.fps));
        const float targetUps = (stats.targetUps > 0.0f) ? stats.targetUps : 60.0f;
        const float upsRatio = std::clamp(stats.ups / targetUps, 0.0f, 1.0f);
        outLeftLines.push_back(std::format("UPS:    {:.2f}", upsRatio));

        outLeftLines.push_back("");

        outLeftLines.push_back("[Scene]");
        outLeftLines.push_back(std::format("Objects: {}", stats.objectCount > 0 ? stats.objectCount : stats.itemCount));
        outLeftLines.push_back(std::format("Lights:  {}", stats.lightCount));
        outLeftLines.push_back(std::format("Verts:   {}", FormatWithCommas(stats.vertexCount)));
        outLeftLines.push_back(std::format("Tris:    {}", FormatWithCommas(stats.triangleCount)));

        // Right block: dynamic memory utilisation, frame pipeline dispatch, anti-aliasing, and camera coordinates
        constexpr double bytesToMb = 1024.0 * 1024.0;
        constexpr double bytesToKb = 1024.0;

        if (stats.vramLocalBudgetBytes > 0)
        {
            const double usedMb = static_cast<double>(stats.vramLocalUsedBytes) / bytesToMb;
            const double budgetMb = static_cast<double>(stats.vramLocalBudgetBytes) / bytesToMb;
            outRightLines.push_back(std::format("VRAM:    {:.0f} / {:.0f} MB", usedMb, budgetMb));
        }
        else if (stats.vramLocalUsedBytes > 0)
        {
            const double usedMb = static_cast<double>(stats.vramLocalUsedBytes) / bytesToMb;
            outRightLines.push_back(std::format("VRAM:    {:.0f} MB", usedMb));
        }
        else
        {
            outRightLines.push_back("VRAM:    Active");
        }

        const double uploadKb = static_cast<double>(stats.uploadUsedBytes) / bytesToKb;
        outRightLines.push_back(std::format("Upload:  {:.1f} KB", uploadKb));

        outRightLines.push_back("");

        outRightLines.push_back(std::format("Frame:   {:.2f} ms", stats.frameTimeMs));
        outRightLines.push_back(std::format("Draws:   {}", stats.drawCallCount));
        outRightLines.push_back(std::format("Batches: {}", stats.batchCount));
        outRightLines.push_back(std::format("Culled:  {}", stats.culledItemCount));

        outRightLines.push_back("");

        const std::string aaMethod = stats.aaDescription.empty() ? "MSAA 4x" : stats.aaDescription;
        const std::string_view vsyncSuffix = stats.vSync ? ", VSync On" : ", VSync Off";
        outRightLines.push_back(std::format("{}{}", aaMethod, vsyncSuffix));

        outRightLines.push_back("");

        // Wrap Cartesian camera spatial coordinates across up to three lines when exceeding block limits
        size_t leftMaxLen = 0;
        for (const auto& line : outLeftLines)
        {
            leftMaxLen = std::max(leftMaxLen, line.length());
        }

        const float charAdvance = m_textOverlay ? m_textOverlay->GetCharAdvance(m_scale) : (8.0f * m_scale);
        const float leftWidth   = static_cast<float>(leftMaxLen) * charAdvance;
        constexpr float blockGutter = 32.0f;
        const float maxAvailableRightWidth = static_cast<float>(screenWidth) - (2.0f * m_marginX) - leftWidth - blockGutter;

        const size_t availableRightChars = (maxAvailableRightWidth > 0.0f && charAdvance > 0.0f)
            ? static_cast<size_t>(maxAvailableRightWidth / charAdvance)
            : MaxRightBlockCharacters;
        const size_t wrapCharacterLimit  = std::min(MaxRightBlockCharacters, availableRightChars);

        const std::string opt1 = std::format("X: {:.1f}  Y: {:.1f}  Z: {:.1f}",
            stats.cameraPosition.x, stats.cameraPosition.y, stats.cameraPosition.z);

        const std::string opt2Line1 = std::format("X: {:.1f}  Y: {:.1f}",
            stats.cameraPosition.x, stats.cameraPosition.y);
        const std::string opt2Line2 = std::format("Z: {:.1f}",
            stats.cameraPosition.z);

        const std::string opt3Line1 = std::format("X: {:.1f}",
            stats.cameraPosition.x);
        const std::string opt3Line2 = std::format("Y: {:.1f}",
            stats.cameraPosition.y);
        const std::string opt3Line3 = std::format("Z: {:.1f}",
            stats.cameraPosition.z);

        const size_t opt2MaxLen = std::max(opt2Line1.length(), opt2Line2.length());

        if (opt1.length() <= wrapCharacterLimit)
        {
            outRightLines.push_back(opt1);
        }
        else if (opt2MaxLen <= wrapCharacterLimit)
        {
            outRightLines.push_back(opt2Line1);
            outRightLines.push_back(opt2Line2);
        }
        else
        {
            outRightLines.push_back(opt3Line1);
            outRightLines.push_back(opt3Line2);
            outRightLines.push_back(opt3Line3);
        }
    }

    void DebugOverlay::Update(
        UINT frameIndex,
        const OverlayStatistics& stats,
        uint32_t screenWidth,
        uint32_t screenHeight
    )
    {
        if (!m_textOverlay || !m_textOverlay->IsInitialised() || frameIndex >= Renderer::TextOverlay::BufferCount)
        {
            return;
        }

        std::vector<std::string> leftLines;
        std::vector<std::string> rightLines;
        FormatMetrics(stats, screenWidth, screenHeight, leftLines, rightLines);

        const bool contentChanged = (leftLines != m_cachedLeftLines) ||
                                    (rightLines != m_cachedRightLines) ||
                                    (screenWidth != m_cachedScreenWidth) ||
                                    (screenHeight != m_cachedScreenHeight);

        if (contentChanged)
        {
            m_cachedLeftLines    = leftLines;
            m_cachedRightLines   = rightLines;
            m_cachedScreenWidth  = screenWidth;
            m_cachedScreenHeight = screenHeight;

            // Invalidate all frame buffers in flight so each receives updated geometry on its turn
            for (bool& dirty : m_dirtyBuffer)
            {
                dirty = true;
            }

            m_textOverlay->ResetGeometry();

            const float charAdvance = m_textOverlay->GetCharAdvance(m_scale);
            const float lineHeight  = m_textOverlay->GetLineHeight(m_scale);
            constexpr float blockGutter = 32.0f;

            size_t rightMaxLen = 0;
            for (const auto& line : rightLines)
            {
                rightMaxLen = std::max(rightMaxLen, line.length());
            }

            const size_t effectiveRightLen = std::max(rightMaxLen, MinRightBlockCharacters);

            size_t leftMaxLen = 0;
            for (const auto& line : leftLines)
            {
                leftMaxLen = std::max(leftMaxLen, line.length());
            }

            const float rightWidth = static_cast<float>(effectiveRightLen) * charAdvance;
            const float leftWidth  = static_cast<float>(leftMaxLen) * charAdvance;

            float rightStartX = static_cast<float>(screenWidth) - m_marginX - rightWidth;
            float leftStartX  = rightStartX - blockGutter - leftWidth;
            if (leftStartX < m_marginX)
            {
                leftStartX  = m_marginX;
                rightStartX = leftStartX + leftWidth + blockGutter;
            }
            const float textStartY  = m_marginY;

            const float maxTotalHeight = std::max(leftLines.size(), rightLines.size()) * lineHeight;

            m_textOverlay->DrawBlock(leftLines, leftStartX, textStartY, m_textColor, m_backgroundColor, m_scale);
            m_textOverlay->DrawBlock(rightLines, rightStartX, textStartY, m_textColor, m_backgroundColor, m_scale);

            const float shadowExtraX = m_dropShadowEnabled ? (m_shadowOffset.x * m_scale) : 0.0f;
            const float shadowExtraY = m_dropShadowEnabled ? (m_shadowOffset.y * m_scale) : 0.0f;

            m_textOverlay->SetScissorBounds(
                leftStartX,
                textStartY,
                rightStartX + rightWidth + shadowExtraX,
                textStartY + maxTotalHeight + shadowExtraY
            );
        }

        // Upload geometry to this frame's GPU buffer if dirty
        if (m_dirtyBuffer[frameIndex])
        {
            m_textOverlay->UploadBuffers(frameIndex);
            m_dirtyBuffer[frameIndex] = false;
        }
    }

    void DebugOverlay::Render(
        ID3D12GraphicsCommandList* commandList,
        UINT frameIndex,
        uint32_t screenWidth,
        uint32_t screenHeight
    )
    {
        if (m_textOverlay)
        {
            m_textOverlay->Render(commandList, frameIndex, screenWidth, screenHeight);
        }
    }
}

