#include <stdio.h>
#include <Shlwapi.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include <chrono>

#pragma comment(lib, "shlwapi.lib")


typedef WINBASEAPI BOOL(WINAPI* CreateProcessHid)(
	_In_opt_ LPCWSTR lpApplicationName,
	_Inout_opt_ LPWSTR lpCommandLine,
	_In_opt_ LPSECURITY_ATTRIBUTES lpProcessAttributes,
	_In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
	_In_ BOOL bInheritHandles,
	_In_ DWORD dwCreationFlags,
	_In_opt_ LPVOID lpEnvironment,
	_In_opt_ LPCWSTR lpCurrentDirectory,
	_In_ LPSTARTUPINFOW lpStartupInfo,
	_Out_ LPPROCESS_INFORMATION lpProcessInformation
	);

// Test media files for demonstration
const TCHAR* TestImageFiles[] = {
	_T("C:\\test\\image.png"),
	_T("C:\\test\\animation.gif"),
	_T("C:\\test\\video.mp4"),
};

void TestInputBlocking(HWND hwnd);
void TestMediaPlayback(HWND hwnd);

void PrintError(const TCHAR* header)
{
	TCHAR msg[256] = { 0, };
	DWORD code = GetLastError();
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		msg, (sizeof(msg) / sizeof(wchar_t)), NULL);

	TCHAR buf[1024] = { 0, };
	_sntprintf_s(buf, sizeof(buf) / sizeof(buf[0]), _TRUNCATE, _T("Failed %s, 0x%x, %s"), header, code, msg);
	_tprintf(buf);
}

BOOL InjectDll(HANDLE hProcess, HANDLE hThread, const TCHAR* path)
{
	size_t strSize = (_tcslen(path) + 1) * sizeof(TCHAR);
	LPVOID pBuf = VirtualAllocEx(hProcess, 0, strSize, MEM_COMMIT, PAGE_READWRITE);
	if (pBuf == NULL)
	{
		PrintError(_T("VirtualAllocEx"));
		return FALSE;
	}

	SIZE_T written;
	if (!WriteProcessMemory(hProcess, pBuf, path, strSize, &written))
	{
		PrintError(_T("WriteProcessMemory"));
		return FALSE;
	}

	HMODULE hmodule = GetModuleHandle(_T("kernel32"));
	if (NULL == hmodule)
	{
		PrintError(_T("GetModuleHandle"));
		return FALSE;
	}

#ifdef _UNICODE
	LPVOID pLoadLibrary = GetProcAddress(hmodule, "LoadLibraryW");
#else
	LPVOID pLoadLibrary = GetProcAddress(hmodule, "LoadLibraryA");
#endif
	if (NULL == pLoadLibrary)
	{
		PrintError(_T("GetProcAddress"));
		return FALSE;
	}

	DWORD APCRet = QueueUserAPC((PAPCFUNC)pLoadLibrary, hThread, (ULONG_PTR)pBuf);
	if (0 == APCRet)
	{
		PrintError(_T("QueueUserAPC"));
		return FALSE;
	}
	return TRUE;
}

BOOL GetExecutableDir(TCHAR* dir, int maxLen)
{
	if (0 == GetModuleFileName(nullptr, dir, maxLen))
	{
		PrintError(_T("GetModuleFileName"));
		return FALSE;
	}
	PathRemoveFileSpec(dir);
	return TRUE;
}

typedef BOOL(WINAPI* SetWindowBand)(HWND hWnd, HWND hwndInsertAfter, DWORD dwBand);

int main(int argc, char* argv[])
{
	TCHAR dir[MAX_PATH] = { 0, };
	if (FALSE == GetExecutableDir(dir, sizeof(dir) / sizeof(dir[0])))
	{
		return 1;
	}

	TCHAR path[MAX_PATH] = { 0, };
	_sntprintf_s(path, sizeof(path) / sizeof(path[0]), _TRUNCATE, _T("%s\\WindowInjection.dll"), dir);

	STARTUPINFO startInfo = { 0 };
	PROCESS_INFORMATION procInfo = { 0 };

	//TCHAR cmdline[MAX_PATH] = { 0, };
	// _sntprintf_s(cmdline, sizeof(cmdline) / sizeof(cmdline[0]), _TRUNCATE, _T("%s\\MiniBroker.exe"), dir);
	TCHAR cmdline[] = L"C:\\Windows\\System32\\RuntimeBroker.exe";

	startInfo.cb = sizeof(startInfo);

	if (CreateProcess(nullptr, cmdline, nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &startInfo, &procInfo))
	{
		(VOID)InjectDll(procInfo.hProcess, procInfo.hThread, path);
		ResumeThread(procInfo.hThread);
	}
	else
	{
		PrintError(_T("CreateProcess"));
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(2 * 1000));
	const TCHAR* WindowTitle = _T("RustDeskPrivacyWindow");
	HWND hwnd = FindWindow(NULL, WindowTitle);
	if (hwnd == NULL)
	{
		PrintError(_T("FindWindow"));
	}
	else
	{
		printf("\n=== Privacy Window Found - Running Tests ===\n");
		
		// Test basic window visibility
		printf("Test 1: Basic window visibility controls\n");
		printf("Hiding window for 2 seconds...\n");
		ShowWindow(hwnd, SW_HIDE);
		std::this_thread::sleep_for(std::chrono::milliseconds(2 * 1000));

		printf("Showing window again...\n");
		ShowWindow(hwnd, SW_SHOW);
		std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));

		// Test input blocking functionality
		TestInputBlocking(hwnd);
		
		// Test media playback functionality
		TestMediaPlayback(hwnd);

		printf("\nAll tests completed. Window will close in 3 seconds...\n");
		printf("During these 3 seconds, try clicking or pressing keys to test blocking.\n");
		printf("Press Escape 5 times quickly to trigger emergency exit.\n");
		std::this_thread::sleep_for(std::chrono::milliseconds(3 * 1000));
		
		printf("Closing privacy window...\n");
		PostMessage(hwnd, WM_CLOSE, NULL, NULL);
	}

	return 0;
}

