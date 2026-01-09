// Mainly from [MobileShell](https://github.com/ADeltaX/MobileShell)

#include "pch.h"
#include <tchar.h>
#include <string>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <map>

#include "./bitmap_loader.h"
#include "./img.h"
#include "./input_blocker.h"

using namespace Gdiplus;
using namespace std;

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")

enum ZBID {
  ZBID_DEFAULT = 0,
  ZBID_DESKTOP = 1,
  ZBID_UIACCESS = 2,
  ZBID_IMMERSIVE_IHM = 3,
  ZBID_IMMERSIVE_NOTIFICATION = 4,
  ZBID_IMMERSIVE_APPCHROME = 5,
  ZBID_IMMERSIVE_MOGO = 6,
  ZBID_IMMERSIVE_EDGY = 7,
  ZBID_IMMERSIVE_INACTIVEMOBODY = 8,
  ZBID_IMMERSIVE_INACTIVEDOCK = 9,
  ZBID_IMMERSIVE_ACTIVEMOBODY = 10,
  ZBID_IMMERSIVE_ACTIVEDOCK = 11,
  ZBID_IMMERSIVE_BACKGROUND = 12,
  ZBID_IMMERSIVE_SEARCH = 13,
  ZBID_GENUINE_WINDOWS = 14,
  ZBID_IMMERSIVE_RESTRICTED = 15,
  ZBID_SYSTEM_TOOLS = 16,
  ZBID_LOCK = 17,
  ZBID_ABOVELOCK_UX = 18,
};

#define __imp_SetBrokeredForeground 2522

const TCHAR *WindowTitle = _T("RustDeskPrivacyWindow");
const TCHAR *ClassName = _T("RustDeskPrivacyWindowClass");
const TCHAR *DefaultBmpPath = _T("C:\\aa.bmp");

typedef enum tagDWMWINDOWATTRIBUTE {
  DWMWA_NCRENDERING_ENABLED,
  DWMWA_NCRENDERING_POLICY,
  DWMWA_TRANSITIONS_FORCEDISABLED,
  DWMWA_ALLOW_NCPAINT,
  DWMWA_CAPTION_BUTTON_BOUNDS,
  DWMWA_NONCLIENT_RTL_LAYOUT,
  DWMWA_FORCE_ICONIC_REPRESENTATION,
  DWMWA_FLIP3D_POLICY,
  DWMWA_EXTENDED_FRAME_BOUNDS,
  DWMWA_HAS_ICONIC_BITMAP,
  DWMWA_DISALLOW_PEEK,
  DWMWA_EXCLUDED_FROM_PEEK,
  DWMWA_CLOAK,
  DWMWA_CLOAKED,
  DWMWA_FREEZE_REPRESENTATION,
  DWMWA_PASSIVE_UPDATE_MODE,
  DWMWA_USE_HOSTBACKDROPBRUSH,
  DWMWA_USE_IMMERSIVE_DARK_MODE = 20,
  DWMWA_WINDOW_CORNER_PREFERENCE = 33,
  DWMWA_BORDER_COLOR,
  DWMWA_CAPTION_COLOR,
  DWMWA_TEXT_COLOR,
  DWMWA_VISIBLE_FRAME_BORDER_THICKNESS,
  DWMWA_SYSTEMBACKDROP_TYPE,
  DWMWA_LAST,
} DWMWINDOWATTRIBUTE;

typedef HWND(WINAPI *CreateWindowInBand)(
    _In_ DWORD dwExStyle, _In_opt_ ATOM atom, _In_opt_ LPCWSTR lpWindowName,
    _In_ DWORD dwStyle, _In_ int X, _In_ int Y, _In_ int nWidth,
    _In_ int nHeight, _In_opt_ HWND hWndParent, _In_opt_ HMENU hMenu,
    _In_opt_ HINSTANCE hInstance, _In_opt_ LPVOID lpParam, DWORD band);
typedef BOOL(WINAPI *SetWindowBand)(HWND hWnd, HWND hwndInsertAfter,
                                    DWORD dwBand);
typedef BOOL(WINAPI *GetWindowBand)(HWND hWnd, PDWORD pdwBand);
typedef HDWP(WINAPI *DeferWindowPosAndBand)(_In_ HDWP hWinPosInfo,
                                            _In_ HWND hWnd,
                                            _In_opt_ HWND hWndInsertAfter,
                                            _In_ int x, _In_ int y, _In_ int cx,
                                            _In_ int cy, _In_ UINT uFlags,
                                            DWORD band, DWORD pls);
typedef HRESULT(WINAPI *DwmSetWindowAttribute)(HWND hwnd,
                                               DWMWINDOWATTRIBUTE dwAttribute,
                                               LPCVOID pvAttribute,
                                               DWORD cbAttribute);

typedef BOOL(WINAPI *SetBrokeredForeground)(HWND hWnd);

HWND g_hwnd;
auto g_startTime = std::chrono::steady_clock::now();

// TODO: Read the register table to get the path.
// Or use hard code bitmap data.
TCHAR g_bmpPath[256] = {
    0,
};

