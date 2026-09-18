// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <comdef.h>
#include <exception>
#include <format>
#include <string>

namespace Sandbox3D
{
    // Exception class encapsulating HRESULT failures with source line and function diagnostics
    class DxException final : public std::exception
    {
    public:
        DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber)
            : m_errorCode(hr)
            , m_functionName(functionName)
            , m_filename(filename)
            , m_lineNumber(lineNumber)
        {
            _com_error err(hr);
            m_errorMessage = std::format(
                L"[DirectX Error] HRESULT: 0x{:08X} | Message: {} | Function: {} | File: {}:{}",
                static_cast<unsigned long>(hr),
                err.ErrorMessage(),
                m_functionName,
                m_filename,
                m_lineNumber
            );

            // Proper UTF-8 conversion for std::exception::what()
            const int sizeNeeded = WideCharToMultiByte(
                CP_UTF8,
                0,
                m_errorMessage.c_str(),
                static_cast<int>(m_errorMessage.length()),
                nullptr,
                0,
                nullptr,
                nullptr
            );
            if (sizeNeeded > 0)
            {
                m_narrowMessage.resize(sizeNeeded);
                WideCharToMultiByte(
                    CP_UTF8,
                    0,
                    m_errorMessage.c_str(),
                    static_cast<int>(m_errorMessage.length()),
                    m_narrowMessage.data(),
                    sizeNeeded,
                    nullptr,
                    nullptr
                );
            }
        }

        [[nodiscard]] const char* what() const noexcept override
        {
            return m_narrowMessage.c_str();
        }

        [[nodiscard]] HRESULT GetErrorCode() const noexcept { return m_errorCode; }
        [[nodiscard]] const std::wstring& GetErrorMessage() const noexcept { return m_errorMessage; }

    private:
        HRESULT       m_errorCode;
        std::wstring  m_functionName;
        std::wstring  m_filename;
        int           m_lineNumber;
        std::wstring  m_errorMessage;
        std::string   m_narrowMessage;
    };

    #ifndef HR_CHECK
    #define HR_CHECK(hr_expression)                                                                     \
        do                                                                                              \
        {                                                                                               \
            const HRESULT _hr_val = (hr_expression);                                                    \
            if (FAILED(_hr_val))                                                                        \
            {                                                                                           \
                if (IsDebuggerPresent())                                                                \
                {                                                                                       \
                    OutputDebugStringW(std::format(L"[HR_CHECK FAILED] 0x{:08X} at {}:{}\n",            \
                        static_cast<unsigned long>(_hr_val), TEXT(__FILE__), __LINE__).c_str());        \
                    __debugbreak();                                                                     \
                }                                                                                       \
                throw Sandbox3D::DxException(_hr_val, TEXT(#hr_expression), TEXT(__FILE__), __LINE__); \
            }                                                                                           \
        } while (0)
    #endif
}
