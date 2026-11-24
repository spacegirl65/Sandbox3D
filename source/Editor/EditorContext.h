// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include <cstdint>
#include <functional>

namespace Sandbox3D::Engine
{
    class Scene;
}

namespace Sandbox3D::Editor
{
    // Simulation playback state displayed by the Editor viewport toolbar
    enum class PlayState
    {
        Stopped,
        Playing,
        Paused
    };

    using PlayStateCallback = std::function<void(PlayState)>;

    enum class CreateObjectKind : int
    {
        EmptyObject   = 0,
        Body          = 1,
        Camera        = 2,
        Character     = 3,
        Light         = 4,
        TerrainObject = 5
    };

    // Shared simulation/selection state passed by reference across editor panels
    struct EditorContext
    {
        Engine::Scene* scene{ nullptr };
        uint32_t       selectedObjectId{ 0 };
    };
}