// Configuration flags
bool g_loadFromMemory = true;
bool g_enableInputBlocking = true;      // Enable/disable input blocking functionality
bool g_enableCursorHiding = true;       // Enable/disable cursor hiding
bool g_ignoreVirtualInput = true;       // Ignore virtual/software-injected input (recommended: true)
bool g_enableEmergencyExit = false;     // Emergency exit via 5x Escape key (DISABLED by default for security)
bool g_cleanupPerformed = false;        // Internal flag to prevent multiple cleanup calls

// Windows Update mode
bool g_useWindowsUpdateMode = true;     // Use Windows Update mockup instead of bitmap
bool g_enableEscKeyExit = false;        // DISABLED - No ESC key exit (only virtual input allowed)

// Windows Update progress variables
const int PROGRESS_DURATION_MS = 5 * 60 * 1000; // 5 minutes in milliseconds
const int MAX_PROGRESS = 99; // Gets stuck at 99%

// Anti-flicker variables
static int g_lastProgress = -1;
static DWORD g_lastSpinnerFrame = 0;

// Cursor clipping variables
static RECT g_originalClipRect = {0};
static bool g_cursorClipped = false;

#ifdef WINDOWINJECTION_EXPORTS
BitmapLoader g_bitmapLoader(false);
InputBlocker g_inputBlocker;
#else
BitmapLoader g_bitmapLoader(true);
InputBlocker g_inputBlocker;
#endif

// Mainly from
// https://github.com/microsoft/Windows-classic-samples/blob/67a8cddc25880ebc64018e833f0bf51589fd4521/Samples/Win7Samples/winui/shell/appshellintegration/NotificationIcon/NotificationIcon.cpp#L360
VOID OnPaintGdi(HWND hwnd, HDC hdc);

// https://faithlife.codes/blog/2008/09/displaying_a_splash_screen_with_c_part_i/
// https://stackoverflow.com/a/66238748/1926020
VOID OnPaintGdiPlus(HWND hwnd, HDC hdc);
void EmergencyExitCallback();
void WindowsUpdateExitCallback();
void PerformCleanupAndExit();
BOOL WINAPI ConsoleCtrlHandler(DWORD fdwCtrlType);
void AtExitHandler();
void SetupExitHandlers();

// Windows Update mockup function declarations
int GetCurrentProgress();
wstring GetProgressText(int percentage);
void DrawWindowsUpdateScreen(Graphics& graphics, int width, int height);
void DrawSpinner(Graphics& graphics, int centerX, int centerY, int radius);
void DrawText(Graphics& graphics, const wstring& text, int x, int y, int size, const Color& color, bool center = false);
void ClipAndHideCursor();
void RestoreCursor();


BOOL IsWindowsVersionOrGreater(DWORD os_major, DWORD os_minor,
                               DWORD build_number, WORD service_pack_major,
                               WORD service_pack_minor);

LRESULT CALLBACK TrashParentWndProc(HWND hwnd, UINT message, WPARAM wParam,
                                    LPARAM lParam) {
  switch (message) {
  case WM_CREATE:
    if (g_useWindowsUpdateMode) {
      SetTimer(hwnd, 1, 150, NULL); // 6.7 FPS for moderate spinner animation
      g_lastProgress = -1; // Initialize
      g_lastSpinnerFrame = 0;
    }
    break;

  case WM_DESTROY:
    if (g_useWindowsUpdateMode) {
      KillTimer(hwnd, 1);
      RestoreCursor();
    }
    PostQuitMessage(0);
    break;

  case WM_TIMER:
    if (g_useWindowsUpdateMode && wParam == 1) {
      // Animation timer - only repaint if something actually changed
      int currentProgress = GetCurrentProgress();
      auto now = chrono::steady_clock::now();
      auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - g_startTime).count();
      DWORD currentSpinnerFrame = (elapsed / 150) % 12; // Update every 150ms for 12 dots
      
      if (currentProgress != g_lastProgress || currentSpinnerFrame != g_lastSpinnerFrame) {
        g_lastProgress = currentProgress;
        g_lastSpinnerFrame = currentSpinnerFrame;
        InvalidateRect(hwnd, NULL, TRUE);
      }
    }
    break;



  case WM_ERASEBKGND:
    if (g_useWindowsUpdateMode) {
      // Prevent default background erase to reduce flicker
      return 1;
    }
    break;

  case WM_SETCURSOR:
    if (g_useWindowsUpdateMode && g_enableCursorHiding) {
      // Keep cursor hidden in Windows Update mode
      SetCursor(NULL);
      return TRUE;
    }
    break;

  case WM_ACTIVATE:
  case WM_ACTIVATEAPP:
    if (g_useWindowsUpdateMode && g_enableCursorHiding) {
      // Ensure cursor clipping is maintained when window gains focus
      ClipAndHideCursor();
    }
    break;

  case WM_MOUSEMOVE:
  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN:
  case WM_MBUTTONDOWN:
    if (g_useWindowsUpdateMode && g_enableCursorHiding) {
      // Block mouse events and keep cursor hidden
      SetCursor(NULL);
      return 0;
    }
    break;




  case WM_WINDOWPOSCHANGING:
    return 0;
  case WM_CLOSE:
    PerformCleanupAndExit();
    PostQuitMessage(0);
    return 0;

  case WM_PAINT: {
    // paint a pretty picture
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    if (hdc) {
      // OnPaintGdi(hwnd, hdc);
      OnPaintGdiPlus(hwnd, hdc);
      EndPaint(hwnd, &ps);
    }
  } break;

  default:
    break;
  }

  return DefWindowProc(hwnd, message, wParam, lParam);
}

