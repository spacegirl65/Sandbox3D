// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Core/Application.h"
#include "DxCheck.h"

#include <iostream>

int main(int argc, char* argv[])
{
    try
    {
        std::wcout << L"Starting Sandbox3D DirectX 12 Native Application...\n";

        Sandbox3D::Core::Application app(1280, 720, L"Sandbox3D - DirectX 12 Red Triangle");
        return app.Run();
    }
    catch (const Sandbox3D::DxException& ex)
    {
        std::wcerr << L"[DX_EXCEPTION] " << ex.GetErrorMessage() << L"\n";
        return -1;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[STD_EXCEPTION] " << ex.what() << "\n";
        return -1;
    }

    return 0;
}