void TestInputBlocking(HWND hwnd)
{
	printf("\n=== Testing Input Blocking Features ===\n");
	
	// Get current cursor position
	POINT cursorPos;
	GetCursorPos(&cursorPos);
	printf("Initial cursor position: (%d, %d)\n", cursorPos.x, cursorPos.y);
	
	// Check if cursor is hidden
	CURSORINFO ci;
	ci.cbSize = sizeof(CURSORINFO);
	if (GetCursorInfo(&ci))
	{
		printf("Cursor visibility flags: 0x%x (0 = hidden, 1 = visible)\n", ci.flags);
		if (ci.flags == 0)
		{
			printf("✓ Cursor is successfully hidden\n");
		}
		else
		{
			printf("! Cursor is still visible\n");
		}
	}
	
	printf("Test 2: Input blocking is now active\n");
	printf("Try the following (they should be blocked):\n");
	printf("  - Move mouse\n");
	printf("  - Click mouse buttons\n");
	printf("  - Press keyboard keys\n");
	printf("  - Alt+Tab, Win key, etc.\n");
	printf("Observing for 5 seconds...\n");
	
	// Monitor for 5 seconds
	auto startTime = std::chrono::steady_clock::now();
	POINT lastPos = cursorPos;
	bool mouseMovementDetected = false;
	
	while (std::chrono::steady_clock::now() - startTime < std::chrono::seconds(5))
	{
		POINT currentPos;
		GetCursorPos(&currentPos);
		if (currentPos.x != lastPos.x || currentPos.y != lastPos.y)
		{
			mouseMovementDetected = true;
			printf("! Mouse movement detected: (%d, %d) -> (%d, %d)\n", 
				lastPos.x, lastPos.y, currentPos.x, currentPos.y);
			lastPos = currentPos;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	
	if (!mouseMovementDetected)
	{
		printf("✓ Mouse input successfully blocked\n");
	}
	
	printf("=== Input Blocking Test Complete ===\n");
}

void TestMediaPlayback(HWND hwnd)
{
	printf("\n=== Testing Media Playback Features ===\n");
	
	// Test 1: Window positioning and sizing
	printf("Test 1: Adjusting window position and size\n");
	RECT rect;
	if (GetWindowRect(hwnd, &rect))
	{
		int width = rect.right - rect.left;
		int height = rect.bottom - rect.top;
		printf("Current window size: %dx%d\n", width, height);
		
		// Move window to center of primary monitor
		int screenWidth = GetSystemMetrics(SM_CXSCREEN);
		int screenHeight = GetSystemMetrics(SM_CYSCREEN);
		int newX = (screenWidth - width) / 2;
		int newY = (screenHeight - height) / 2;
		
		SetWindowPos(hwnd, HWND_TOPMOST, newX, newY, width, height, SWP_SHOWWINDOW);
		printf("Moved window to center: (%d, %d)\n", newX, newY);
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
	}
	
	// Test 2: Animation timing test
	printf("Test 2: Animation timing test (if animated GIF is loaded)\n");
	printf("Observing animation for 5 seconds...\n");
	auto startTime = std::chrono::steady_clock::now();
	int frameCount = 0;
	
	while (std::chrono::steady_clock::now() - startTime < std::chrono::seconds(5))
	{
		// Force window repaint to see animation
		InvalidateRect(hwnd, NULL, TRUE);
		UpdateWindow(hwnd);
		frameCount++;
		std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 10 FPS update
	}
	
	printf("Completed animation test with %d frame updates\n", frameCount);
	
	// Test 3: Window transparency test
	printf("Test 3: Testing window transparency\n");
	BYTE currentAlpha = 255;
	
	for (int i = 0; i < 10; i++)
	{
		currentAlpha -= 25;
		SetLayeredWindowAttributes(hwnd, 0, currentAlpha, LWA_ALPHA);
		printf("Set alpha to %d\n", currentAlpha);
		std::this_thread::sleep_for(std::chrono::milliseconds(300));
	}
	
	// Restore full opacity
	SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
	printf("Restored full opacity\n");
	
	printf("=== Media Playback Tests Complete ===\n\n");
}