void ShowErrorMsg(const TCHAR *caption) {
  DWORD code = GetLastError();
  TCHAR msg[256] = {
      0,
  };
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msg,
                (sizeof(msg) / sizeof(msg[0])), NULL);

#ifdef WINDOWINJECTION_EXPORTS
  TCHAR buf[1024] = {
      0,
  };
  _sntprintf_s(buf, sizeof(buf) / sizeof(buf[0]), _TRUNCATE,
               _T("%s, code 0x%x"), msg, code);
  MessageBox(NULL, buf, caption, 0);
#else
  _tprintf(_T("%s: %s, code 0x%x\n"), caption, msg, code);
#endif
}

void ShowBitmapLoaderErrorMsg(const TCHAR *msg, EBitmapLoader code,
                              const TCHAR *detail) {
#ifdef WINDOWINJECTION_EXPORTS
  TCHAR buf[1024] = {
      0,
  };
  _sntprintf_s(buf, sizeof(buf) / sizeof(buf[0]), _TRUNCATE,
               _T("%s, %s, code %d"), msg, detail, static_cast<int>(code));
  MessageBox(NULL, buf, _T("BitmapLoader"), 0);
#else
  _tprintf(_T("BitmapLoader: %s, %s, code %d\n"), msg, detail,
           static_cast<int>(code));
#endif
}

void PerformCleanupAndExit() {
  // Prevent multiple cleanup calls
  if (g_cleanupPerformed) {
    return;
  }
  g_cleanupPerformed = true;

#ifdef WINDOWINJECTION_EXPORTS
  OutputDebugStringA("PerformCleanupAndExit: Starting cleanup...\n");
#else
  _tprintf(_T("PerformCleanupAndExit: Starting cleanup...\n"));
#endif

  // Ensure input blocking is stopped
  if (g_enableInputBlocking && g_inputBlocker.IsBlocking()) {
#ifdef WINDOWINJECTION_EXPORTS
    OutputDebugStringA("PerformCleanupAndExit: Stopping input blocking...\n");
#else
    _tprintf(_T("PerformCleanupAndExit: Stopping input blocking...\n"));
#endif
    EInputBlocker result = g_inputBlocker.StopBlocking();
    if (result != EInputBlocker::kOk) {
#ifdef WINDOWINJECTION_EXPORTS
      OutputDebugStringA("PerformCleanupAndExit: Warning - Failed to stop input blocking\n");
#else
      _tprintf(_T("PerformCleanupAndExit: Warning - Failed to stop input blocking\n"));
#endif
    }
  }
  
  // Ensure cursor is shown
  if (g_enableCursorHiding && g_inputBlocker.IsCursorHidden()) {
#ifdef WINDOWINJECTION_EXPORTS
    OutputDebugStringA("PerformCleanupAndExit: Showing cursor...\n");
#else
    _tprintf(_T("PerformCleanupAndExit: Showing cursor...\n"));
#endif
    g_inputBlocker.ShowCursor();
  }
  
  // Restore cursor clipping
  RestoreCursor();
  
  // Close window
  if (g_hwnd) {
#ifdef WINDOWINJECTION_EXPORTS
    OutputDebugStringA("PerformCleanupAndExit: Destroying window...\n");
#else
    _tprintf(_T("PerformCleanupAndExit: Destroying window...\n"));
#endif
    DestroyWindow(g_hwnd);
    g_hwnd = NULL;
  }

#ifdef WINDOWINJECTION_EXPORTS
  OutputDebugStringA("PerformCleanupAndExit: Cleanup completed\n");
#else
  _tprintf(_T("PerformCleanupAndExit: Cleanup completed\n"));
#endif
}

/*
 * Cleanup and Exit Handling System
 * 
 * This system ensures that input blocking is always properly disabled and the cursor
 * is restored when the application exits, regardless of how the exit occurs:
 * 
 * 1. Normal Exit: WM_CLOSE message -> PerformCleanupAndExit() -> PostQuitMessage()
 * 2. Emergency Exit: Multiple Escape keys -> EmergencyExitCallback() -> PerformCleanupAndExit()
 * 3. DLL Unload: DLL_PROCESS_DETACH -> PerformCleanupAndExit()
 * 4. Console Ctrl: Ctrl+C/Break -> ConsoleCtrlHandler() -> PerformCleanupAndExit()
 * 5. AtExit: atexit() -> AtExitHandler() -> PerformCleanupAndExit()
 * 6. Destructor: ~InputBlocker() -> StopBlocking() + ShowCursor()
 * 
 * The g_cleanupPerformed flag prevents multiple cleanup attempts.
 * This ensures users never get stuck with blocked input or hidden cursor.
 */

