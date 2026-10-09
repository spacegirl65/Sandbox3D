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
    }

    void DebugOverlay::ToggleVisibility() noexcept
    {
        if (m_textOverlay)
        {
            m_textOverlay->ToggleVisibility();
        }
    }

    void DebugOverlay::SetScale(float scale) noexcept
    {
        m_scale = scale;
        if (m_textOverlay)
        {
            m_textOverlay->SetScale(scale);
        }
    }

    void DebugOverlay::SetTextColor(const Vec4& color) noexcept
    {
        m_textColor = color;
        if (m_textOverlay)
        {
            m_textOverlay->SetTextColor(color);
        }
    }

    void DebugOverlay::SetBackgroundColor(const Vec4& color) noexcept
    {
        m_backgroundColor = color;
        if (m_textOverlay)
        {
            m_textOverlay->SetBackgroundColor(color);
        }
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
        for (const std::string& token : { "(R)", "(TM)", "Corporation", " Graphics" })
        {
            size_t pos = 0;
            while ((pos = name.find(token, pos)) != std::string::npos)
            {
                name.erase(pos, token.length());
            }
        }

        if (name.starts_with("NVIDIA GeForce "))
        {
            name.erase(0, 7); // Preserves "GeForce RTX ..."
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

        if (name.length() > maxLength)
        {
            const std::string lower = [](std::string s) {
                for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
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

        outRightLines.push_back(stats.aaDescription.empty() ? "MSAA 4x" : stats.aaDescription);

        outRightLines.push_back("");

        outRightLines.push_back(std::format("X: {:.1f}  Y: {:.1f}  Z: {:.1f}",
            stats.cameraPosition.x, stats.cameraPosition.y, stats.cameraPosition.z));
    }

    void DebugOverlay::Update(
        UINT frameIndex,
        const OverlayStatistics& stats,
        uint32_t screenWidth,
        uint32_t screenHeight
    )
    {
        if (!m_textOverlay || !m_textOverlay->IsInitialised())
        {
            return;
        }

        m_textOverlay->ResetGeometry();

        std::vector<std::string> leftLines;
        std::vector<std::string> rightLines;
        FormatMetrics(stats, screenWidth, screenHeight, leftLines, rightLines);

        const float charAdvance = m_textOverlay->GetCharAdvance(m_scale);
        const float lineHeight  = m_textOverlay->GetLineHeight(m_scale);
        constexpr float blockGutter = 32.0f;

        size_t rightMaxLen = 0;
        for (const auto& line : rightLines)
        {
            rightMaxLen = std::max(rightMaxLen, line.length());
        }

        size_t leftMaxLen = 0;
        for (const auto& line : leftLines)
        {
            leftMaxLen = std::max(leftMaxLen, line.length());
        }

        const float rightWidth = static_cast<float>(rightMaxLen) * charAdvance;
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

        m_textOverlay->SetScissorBounds(
            leftStartX,
            textStartY,
            rightStartX + rightWidth,
            textStartY + maxTotalHeight
        );

        m_textOverlay->UploadBuffers(frameIndex);
    }

    void DebugOverlay::Render(
        ID3D12GraphicsCommandList* commandList,
        UINT frameIndex,
        uint32_t screenWidth,
        uint32_t screenHeight,
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle
    )
    {
        if (m_textOverlay)
        {
            m_textOverlay->Render(commandList, frameIndex, screenWidth, screenHeight, dsvHandle);
        }
    }
}

