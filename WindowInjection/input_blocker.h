#pragma once

#include <windows.h>
#include <functional>

enum class EInputBlocker
{
    kOk = 0,
    kErrInstallMouseHook = 1001,
    kErrInstallKeyboardHook = 1002,
    kErrUninstallMouseHook = 1003,
    kErrUninstallKeyboardHook = 1004,
    kErrAlreadyActive = 1005,
    kErrNotActive = 1006,
    kErrUnknown = 9999
};

class InputBlocker
{
public:
    InputBlocker();
    ~InputBlocker();

    // Main control functions
    EInputBlocker StartBlocking();
    EInputBlocker StopBlocking();
    bool IsBlocking() const { return m_isBlocking; }

    // Cursor control
    void HideCursor();
    void ShowCursor();
    bool IsCursorHidden() const { return m_cursorHidden; }

    // Configuration
    void SetAllowEscapeKey(bool allow) { m_allowEscapeKey = allow; }
    void SetAllowCtrlAltDel(bool allow) { m_allowCtrlAltDel = allow; }
    void SetAllowWinKey(bool allow) { m_allowWinKey = allow; }
    void SetBlockMouse(bool block) { m_blockMouse = block; }
    void SetBlockKeyboard(bool block) { m_blockKeyboard = block; }

    // Emergency exit callback
    void SetEmergencyExitCallback(std::function<void()> callback) { m_emergencyExitCallback = callback; }

    // Get error message
    const TCHAR* GetLastErrorMsg() const { return m_lastErrorMsg; }

private:
    // Hook procedures
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

    // Instance methods called by static hook procedures
    LRESULT HandleMouseHook(int nCode, WPARAM wParam, LPARAM lParam);
    LRESULT HandleKeyboardHook(int nCode, WPARAM wParam, LPARAM lParam);

    // Helper functions
    bool ShouldBlockKeyboard(WPARAM wParam, LPARAM lParam);
    bool ShouldBlockMouse(WPARAM wParam, LPARAM lParam);
    void SetLastErrorMsg(const TCHAR* format, ...);
    void RestoreCursorState();

private:
    // State
    bool m_isBlocking;
    bool m_cursorHidden;
    int m_originalCursorCount;

    // Configuration
    bool m_allowEscapeKey;
    bool m_allowCtrlAltDel;
    bool m_allowWinKey;
    bool m_blockMouse;
    bool m_blockKeyboard;

    // Hooks
    HHOOK m_mouseHook;
    HHOOK m_keyboardHook;

    // Emergency exit
    std::function<void()> m_emergencyExitCallback;
    DWORD m_escapeKeyPressTime;
    int m_escapeKeyPressCount;
    static const DWORD EMERGENCY_EXIT_TIMEOUT = 3000; // 3 seconds
    static const int EMERGENCY_EXIT_COUNT = 5; // 5 presses

    // Error handling
    TCHAR m_lastErrorMsg[512];

    // Static instance for hook callbacks
    static InputBlocker* s_instance;
};