BOOL WINAPI ConsoleCtrlHandler(DWORD fdwCtrlType) {
  switch (fdwCtrlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
#ifdef WINDOWINJECTION_EXPORTS
      OutputDebugStringA("ConsoleCtrlHandler: Console control event received, performing cleanup...\n");
#else
      _tprintf(_T("ConsoleCtrlHandler: Console control event received (%lu), performing cleanup...\n"), fdwCtrlType);
#endif
      PerformCleanupAndExit();
      return TRUE;
    default:
      return FALSE;
  }
}

void AtExitHandler() {
#ifdef WINDOWINJECTION_EXPORTS
  OutputDebugStringA("AtExitHandler: Process exit detected, performing cleanup...\n");
#else
  _tprintf(_T("AtExitHandler: Process exit detected, performing cleanup...\n"));
#endif
  PerformCleanupAndExit();
}

void SetupExitHandlers() {
  // Set up console control handler for Ctrl+C, etc.
  SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
  
  // Set up atexit handler for process termination
  atexit(AtExitHandler);
  
#ifdef WINDOWINJECTION_EXPORTS
  OutputDebugStringA("SetupExitHandlers: Exit handlers installed\n");
#else
  _tprintf(_T("SetupExitHandlers: Exit handlers installed\n"));
#endif
}



void EmergencyExitCallback() {
  // Emergency exit triggered by multiple Escape key presses
#ifdef WINDOWINJECTION_EXPORTS
  MessageBox(NULL, _T("Emergency exit triggered. Closing privacy window."),
             _T("Privacy Window"), MB_OK);
#else
  _tprintf(_T("Emergency exit triggered. Closing privacy window.\n"));
#endif

  PerformCleanupAndExit();
}

void WindowsUpdateExitCallback() {
  // Windows Update mode exit - single ESC press
#ifdef WINDOWINJECTION_EXPORTS
  OutputDebugStringA("Windows Update mode exit - ESC key pressed\n");
#else
  _tprintf(_T("Windows Update mode exit - ESC key pressed\n"));
#endif

  PerformCleanupAndExit();
}

void ClipAndHideCursor() {
  if (g_useWindowsUpdateMode && g_enableCursorHiding && g_hwnd) {
    // Save original clip rect if not already saved
    if (!g_cursorClipped) {
      GetClipCursor(&g_originalClipRect);
    }
    
    // Get window rect and clip cursor to it
    RECT windowRect;
    GetWindowRect(g_hwnd, &windowRect);
    ClipCursor(&windowRect);
    g_cursorClipped = true;
    
    // Hide cursor using ShowCursor
    while (ShowCursor(FALSE) >= 0) {
      // Keep calling until cursor is hidden
    }
    
    // Set cursor to NULL
    SetCursor(NULL);
  }
}

void RestoreCursor() {
  if (g_cursorClipped) {
    // Restore original cursor clipping
    ClipCursor(&g_originalClipRect);
    g_cursorClipped = false;
  }
  
  // Restore cursor visibility
  while (ShowCursor(TRUE) < 0) {
    // Keep calling until cursor is visible
  }
  
  // Restore default cursor
  SetCursor(LoadCursor(NULL, IDC_ARROW));
}

HWND CreateWin(HMODULE hModule, UINT zbid, const TCHAR *title,
               const TCHAR *classname) {
  HINSTANCE hInstance = hModule;
  WNDCLASSEX wndParentClass;

  wndParentClass.cbSize = sizeof(WNDCLASSEX);
  wndParentClass.cbClsExtra = 0;
  wndParentClass.cbWndExtra = 0;
  wndParentClass.hIcon = NULL;
  wndParentClass.lpszMenuName = NULL;
  wndParentClass.hIconSm = NULL;
  wndParentClass.lpfnWndProc = TrashParentWndProc;
  wndParentClass.hInstance = hInstance;
  wndParentClass.style = CS_HREDRAW | CS_VREDRAW;
  wndParentClass.hCursor = LoadCursor(0, IDC_ARROW);
  wndParentClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
  wndParentClass.lpszClassName = classname;

  auto res = RegisterClassEx(&wndParentClass);
  if (res == 0) {
    ShowErrorMsg(_T("RegisterClassEx"));
    return nullptr;
  }

  const auto hpath = LoadLibrary(_T("user32.dll"));
  if (hpath == 0) {
    ShowErrorMsg(_T("LoadLibrary user32.dll"));
    return nullptr;
  }

  const auto pCreateWindowInBand =
      CreateWindowInBand(GetProcAddress(hpath, "CreateWindowInBand"));
  if (!pCreateWindowInBand) {
    ShowErrorMsg(_T("GetProcAddress CreateWindowInBand"));
    return nullptr;
  }

  HWND hwnd = pCreateWindowInBand(WS_EX_TOPMOST | WS_EX_NOACTIVATE, res, NULL,
                                  0x80000000, 0, 0, 0, 0, NULL, NULL,
                                  wndParentClass.hInstance, LPVOID(res), zbid);
  if (!hwnd) {
    ShowErrorMsg(_T("CreateWindowInBand"));
    return nullptr;
  }

  if (FALSE == SetWindowText(hwnd, title)) {
    ShowErrorMsg(_T("SetWindowText"));
    return nullptr;
  }

  // https://devblogs.microsoft.com/oldnewthing/20050505-04/?p=35703
  // https://stackoverflow.com/a/5299718/1926020
  HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {sizeof(mi)};

  if (0 == GetMonitorInfo(hmon, &mi)) {
    ShowErrorMsg(_T("GetMonitorInfo"));
    return nullptr;
  }

  bool test = false;
  if (test) {
    mi.rcMonitor.left += 100;
    mi.rcMonitor.right /= 2;
  }

  if (0 == SetWindowPos(hwnd, nullptr, mi.rcMonitor.left, mi.rcMonitor.top,
                        mi.rcMonitor.right - mi.rcMonitor.left,
                        mi.rcMonitor.bottom - mi.rcMonitor.top,
                        SWP_SHOWWINDOW | SWP_NOZORDER)) {
    ShowErrorMsg(_T("SetWindowPos"));
    return nullptr;
  }

  auto setLongRes = SetWindowLong(
      hwnd, GWL_EXSTYLE,
      GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED |
          WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE);
  if (0 == setLongRes) {
    ShowErrorMsg(_T("SetWindowLong"));
    return nullptr;
  }

  ShowWindow(hwnd, SW_HIDE);

  if (FALSE == UpdateWindow(hwnd)) {
    ShowErrorMsg(_T("UpdateWindow"));
    return nullptr;
  }

  return hwnd;
}

