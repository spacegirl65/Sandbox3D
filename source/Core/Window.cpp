// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Window.h"

#include <stdexcept>
#include <format>

namespace Sandbox3D::Core
{
    Window::Window(uint32_t width, uint32_t height, const std::wstring& title)
        : m_title(title)
        , m_className(title + L"_WindowClass")
        , m_width(width)
        , m_height(height)
        , m_hinstance(GetModuleHandleW(nullptr))
    {
        // Enable per-monitor DPI awareness
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(WNDCLASSEXW);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = WindowProcSetup;
        wc.hInstance     = m_hinstance;
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
        // 1. Send simulated Enter key to satisfy any pending console input or batch pause
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

        // 2. Terminate any launcher process (such as cmd.exe waiting to run "pause") attached to this console
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

        // 3. Post WM_CLOSE to the console window itself
        if (HWND consoleHwnd = GetConsoleWindow())
        {
            PostMessageW(consoleHwnd, WM_CLOSE, 0, 0);
        }

        // 4. Detach from console
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

        // 1. Set title via Win32 Console API for conhost and ConPTY synchronization
        SetConsoleTitleW(s_terminalTitle.c_str());

        // 2. Emit Virtual Terminal OSC sequences to explicitly set window and tab title in modern terminals
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
}

