// Mainly from [MobileShell](https://github.com/ADeltaX/MobileShell)

#include <string>
#include <tchar.h>

#include "pch.h"
#include "./bitmap_loader.h"
#include "./img.h"
#include "./input_blocker.h"

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

// TODO: Read the register table to get the path.
// Or use hard code bitmap data.
TCHAR g_bmpPath[256] = {
    0,
};

bool g_loadFromMemory = true;
bool g_enableInputBlocking = true;
bool g_enableCursorHiding = true;

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

BOOL IsWindowsVersionOrGreater(DWORD os_major, DWORD os_minor,
                               DWORD build_number, WORD service_pack_major,
                               WORD service_pack_minor);

LRESULT CALLBACK TrashParentWndProc(HWND hwnd, UINT message, WPARAM wParam,
                                    LPARAM lParam) {
  switch (message) {
  case WM_CREATE:
    break;

  case WM_DESTROY:
    PostQuitMessage(0);
    break;

  case WM_WINDOWPOSCHANGING:
    return 0;
  case WM_CLOSE:
    HANDLE myself;
    myself = OpenProcess(PROCESS_ALL_ACCESS, false, GetCurrentProcessId());
    TerminateProcess(myself, 0);
    return true;

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

void EmergencyExitCallback() {
  // Emergency exit triggered by multiple Escape key presses
#ifdef WINDOWINJECTION_EXPORTS
  MessageBox(NULL, _T("Emergency exit triggered. Closing privacy window."),
             _T("Privacy Window"), MB_OK);
#else
  _tprintf(_T("Emergency exit triggered. Closing privacy window.\n"));
#endif

  if (g_hwnd) {
    PostMessage(g_hwnd, WM_CLOSE, NULL, NULL);
  }
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
      GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT | WS_EX_LAYERED |
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
    g_inputBlocker.SetEmergencyExitCallback(EmergencyExitCallback);
    g_inputBlocker.SetAllowEscapeKey(false);
    g_inputBlocker.SetAllowCtrlAltDel(false);
    g_inputBlocker.SetAllowWinKey(false);
    g_inputBlocker.SetBlockMouse(true);
    g_inputBlocker.SetBlockKeyboard(true);

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
  }

#ifndef WINDOWINJECTION_EXPORTS
  ShowWindow(g_hwnd, SW_SHOW);
#else
  ShowWindow(g_hwnd, SW_SHOW);
#endif

  if (g_enableCursorHiding) {
    g_inputBlocker.HideCursor();
  }

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

  auto bitmap = g_bitmapLoader.GetBitmap();
  if (bitmap) {
    Gdiplus::SizeF sizef = Gdiplus::SizeF((Gdiplus::REAL)bitmap->GetWidth(),
                                          (Gdiplus::REAL)bitmap->GetHeight());

    Gdiplus::RectF rcClient = Gdiplus::RectF(Gdiplus::PointF(0, 0), sizef);

    Gdiplus::Graphics graphics(hdc);
    graphics.DrawImage(bitmap, rcClient);
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
    if (g_hwnd) {
      PostMessage(g_hwnd, WM_CLOSE, NULL, NULL);
    }
    // MediaLoader temporarily disabled
    if (g_enableInputBlocking) {
      g_inputBlocker.StopBlocking();
    }
    if (g_enableCursorHiding) {
      g_inputBlocker.ShowCursor();
    }
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