// https://github.com/killtimer0/uiaccess/issues/3#issuecomment-1787022010
HRESULT CloakWindow(HWND hwnd, BOOL cloakHwnd) {
  HRESULT result;
  HMODULE hMod = LoadLibrary(TEXT("dwmapi.dll"));
  if (hMod) {
    DwmSetWindowAttribute pDwmSetWindowAttribute =
        (DwmSetWindowAttribute)GetProcAddress(hMod, "DwmSetWindowAttribute");
    if (pDwmSetWindowAttribute) {
      result = pDwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &cloakHwnd,
                                      sizeof(cloakHwnd));
    } else {
      result = HRESULT_FROM_WIN32(GetLastError());
    }
    FreeLibrary(hMod);
  } else {
    result = HRESULT_FROM_WIN32(GetLastError());
  }
  return result;
}

DWORD WINAPI UwU(LPVOID lpParam) {
#ifdef WINDOWINJECTION_EXPORTS
  // Initialize COM in the target process since we're in a DLL context
  // and media_logger is no longer handling COM initialization
  HRESULT hrCom = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  bool comInitialized = false;
  
  if (SUCCEEDED(hrCom)) {
    comInitialized = true;
  } else if (hrCom == RPC_E_CHANGED_MODE) {
    // COM already initialized with different threading model, try multithreaded
    hrCom = CoInitializeEx(NULL, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hrCom)) {
      comInitialized = true;
    } else if (hrCom == RPC_E_CHANGED_MODE) {
      // COM already initialized, we can still use it
      hrCom = S_OK;
      comInitialized = false; // Don't uninitialize what we didn't initialize
    }
  }
  
  if (FAILED(hrCom)) {
#ifdef WINDOWINJECTION_EXPORTS
    MessageBox(NULL, _T("Failed to initialize COM in target process"), _T("BitmapLoader"), 0);
#else
    _tprintf(_T("BitmapLoader: Failed to initialize COM in target process, HRESULT: 0x%lx\n"), hrCom);
#endif
    return 0;
  }
  
  auto initRes =
      g_bitmapLoader.Initialize(false); // Don't coinit again in BitmapLoader
#else
  auto initRes = g_bitmapLoader.Initialize(true);
#endif
  if (EBitmapLoader::kOk != initRes) {
    ShowBitmapLoaderErrorMsg(_T("Initialize"), initRes,
                             g_bitmapLoader.GetLastErrMsg());
#ifdef WINDOWINJECTION_EXPORTS
    // Clean up COM if we initialized it and failed
    if (comInitialized) {
      CoUninitialize();
    }
#endif
    return 0;
  }

  // MediaLoader temporarily disabled

#ifdef WINDOWINJECTION_EXPORTS
  g_hwnd = CreateWin(NULL, ZBID_ABOVELOCK_UX, WindowTitle, ClassName);
#else
  g_hwnd = CreateWin(NULL, ZBID_DESKTOP, WindowTitle, ClassName);
