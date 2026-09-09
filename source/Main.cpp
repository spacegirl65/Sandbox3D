// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "GraphicsEngine.h"

#include <iostream>

int main(int argc, char* argv[])
{
    try
    {
        std::wcout << L"Initialising Sandbox3D Graphics Subsystem...\n";

        Sandbox3D::GraphicsEngine engine;
        engine.Initialise(/* enableDebugLayer = */ true);

        std::wcout << L"Graphics Engine initialisation complete.\n";
        std::wcout << L"Active GPU: " << engine.GetGpuDescription() << L"\n";

        // Foundation successfully verified; clean up upon scope exit
        engine.Shutdown();
    }
    catch (const Sandbox3D::DxException& ex)
    {
        std::wcerr << L"[EXCEPTION] " << ex.GetErrorMessage() << L"\n";
        return -1;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[EXCEPTION] " << ex.what() << "\n";
        return -1;
    }

    return 0;
}

