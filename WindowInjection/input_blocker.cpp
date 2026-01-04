#include "pch.h"
#include "input_blocker.h"
#include <stdio.h>
#include <strsafe.h>
#include <tchar.h>

// Static instance for hook callbacks
InputBlocker* InputBlocker::s_instance = nullptr;

InputBlocker::InputBlocker()
    : m_isBlocking(false)
    , m_cursorHidden(false)
    , m_originalCursorCount(0)
    , m_allowEscapeKey(false)
    , m_allowCtrlAltDel(true)
    , m_allowWinKey(false)
    , m_blockMouse(true)
    , m_blockKeyboard(true)
    , m_ignoreVirtualInput(true)
    , m_mouseHook(nullptr)
    , m_keyboardHook(nullptr)
    , m_escapeKeyPressTime(0)
    , m_escapeKeyPressCount(0)
{
    memset(m_lastErrorMsg, 0, sizeof(m_lastErrorMsg));
    s_instance = this;
}

InputBlocker::~InputBlocker()
{
    StopBlocking();
    ShowCursor();
    s_instance = nullptr;
}

EInputBlocker InputBlocker::StartBlocking()
{
    if (m_isBlocking)
    {
        SetLastErrorMsg(_T("Input blocking already active"));
        return EInputBlocker::kErrAlreadyActive;
    }

    // Install mouse hook if mouse blocking is enabled
    if (m_blockMouse && !m_mouseHook)
    {
        m_mouseHook = SetWindowsHookEx(
            WH_MOUSE_LL,
            LowLevelMouseProc,
            GetModuleHandle(NULL),
            0
        );
        
        if (!m_mouseHook)
        {
            SetLastErrorMsg(_T("Failed to install mouse hook, error: %d"), GetLastError());
            return EInputBlocker::kErrInstallMouseHook;
        }
    }

    // Install keyboard hook if keyboard blocking is enabled
    if (m_blockKeyboard && !m_keyboardHook)
    {
        m_keyboardHook = SetWindowsHookEx(
            WH_KEYBOARD_LL,
            LowLevelKeyboardProc,
            GetModuleHandle(NULL),
            0
        );
        
        if (!m_keyboardHook)
        {
            SetLastErrorMsg(_T("Failed to install keyboard hook, error: %d"), GetLastError());
            
            // Clean up mouse hook if it was installed
            if (m_mouseHook)
            {
                UnhookWindowsHookEx(m_mouseHook);
                m_mouseHook = nullptr;
            }
            
            return EInputBlocker::kErrInstallKeyboardHook;
        }
    }

    m_isBlocking = true;
    m_escapeKeyPressTime = 0;
    m_escapeKeyPressCount = 0;
    
    SetLastErrorMsg(_T("Input blocking started successfully"));
    return EInputBlocker::kOk;
}

EInputBlocker InputBlocker::StopBlocking()
{
    if (!m_isBlocking)
    {
        SetLastErrorMsg(_T("Input blocking not active"));
        return EInputBlocker::kErrNotActive;
    }

    EInputBlocker result = EInputBlocker::kOk;

    // Uninstall mouse hook
    if (m_mouseHook)
    {
        if (!UnhookWindowsHookEx(m_mouseHook))
        {
            SetLastErrorMsg(_T("Failed to uninstall mouse hook, error: %d"), GetLastError());
            result = EInputBlocker::kErrUninstallMouseHook;
        }
        m_mouseHook = nullptr;
    }

    // Uninstall keyboard hook
    if (m_keyboardHook)
    {
        if (!UnhookWindowsHookEx(m_keyboardHook))
        {
            SetLastErrorMsg(_T("Failed to uninstall keyboard hook, error: %d"), GetLastError());
            result = EInputBlocker::kErrUninstallKeyboardHook;
        }
        m_keyboardHook = nullptr;
    }

    m_isBlocking = false;
    
    if (result == EInputBlocker::kOk)
    {
        SetLastErrorMsg(_T("Input blocking stopped successfully"));
    }
    
    return result;
}

void InputBlocker::HideCursor()
{
    if (!m_cursorHidden)
    {
        // Hide cursor by decrementing display count
        int count = ::ShowCursor(FALSE);
        m_originalCursorCount = count + 1; // Store original count
        
        // Ensure cursor is completely hidden
        while (::ShowCursor(FALSE) >= 0) {
            // Keep decrementing until negative
        }
        
        // Also set cursor to null for extra measure
        SetCursor(NULL);
        
        m_cursorHidden = true;
    }
}

void InputBlocker::ShowCursor()
{
    if (m_cursorHidden)
    {
        RestoreCursorState();
        m_cursorHidden = false;
    }
}

void InputBlocker::RestoreCursorState()
{
    // Restore cursor display count
    while (::ShowCursor(TRUE) < m_originalCursorCount) {
        // Keep incrementing until we reach original count
    }
    
    // Restore default cursor
    SetCursor(LoadCursor(NULL, IDC_ARROW));
}