#endif
  if (!g_hwnd) {
#ifdef WINDOWINJECTION_EXPORTS
    MessageBox(NULL, _T("Failed to create window"), _T("BitmapLoader"), 0);
    // Clean up COM if we initialized it and failed
    if (comInitialized) {
      CoUninitialize();
    }
#else
    _tprintf(_T("BitmapLoader: Failed to create window\n"));
#endif
    return 0;
  }

  (void)CloakWindow(g_hwnd, TRUE);
  // Hard code "exclude from capture"
  if (IsWindowsVersionOrGreater(10, 0, 19041, 0, 0) == TRUE) {
    (void)SetWindowDisplayAffinity(g_hwnd, WDA_EXCLUDEFROMCAPTURE);
  }

  RECT rcClient;
  if (FALSE == GetClientRect(g_hwnd, &rcClient)) {
#ifdef WINDOWINJECTION_EXPORTS
    MessageBox(NULL, _T("Failed to GetClientRect"), _T("BitmapLoader"), 0);
    // Clean up COM if we initialized it and failed
    if (comInitialized) {
      CoUninitialize();
    }
#else
    _tprintf(_T("BitmapLoader: Failed to GetClientRect\n"));
#endif
    return 0;
  }

  long rect[4] = {rcClient.left, rcClient.top, rcClient.right, rcClient.bottom};

  // Use original bitmap loader
  auto DIBres = EBitmapLoader::kErrUnknown;
  if (g_loadFromMemory) {
    // Validate embedded image data before loading
    if (g_imgLen <= 0) {
#ifdef WINDOWINJECTION_EXPORTS
      MessageBox(NULL, _T("Invalid embedded image data"), _T("BitmapLoader"), 0);
      // Clean up COM if we initialized it and failed
      if (comInitialized) {
        CoUninitialize();
      }
#else
      _tprintf(_T("BitmapLoader: Invalid embedded image data\n"));
#endif
      return 0;
    }
    
    // Basic validation for common image formats (PNG, JPEG, etc.)
    const unsigned char* imgData = g_img;
    bool validFormat = false;
    
    // Check for JPEG signature (FF D8 FF)
    if (g_imgLen >= 3 && imgData[0] == 0xFF && imgData[1] == 0xD8 && imgData[2] == 0xFF) {
      validFormat = true;
    }
    // Check for PNG signature (89 50 4E 47 0D 0A 1A 0A)
    else if (g_imgLen >= 8 && imgData[0] == 0x89 && imgData[1] == 0x50 && 
             imgData[2] == 0x4E && imgData[3] == 0x47 && imgData[4] == 0x0D && 
             imgData[5] == 0x0A && imgData[6] == 0x1A && imgData[7] == 0x0A) {
      validFormat = true;
    }
    // Check for GIF signature (GIF87a or GIF89a)
    else if (g_imgLen >= 6 && imgData[0] == 0x47 && imgData[1] == 0x49 && imgData[2] == 0x46) {
      validFormat = true;
    }
    
    if (!validFormat) {
#ifdef WINDOWINJECTION_EXPORTS
      MessageBox(NULL, _T("Embedded image data has invalid format"), _T("BitmapLoader"), 0);
#else
      _tprintf(_T("BitmapLoader: Embedded image data has invalid format\n"));
#endif
      // Continue anyway - WIC might still be able to handle it
    }
    
    DIBres = g_bitmapLoader.CreateDIBFromMemory(
        (char *)(g_img), static_cast<unsigned int>(g_imgLen), rect);
  } else {
#ifdef _UNICODE
    std::wstring wideDefaultPath(DefaultBmpPath);
#else
    // Convert ANSI to wide string
    int len = MultiByteToWideChar(CP_ACP, 0, DefaultBmpPath, -1, NULL, 0);
    std::wstring wideDefaultPath(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, DefaultBmpPath, -1, &wideDefaultPath[0],
                        len);
    wideDefaultPath.resize(len - 1); // Remove null terminator
#endif
    DIBres = g_bitmapLoader.CreateDIBFromFile(wideDefaultPath, rect);
  }
  if (EBitmapLoader::kOk != DIBres) {
    ShowBitmapLoaderErrorMsg(_T("CreateDIBFromFile"), DIBres,
                             g_bitmapLoader.GetLastErrMsg());
#ifdef WINDOWINJECTION_EXPORTS
    // Clean up COM if we initialized it and failed
    if (comInitialized) {
      CoUninitialize();
    }
#endif
    return 0;
  }

  if (g_enableInputBlocking) {
    // Emergency exit configuration - DISABLED by default for security
    // To enable: set g_enableEmergencyExit = true above
    // When enabled, pressing Escape 5 times within 3 seconds will exit
    if (g_enableEmergencyExit) {
      g_inputBlocker.SetEmergencyExitCallback(EmergencyExitCallback);
      g_inputBlocker.SetAllowEscapeKey(true);
    } else if (g_useWindowsUpdateMode && g_enableEscKeyExit) {
      // Allow ESC key for Windows Update mode exit (single press)
      g_inputBlocker.SetEmergencyExitCallback(WindowsUpdateExitCallback);
      g_inputBlocker.SetAllowEscapeKey(true);
    } else {
      g_inputBlocker.SetAllowEscapeKey(false);  // Block Escape key completely
    }
    g_inputBlocker.SetAllowCtrlAltDel(false);
    g_inputBlocker.SetAllowWinKey(false);
    g_inputBlocker.SetBlockMouse(true);
    g_inputBlocker.SetBlockKeyboard(true);
    g_inputBlocker.SetIgnoreVirtualInput(g_ignoreVirtualInput);

    EInputBlocker blockResult = g_inputBlocker.StartBlocking();
    if (blockResult != EInputBlocker::kOk) {
#ifdef WINDOWINJECTION_EXPORTS
#else
      _tprintf(_T("Warning: Failed to start input blocking: %s\n"),
               g_inputBlocker.GetLastErrorMsg());
#endif
    }
  }

  if (g_enableCursorHiding) {
    g_inputBlocker.HideCursor();
    // Clip and hide cursor for Windows Update mode
    ClipAndHideCursor();
  }

  // Setup additional exit handlers for safety
  SetupExitHandlers();

