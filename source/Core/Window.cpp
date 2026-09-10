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
        constexpr DWORD windowStyle = WS_OVERLAPPEDWINDOW;
        AdjustWindowRect(&wr, windowStyle, FALSE);

        const int windowWidth  = wr.right - wr.left;
        const int windowHeight = wr.bottom - wr.top;

        m_hwnd = CreateWindowExW(
            0,
            m_className.c_str(),
            m_title.c_str(),
            windowStyle,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            windowWidth,
            windowHeight,
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

        case WM_CLOSE:
            m_isRunning = false;
            PostQuitMessage(0);
            return 0;

        case WM_DESTROY:
            m_isRunning = false;
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
        }
    }
}

