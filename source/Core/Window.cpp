// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Window.h"
#include "resource.h"

#include <stdexcept>
#include <format>
#include <filesystem>
#include <wincodec.h>
#include <wrl/client.h>

#pragma comment(lib, "windowscodecs.lib")

namespace Sandbox3D::Core
{
    std::wstring Window::FormatTitle(uint32_t width, uint32_t height)
    {
        return std::format(L"Sandbox3D - [DX12, {} x {}]", width, height);
    }

    void Window::SetTitle(const std::wstring& title)
    {
        m_title = title;
        if (m_hwnd)
        {
            SetWindowTextW(m_hwnd, m_title.c_str());
        }
    }

    Window::Window(uint32_t width, uint32_t height, const std::wstring& title)
        : m_title(title.empty() ? FormatTitle(width, height) : title)
        , m_className(L"Sandbox3D_WindowClass")
        , m_width(width)
        , m_height(height)
        , m_hinstance(GetModuleHandleW(nullptr))
        , m_autoUpdateTitleDimensions(title.empty())
    {
        // Enable per-monitor DPI awareness
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        // Load custom application icon from icon.png or embedded resources
        const std::wstring iconPath = FindIconFilePath(L"icon.png");
        const int bigIconWidth      = GetSystemMetrics(SM_CXICON);
        const int bigIconHeight     = GetSystemMetrics(SM_CYICON);
        const int smallIconWidth    = GetSystemMetrics(SM_CXSMICON);
        const int smallIconHeight   = GetSystemMetrics(SM_CYSMICON);

        if (!iconPath.empty())
        {
            m_hIconBig   = CreateIconFromPng(iconPath, bigIconWidth, bigIconHeight);
            m_hIconSmall = CreateIconFromPng(iconPath, smallIconWidth, smallIconHeight);
        }

        // Fallback to embedded module resource icon if PNG creation was not available
        if (!m_hIconBig)
        {
            m_hIconBig = LoadIconW(m_hinstance, MAKEINTRESOURCEW(IDI_APP_ICON));
        }
        if (!m_hIconSmall)
        {
            m_hIconSmall = static_cast<HICON>(LoadImageW(
                m_hinstance,
                MAKEINTRESOURCEW(IDI_APP_ICON),
                IMAGE_ICON,
                smallIconWidth,
                smallIconHeight,
                LR_DEFAULTCOLOR
            ));
        }

        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(WNDCLASSEXW);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = WindowProcSetup;
        wc.hInstance     = m_hinstance;
        wc.hIcon         = m_hIconBig;
        wc.hIconSm       = m_hIconSmall;
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        wc.lpszClassName = m_className.c_str();

        if (!RegisterClassExW(&wc))
        {
            throw std::runtime_error("Failed to register Win32 window class.");
        }

        // Calculate client area dimensions
        RECT wr = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
        constexpr DWORD windowStyle = (WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX));
        AdjustWindowRect(&wr, windowStyle, FALSE);

        m_windowWidth  = wr.right - wr.left;
        m_windowHeight = wr.bottom - wr.top;

        m_hwnd = CreateWindowExW(
            0,
            m_className.c_str(),
            m_title.c_str(),
            windowStyle,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            m_windowWidth,
            m_windowHeight,
            nullptr,
            nullptr,
            m_hinstance,
            this
        );

        if (!m_hwnd)
        {
            UnregisterClassW(m_className.c_str(), m_hinstance);
            throw std::runtime_error("Failed to create Win32 native window.");
        }

