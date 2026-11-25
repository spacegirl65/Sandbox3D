// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "EditorContext.h"

struct ImFont;

namespace Sandbox3D::Editor
{
    // Renders the Object Inspector panel for the currently selected scene object
    class ObjectInspectorPanel final
    {
    public:
        void Draw(EditorContext& context, bool& isOpen, ImFont* italicFont = nullptr);
    };
}