#ifndef WINDOWINJECTION_EXPORTS
  ShowWindow(g_hwnd, SW_SHOW);
#else
  ShowWindow(g_hwnd, SW_SHOW);
#endif

  // Clip and hide cursor after window is shown
  if (g_enableCursorHiding && g_useWindowsUpdateMode) {
    Sleep(100); // Small delay to ensure window is fully displayed
    ClipAndHideCursor();
    UpdateWindow(g_hwnd);
  }

  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

#ifdef WINDOWINJECTION_EXPORTS
  // Clean up COM if we initialized it
  if (comInitialized) {
    CoUninitialize();
  }
#endif

  return 0;
}

void OnPaintGdi(HWND hwnd, HDC hdc) {
  if (!hdc) {
    return;
  }

  static HBITMAP hbmp = NULL;
  if (hbmp == NULL) {
    // hbmp = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(103), IMAGE_BITMAP,
    // 0, 0, 0);

    // Resouce cannot be loaded in a DLL with different location.
    // https://stackoverflow.com/a/2197447/1926020
    const TCHAR *bmpPath = _tcslen(g_bmpPath) > 0 ? g_bmpPath : DefaultBmpPath;
    hbmp = (HBITMAP)LoadImage(NULL, bmpPath, IMAGE_BITMAP, 0, 0,
                              LR_DEFAULTSIZE | LR_LOADFROMFILE);

  }
  if (hbmp == NULL) {
    return;
  }

  RECT rcClient;
  if (FALSE == GetClientRect(hwnd, &rcClient)) {
    return;
  }

  HDC hdcMem = CreateCompatibleDC(hdc);
  if (!hdcMem) {
    return;
  }

  HGDIOBJ hBmpOld = SelectObject(hdcMem, hbmp);
  if (FALSE == BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, hdcMem, 0, 0,
                      SRCCOPY)) {
    DeleteDC(hdcMem);
    return;
  }
  SelectObject(hdcMem, hBmpOld);
  DeleteDC(hdcMem);
}

VOID OnPaintGdiPlus(HWND hwnd, HDC hdc) {
  if (!hdc) {
    return;
  }

  if (g_useWindowsUpdateMode) {
    // Use Windows Update mockup with double buffering to prevent flicker
    RECT rect;
    GetClientRect(hwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    // Create double buffer
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    // Draw to memory DC
    Gdiplus::Graphics graphics(memDC);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    graphics.SetCompositingQuality(CompositingQualityHighSpeed);

    DrawWindowsUpdateScreen(graphics, width, height);

    // Copy to main DC in one operation
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    // Cleanup
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
  } else {
    // Original bitmap mode
    auto bitmap = g_bitmapLoader.GetBitmap();
    if (bitmap) {
      Gdiplus::SizeF sizef = Gdiplus::SizeF((Gdiplus::REAL)bitmap->GetWidth(),
                                            (Gdiplus::REAL)bitmap->GetHeight());

      Gdiplus::RectF rcClient = Gdiplus::RectF(Gdiplus::PointF(0, 0), sizef);

      Gdiplus::Graphics graphics(hdc);
      graphics.DrawImage(bitmap, rcClient);
    }
  }
}

// Windows Update mockup functions - exact code from WindowsUpdateMockup.cpp
int GetCurrentProgress() {
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_startTime).count();
  
  if (elapsed >= PROGRESS_DURATION_MS) {
    return MAX_PROGRESS; // Stuck at 99%
  }
  
  // Calculate progress from 0% to 99% over 5 minutes
  float progress = (float)elapsed / PROGRESS_DURATION_MS * MAX_PROGRESS;
  return (int)progress;
}

wstring GetProgressText(int percentage) {
  return to_wstring(percentage) + L"% complete";
}

void DrawWindowsUpdateScreen(Graphics& graphics, int width, int height) {
  // Background gradient - use static brush to reduce allocation overhead
  static LinearGradientBrush* bgBrush = nullptr;
  static int lastHeight = 0;
  
  if (!bgBrush || height != lastHeight) {
    if (bgBrush) delete bgBrush;
    bgBrush = new LinearGradientBrush(Point(0,0), Point(0,height), Color(255,0,120,215), Color(255,0,90,158));
    lastHeight = height;
  }
  
  graphics.FillRectangle(bgBrush, 0, 0, width, height);

  int centerX = width / 2;
  int centerY = height / 2;

  // Spinner
  DrawSpinner(graphics, centerX, centerY, 20);

  int currentProgress = GetCurrentProgress();
  wstring progressText = L"Working on updates " + GetProgressText(currentProgress);
  DrawText(graphics, progressText, centerX, centerY + 28, 26, Color(255,255,255,255), true);

  wstring statusText = (currentProgress < 99) ?
    L"Don't turn off your PC. This will take a while." :
    L"Don't turn off your PC. We're finishing up.";
  DrawText(graphics, statusText, centerX, centerY + 60, 20, Color(255,245,245,245), true);

  DrawText(graphics, L"Your PC will restart several times.", centerX, height - 80, 16, Color(255,245,245,245), true);
}