        if (m_hIconBig)
        {
            SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(m_hIconBig));
        }
        if (m_hIconSmall)
        {
            SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(m_hIconSmall));
        }

        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);

        ApplyTerminalTitle();
    }

    Window::~Window()
    {
        if (m_hwnd)
        {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }

        if (m_hIconBig)
        {
            DestroyIcon(m_hIconBig);
            m_hIconBig = nullptr;
        }

        if (m_hIconSmall)
        {
            DestroyIcon(m_hIconSmall);
            m_hIconSmall = nullptr;
        }

        if (m_hinstance)
        {
            UnregisterClassW(m_className.c_str(), m_hinstance);
        }
    }

    bool Window::ProcessMessages()
    {
        MSG msg = {};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                m_isRunning = false;
                return false;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        return m_isRunning;
    }

    LRESULT CALLBACK Window::WindowProcSetup(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        if (uMsg == WM_NCCREATE)
        {
            const auto* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
            auto* pWnd = static_cast<Window*>(pCreate->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWnd));
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WindowProcThunk));
            return pWnd->HandleMessage(hwnd, uMsg, wParam, lParam);
        }

        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    LRESULT CALLBACK Window::WindowProcThunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        auto* pWnd = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (pWnd)
        {
            return pWnd->HandleMessage(hwnd, uMsg, wParam, lParam);
        }

        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    LRESULT Window::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
        case WM_GETMINMAXINFO:
        {
            if (m_windowWidth > 0 && m_windowHeight > 0)
            {
                auto* pMinMax = reinterpret_cast<MINMAXINFO*>(lParam);
                pMinMax->ptMinTrackSize.x = m_windowWidth;
                pMinMax->ptMinTrackSize.y = m_windowHeight;
                pMinMax->ptMaxTrackSize.x = m_windowWidth;
                pMinMax->ptMaxTrackSize.y = m_windowHeight;
                return 0;
            }
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
        }

        case WM_SIZE:
        {
            const uint32_t newWidth  = LOWORD(lParam);
            const uint32_t newHeight = HIWORD(lParam);

            if (wParam == SIZE_MINIMIZED)
            {
                m_isMinimized = true;
            }
            else
            {
                m_isMinimized = false;
                if ((newWidth != m_width || newHeight != m_height) && newWidth > 0 && newHeight > 0)
                {
                    m_width  = newWidth;
                    m_height = newHeight;
                    if (m_autoUpdateTitleDimensions)
                    {
                        SetTitle(FormatTitle(m_width, m_height));
                    }
                    if (m_resizeCallback)
                    {
                        m_resizeCallback(m_width, m_height);
                    }
                }
            }
            return 0;
        }

        case WM_SETFOCUS:
            m_isFocused = true;
            ApplyTerminalTitle();
            return 0;

        case WM_KILLFOCUS:
            m_isFocused = false;
            ApplyTerminalTitle();
            return 0;

        case WM_ACTIVATE:
            m_isFocused = (LOWORD(wParam) != WA_INACTIVE);
            ApplyTerminalTitle();
            return 0;

        case WM_CLOSE:
            m_isRunning = false;
            CloseTerminalWindow();
            PostQuitMessage(0);
            return 0;

        case WM_DESTROY:
            m_isRunning = false;
            CloseTerminalWindow();
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
        }
    }

    void Window::CloseTerminalWindow() noexcept
    {
        // Send simulated Enter key to satisfy any pending console input or batch pause
        HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
        if (hStdIn != INVALID_HANDLE_VALUE && hStdIn != nullptr)
        {
            INPUT_RECORD ir[2] = {};
            ir[0].EventType = KEY_EVENT;
            ir[0].Event.KeyEvent.bKeyDown = TRUE;
            ir[0].Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
            ir[0].Event.KeyEvent.uChar.UnicodeChar = L'\r';
            ir[0].Event.KeyEvent.wRepeatCount = 1;

            ir[1].EventType = KEY_EVENT;
            ir[1].Event.KeyEvent.bKeyDown = FALSE;
            ir[1].Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
            ir[1].Event.KeyEvent.uChar.UnicodeChar = L'\r';
            ir[1].Event.KeyEvent.wRepeatCount = 1;

            DWORD written = 0;
            WriteConsoleInputW(hStdIn, ir, 2, &written);
        }

        // Terminate any launcher process (such as cmd.exe waiting to run "pause") attached to this console
        DWORD processIds[32] = {};
        const DWORD count = GetConsoleProcessList(processIds, 32);
        const DWORD currentPid = GetCurrentProcessId();

        for (DWORD i = 0; i < count; ++i)
        {
            if (processIds[i] != currentPid && processIds[i] != 0)
            {
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, processIds[i]);
                if (hProc)
                {
                    TerminateProcess(hProc, 0);
                    CloseHandle(hProc);
                }
            }
        }

        // Post WM_CLOSE to the console window itself
        if (HWND consoleHwnd = GetConsoleWindow())
        {
            PostMessageW(consoleHwnd, WM_CLOSE, 0, 0);
        }

        // Detach from console
        FreeConsole();
    }

    void Window::SetTerminalTitle(const std::wstring& title) noexcept
    {
        s_terminalTitle = title;
        ApplyTerminalTitle();
    }

    const std::wstring& Window::GetTerminalTitle() noexcept
    {
        return s_terminalTitle;
    }

    void Window::ApplyTerminalTitle() noexcept
    {
        if (s_terminalTitle.empty())
        {
            return;
        }

        // Set title via Win32 Console API for conhost and ConPTY synchronization
        SetConsoleTitleW(s_terminalTitle.c_str());

        // Emit Virtual Terminal OSC sequences to explicitly set window and tab title in modern terminals
        HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hStdOut != INVALID_HANDLE_VALUE && hStdOut != nullptr)
        {
            DWORD consoleMode = 0;
            if (GetConsoleMode(hStdOut, &consoleMode))
            {
                if (!(consoleMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING))
                {
                    SetConsoleMode(hStdOut, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
                }

                // OSC 0 sets icon name and window title; OSC 2 sets window title
                const std::wstring oscSequence = std::format(L"\x1b]0;{}\x07\x1b]2;{}\x07", s_terminalTitle, s_terminalTitle);
                DWORD written = 0;
                WriteConsoleW(hStdOut, oscSequence.c_str(), static_cast<DWORD>(oscSequence.length()), &written, nullptr);
            }
        }
    }

    bool Window::SetIcon(const std::wstring& iconPath)
    {
        const std::wstring resolvedPath = FindIconFilePath(iconPath);
        if (resolvedPath.empty())
        {
            return false;
        }

        const int bigIconWidth    = GetSystemMetrics(SM_CXICON);
        const int bigIconHeight   = GetSystemMetrics(SM_CYICON);
        const int smallIconWidth  = GetSystemMetrics(SM_CXSMICON);
        const int smallIconHeight = GetSystemMetrics(SM_CYSMICON);

        HICON newBigIcon   = CreateIconFromPng(resolvedPath, bigIconWidth, bigIconHeight);
        HICON newSmallIcon = CreateIconFromPng(resolvedPath, smallIconWidth, smallIconHeight);

        if (!newBigIcon && !newSmallIcon)
        {
            return false;
        }

        if (m_hIconBig)
        {
            DestroyIcon(m_hIconBig);
        }
        if (m_hIconSmall)
        {
            DestroyIcon(m_hIconSmall);
        }

        m_hIconBig   = newBigIcon;
        m_hIconSmall = newSmallIcon;

        if (m_hwnd)
        {
            if (m_hIconBig)
            {
                SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(m_hIconBig));
            }
            if (m_hIconSmall)
            {
                SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(m_hIconSmall));
            }
        }

        return true;
    }

    std::wstring Window::FindIconFilePath(const std::wstring& filename)
    {
        namespace fs = std::filesystem;

        // Check relative path directly in the active working directory
        if (fs::exists(filename))
        {
            return filename;
        }

        // Check directory hierarchy relative to the executing module binary
        wchar_t exeBuffer[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, exeBuffer, MAX_PATH) > 0)
        {
            const fs::path exeDir = fs::path(exeBuffer).parent_path();
            if (fs::exists(exeDir / filename))
            {
                return (exeDir / filename).wstring();
            }
            if (fs::exists(exeDir.parent_path() / filename))
            {
                return (exeDir.parent_path() / filename).wstring();
            }
            if (fs::exists(exeDir.parent_path().parent_path() / filename))
            {
                return (exeDir.parent_path().parent_path() / filename).wstring();
            }
        }

        return {};
    }

    HICON Window::CreateIconFromPng(const std::wstring& path, int targetWidth, int targetHeight)
    {
        if (path.empty() || !std::filesystem::exists(path) || targetWidth <= 0 || targetHeight <= 0)
        {
            return nullptr;
        }

        // Ensure COM subsystem is initialised on the current execution thread
        const HRESULT hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const bool shouldUninitialize = (hrCom == S_OK);

        HICON hIcon = nullptr;

        do
        {
            Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
            HRESULT hr = CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&factory)
            );
            if (FAILED(hr) || !factory)
            {
                break;
            }

            Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
            hr = factory->CreateDecoderFromFilename(
                path.c_str(),
                nullptr,
                GENERIC_READ,
                WICDecodeMetadataCacheOnDemand,
                &decoder
            );
            if (FAILED(hr) || !decoder)
            {
                break;
            }

            Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
            hr = decoder->GetFrame(0, &frame);
            if (FAILED(hr) || !frame)
            {
                break;
            }

            // Rescale frame to target dimensions using high-quality Fant interpolation
            Microsoft::WRL::ComPtr<IWICBitmapScaler> scaler;
            hr = factory->CreateBitmapScaler(&scaler);
            if (FAILED(hr) || !scaler)
            {
                break;
            }

            hr = scaler->Initialize(
                frame.Get(),
                static_cast<UINT>(targetWidth),
                static_cast<UINT>(targetHeight),
                WICBitmapInterpolationModeFant
            );
            if (FAILED(hr))
            {
                break;
            }

            // Convert pixel format to 32-bit BGRA with alpha transparency
            Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
            hr = factory->CreateFormatConverter(&converter);
            if (FAILED(hr) || !converter)
            {
                break;
            }

            hr = converter->Initialize(
                scaler.Get(),
                GUID_WICPixelFormat32bppBGRA,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0,
                WICBitmapPaletteTypeCustom
            );
            if (FAILED(hr))
            {
                break;
            }

            // Create top-down 32-bit colour DIB section
            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth       = targetWidth;
            bmi.bmiHeader.biHeight      = -static_cast<LONG>(targetHeight);
            bmi.bmiHeader.biPlanes      = 1;
            bmi.bmiHeader.biBitCount    = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            void* bits = nullptr;
            HBITMAP hColorBitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
            if (!hColorBitmap || !bits)
            {
                break;
            }

            const UINT stride = static_cast<UINT>(targetWidth * 4);
            const UINT bufferSize = static_cast<UINT>(stride * targetHeight);
            hr = converter->CopyPixels(nullptr, stride, bufferSize, static_cast<BYTE*>(bits));
            if (FAILED(hr))
            {
                DeleteObject(hColorBitmap);
                break;
            }

            // Create 1-bit monochrome mask bitmap required by CreateIconIndirect
            HBITMAP hMaskBitmap = CreateBitmap(targetWidth, targetHeight, 1, 1, nullptr);
            if (!hMaskBitmap)
            {
                DeleteObject(hColorBitmap);
                break;
            }

            ICONINFO iconInfo = {};
            iconInfo.fIcon    = TRUE;
            iconInfo.xHotspot = 0;
            iconInfo.yHotspot = 0;
            iconInfo.hbmMask  = hMaskBitmap;
            iconInfo.hbmColor = hColorBitmap;

            hIcon = CreateIconIndirect(&iconInfo);

            DeleteObject(hColorBitmap);
            DeleteObject(hMaskBitmap);
        } while (false);

        if (shouldUninitialize)
        {
            CoUninitialize();
        }

        return hIcon;
    }
}