LRESULT CALLBACK InputBlocker::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (s_instance && nCode >= 0)
    {
        return s_instance->HandleMouseHook(nCode, wParam, lParam);
    }
    
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK InputBlocker::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (s_instance && nCode >= 0)
    {
        return s_instance->HandleKeyboardHook(nCode, wParam, lParam);
    }
    
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT InputBlocker::HandleMouseHook(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && m_isBlocking && m_blockMouse)
    {
        if (ShouldBlockMouse(wParam, lParam))
        {
            // Block the mouse event
            return 1;
        }
    }
    
    return CallNextHookEx(m_mouseHook, nCode, wParam, lParam);
}

LRESULT InputBlocker::HandleKeyboardHook(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && m_isBlocking && m_blockKeyboard)
    {
        KBDLLHOOKSTRUCT* pKeyboard = (KBDLLHOOKSTRUCT*)lParam;
        
        // Handle emergency exit sequence (multiple Escape key presses)
        if (m_allowEscapeKey && pKeyboard->vkCode == VK_ESCAPE && wParam == WM_KEYDOWN)
        {
            DWORD currentTime = GetTickCount();
            
            // Reset counter if too much time has passed
            if (currentTime - m_escapeKeyPressTime > EMERGENCY_EXIT_TIMEOUT)
            {
                m_escapeKeyPressCount = 0;
            }
            
            m_escapeKeyPressCount++;
            m_escapeKeyPressTime = currentTime;
            
            // Check for emergency exit
            if (m_escapeKeyPressCount >= EMERGENCY_EXIT_COUNT)
            {
                // Trigger emergency exit
                if (m_emergencyExitCallback)
                {
                    m_emergencyExitCallback();
                }
                return CallNextHookEx(m_keyboardHook, nCode, wParam, lParam);
            }
        }
        
        if (ShouldBlockKeyboard(wParam, lParam))
        {
            // Block the keyboard event
            return 1;
        }
    }
    
    return CallNextHookEx(m_keyboardHook, nCode, wParam, lParam);
}

bool InputBlocker::ShouldBlockMouse(WPARAM wParam, LPARAM lParam)
{
    // Check if this is virtual/injected input and we should ignore it
    if (m_ignoreVirtualInput)
    {
        MSLLHOOKSTRUCT* pMouse = (MSLLHOOKSTRUCT*)lParam;
        if (pMouse && (pMouse->flags & LLMHF_INJECTED))
        {
            // Don't block virtual/injected input - allow it to pass through
            return false;
        }
    }
    
    // Block physical mouse events when mouse blocking is enabled
    switch (wParam)
    {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_MOUSEMOVE:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            return true;
        default:
            return false;
    }
}

bool InputBlocker::ShouldBlockKeyboard(WPARAM wParam, LPARAM lParam)
{
    KBDLLHOOKSTRUCT* pKeyboard = (KBDLLHOOKSTRUCT*)lParam;
    DWORD vkCode = pKeyboard->vkCode;
    
    // Check if this is virtual/injected input and we should ignore it
    if (m_ignoreVirtualInput)
    {
        if (pKeyboard && (pKeyboard->flags & LLKHF_INJECTED))
        {
            // Don't block virtual/injected input - allow it to pass through
            return false;
        }
    }
    
    // Always allow Ctrl+Alt+Del if enabled
    if (m_allowCtrlAltDel)
    {
        bool ctrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool altPressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        
        if (ctrlPressed && altPressed && vkCode == VK_DELETE)
        {
            return false; // Don't block Ctrl+Alt+Del
        }
    }
    
    // Handle Windows key
    if (!m_allowWinKey && (vkCode == VK_LWIN || vkCode == VK_RWIN))
    {
        return true; // Block Windows key
    }
    
    // Handle Escape key (for emergency exit)
    if (m_allowEscapeKey && vkCode == VK_ESCAPE)
    {
        // Let the emergency exit logic in HandleKeyboardHook handle this
        return false;
    }
    
    // Block Alt+Tab, Alt+F4, and other system combinations
    bool altPressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    if (altPressed)
    {
        switch (vkCode)
        {
            case VK_TAB:    // Alt+Tab
            case VK_F4:     // Alt+F4
            case VK_ESCAPE: // Alt+Esc
                return true;
            default:
                break;
        }
    }
    
    // Block Ctrl+Shift+Esc (Task Manager)
    bool ctrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    if (ctrlPressed && shiftPressed && vkCode == VK_ESCAPE)
    {
        return true;
    }
    
    // Block function keys that might cause issues
    if (vkCode >= VK_F1 && vkCode <= VK_F12)
    {
        // Allow F1-F12 but block dangerous combinations
        if (altPressed || ctrlPressed)
        {
            return true;
        }
    }
    
    // Block all other keys by default when keyboard blocking is active
    return true;
}

void InputBlocker::SetLastErrorMsg(const TCHAR* format, ...)
{
    memset(m_lastErrorMsg, 0, sizeof(m_lastErrorMsg));
    
    va_list args;
    va_start(args, format);
    _vstprintf_s(
        m_lastErrorMsg,
        _countof(m_lastErrorMsg),
        format,
        args);
    va_end(args);
}