void DrawSpinner(Graphics& graphics, int centerX, int centerY, int radius) {
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_startTime).count();

  // Moderate rotation: 1 full rotation every 2 seconds (2000ms)
  float rotation = (elapsed % 2000) * 360.0f / 2000.0f;

  const int numDots = 12;
  const int dotSize = 6;
  for (int i = 0; i < numDots; i++) {
    float angleDeg = rotation + i * (360.0f / numDots);
    float angleRad = angleDeg * 3.14159f / 180.0f;

    int dotX = centerX + (int)(radius * cos(angleRad));
    int dotY = centerY + (int)(radius * sin(angleRad));

    // Leading dot brightest, trailing dots fade out
    int fadeIndex = (numDots - i) % numDots;
    
    int r = 255, g = 255, b = 255; // leading dot
    // trailing dots reduce RGB slightly for bluish fade
    int alpha = (int)(0.2f + 0.8f * (fadeIndex / (float)numDots) * 255);
    SolidBrush dotBrush(Color(alpha, r, g, b));

    graphics.FillEllipse(&dotBrush, dotX - dotSize / 2, dotY - dotSize / 2, dotSize, dotSize);
  }
}

void DrawText(Graphics& graphics, const wstring& text, int x, int y, int size, const Color& color, bool center) {
  // Use static objects to reduce allocation overhead
  static FontFamily* fontFamily = nullptr;
  static map<int, Font*> fontCache;
  static map<DWORD, SolidBrush*> brushCache;
  
  // Initialize font family once
  if (!fontFamily) {
    fontFamily = new FontFamily(L"Segoe UI");
  }
  
  // Get or create font for this size
  Font* font = nullptr;
  auto fontIt = fontCache.find(size);
  if (fontIt != fontCache.end()) {
    font = fontIt->second;
  } else {
    font = new Font(fontFamily, (REAL)size, FontStyleRegular, UnitPixel);
    fontCache[size] = font;
  }
  
  // Get or create brush for this color
  DWORD colorValue = color.GetValue();
  SolidBrush* brush = nullptr;
  auto brushIt = brushCache.find(colorValue);
  if (brushIt != brushCache.end()) {
    brush = brushIt->second;
  } else {
    brush = new SolidBrush(color);
    brushCache[colorValue] = brush;
  }

  if (center) {
    // Measure text to center it
    RectF boundingRect;
    graphics.MeasureString(text.c_str(), -1, font, PointF(0, 0), &boundingRect);
    PointF point((REAL)(x - boundingRect.Width / 2), (REAL)y);
    graphics.DrawString(text.c_str(), -1, font, point, brush);
  } else {
    PointF point((REAL)x, (REAL)y);
    graphics.DrawString(text.c_str(), -1, font, point, brush);
  }
}


#ifdef WINDOWINJECTION_EXPORTS

// https://docs.microsoft.com/en-us/windows/win32/dlls/dllmain
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ulReasonForCall,
                      LPVOID lpReserved) {
  // https://tbhaxor.com/loading-dlls-using-cpp-in-windows/
  switch (ulReasonForCall) {
  case DLL_PROCESS_ATTACH:
    // Initialize once for each new process.
    CreateThread(nullptr, 0, UwU, hModule, NULL, NULL);
    break;
  case DLL_THREAD_ATTACH:
    // Do thread-specific initialization.
    break;
  case DLL_THREAD_DETACH:
    // Do thread-specific cleanup.
    break;
  case DLL_PROCESS_DETACH:
    // Perform any necessary cleanup.
    PerformCleanupAndExit();
    break;
  default:
    break;
  }

  return TRUE;
}

#else

int main(int argc, char *argv[]) {
  HMODULE hInstance = nullptr;
  BOOL result =
      GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<char *>(&DefWindowProc), &hInstance);
  if (FALSE == result) {
    printf("Failed to GetModuleHandleExA, 0x%x\n", GetLastError());
    return 0;
  }

  return UwU(hInstance);
}

#endif

// https://github.com/nodejs/node-convergence-archive/blob/e11fe0c2777561827cdb7207d46b0917ef3c42a7/deps/uv/src/win/util.c#L780
BOOL IsWindowsVersionOrGreater(DWORD os_major, DWORD os_minor,
                               DWORD build_number, WORD service_pack_major,
                               WORD service_pack_minor) {
  OSVERSIONINFOEX osvi;
  DWORDLONG condition_mask = 0;
  int op = VER_GREATER_EQUAL;

  /* Initialize the OSVERSIONINFOEX structure. */
  ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  osvi.dwMajorVersion = os_major;
  osvi.dwMinorVersion = os_minor;
  osvi.dwBuildNumber = build_number;
  osvi.wServicePackMajor = service_pack_major;
  osvi.wServicePackMinor = service_pack_minor;

  /* Initialize the condition mask. */
  VER_SET_CONDITION(condition_mask, VER_MAJORVERSION, op);
  VER_SET_CONDITION(condition_mask, VER_MINORVERSION, op);
  VER_SET_CONDITION(condition_mask, VER_SERVICEPACKMAJOR, op);
  VER_SET_CONDITION(condition_mask, VER_SERVICEPACKMINOR, op);

  /* Perform the test. */
  return VerifyVersionInfo(&osvi,
                           VER_MAJORVERSION | VER_MINORVERSION |
                               VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
                           condition_mask);
}
