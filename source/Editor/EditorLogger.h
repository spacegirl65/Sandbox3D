// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Core/Logger.h"

#include <mutex>

namespace Sandbox3D::Editor
{
    class EditorWindow;

    // Logger sink routing engine messages into the built-in Editor Terminal panel
    class EditorLogger final : public Core::Logger
    {
    public:
        explicit EditorLogger(EditorWindow* editorWindow = nullptr) noexcept;
        ~EditorLogger() override = default;

        EditorLogger(const EditorLogger&) = delete;
        EditorLogger& operator=(const EditorLogger&) = delete;
        EditorLogger(EditorLogger&&) noexcept = delete;
        EditorLogger& operator=(EditorLogger&&) noexcept = delete;

        void SetEditorWindow(EditorWindow* editorWindow) noexcept { m_editorWindow = editorWindow; }
        void Log(Core::LogLevel level, std::string_view message) override;

    private:
        EditorWindow* m_editorWindow{ nullptr };
        std::mutex    m_outputMutex;
    };
}
