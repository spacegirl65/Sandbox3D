// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <functional>
#include <cstdint>

namespace Sandbox3D::Core
{
    // Encapsulates a Win32 Native Desktop Window with modern event handling
    class Window final
    {
    public:
        using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;

        Window(uint32_t width, uint32_t height, const std::wstring& title);
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&) noexcept = delete;
        Window& operator=(Window&&) noexcept = delete;

        // Message pump: processes pending Win32 messages; returns false if WM_QUIT was encountered
        bool ProcessMessages();

        [[nodiscard]] HWND GetHwnd() const noexcept { return m_hwnd; }
        [[nodiscard]] uint32_t GetWidth() const noexcept { return m_width; }
        [[nodiscard]] uint32_t GetHeight() const noexcept { return m_height; }
        [[nodiscard]] bool IsRunning() const noexcept { return m_isRunning; }
        [[nodiscard]] bool IsMinimized() const noexcept { return m_isMinimized; }

        void SetResizeCallback(ResizeCallback callback) { m_resizeCallback = std::move(callback); }

    private:
        static LRESULT CALLBACK WindowProcSetup(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
        static LRESULT CALLBACK WindowProcThunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    private:
        HWND           m_hwnd{ nullptr };
        HINSTANCE      m_hinstance{ nullptr };
        std::wstring   m_title;
        std::wstring   m_className;
        uint32_t       m_width{ 0 };
        uint32_t       m_height{ 0 };
        bool           m_isRunning{ true };
        bool           m_isMinimized{ false };
        ResizeCallback m_resizeCallback;
    };
}

