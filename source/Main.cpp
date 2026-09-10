// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Core/Application.h"
#include "DxCheck.h"

#include <iostream>

int main()
{
    try
    {
        std::wcout << L"Starting Sandbox3D DirectX 12 Native Application...\n";

        Sandbox3D::Core::Application app(2880, 1200, L"Sandbox3D - [DX12, 2880 x 1200]");
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
}

