// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "EditorContext.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>

struct ImFont;

namespace Sandbox3D::Engine
{
    class Base;
}

namespace Sandbox3D::Editor
{
    // Renders and manages the Scene Hierarchy panel, object creation, filtering, and selection
    class SceneHierarchyPanel final
    {
    public:
        void Draw(EditorContext& context, bool& isOpen, ImFont* italicFont);
        void CreateObject(EditorContext& context, CreateObjectKind kind, const std::shared_ptr<Engine::Base>& parent = nullptr);
        void ResetTransientState() noexcept;

    private:
        void DrawNode(
            EditorContext& context,
            const std::shared_ptr<Engine::Base>& object,
            std::string_view filterView,
            uint32_t& pendingDeleteId
        );
        void BeginRename(const Engine::Base& object);

    private:
        std::array<char, 128> m_filter{};
        uint32_t              m_renamingObjectId{ 0 };
        bool                  m_focusRenameInput{ false };
        std::array<char, 128> m_renameBuffer{};
    };
}
