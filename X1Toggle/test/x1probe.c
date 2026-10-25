// E2E probe for the X1Toggle plugin test:
//  - counts visible top-level windows of the target process (CLaunch)
//  - injects XButton1 press/release pairs via SendInput
// Usage: x1probe.exe <pid> [presses]
#include <windows.h>
#include <stdio.h>

static int	g_nCount;
static DWORD g_dwTargetPid;

static BOOL CALLBACK CountVisibleWindows(HWND hWnd, LPARAM lParam)
{
	DWORD dwPid = 0;
	GetWindowThreadProcessId(hWnd, &dwPid);
	if (dwPid == g_dwTargetPid && IsWindowVisible(hWnd)) {
		g_nCount++;
	}
	return TRUE;
}

static int CountVisible(void)
{
	g_nCount = 0;
	EnumWindows(CountVisibleWindows, 0);
	return g_nCount;
}

static void PressX1(void)
{
	INPUT in[1] = { 0 };
	in[0].type = INPUT_MOUSE;
	in[0].mi.dwFlags = MOUSEEVENTF_XDOWN;
	in[0].mi.mouseData = XBUTTON1;
	SendInput(1, in, sizeof(INPUT));
	Sleep(80);
	in[0].mi.dwFlags = MOUSEEVENTF_XUP;
	SendInput(1, in, sizeof(INPUT));
}

int wmain(int argc, wchar_t **argv)
{
	if (argc < 2) { wprintf(L"usage: x1probe.exe <pid> [presses]\n"); return 2; }
	g_dwTargetPid = (DWORD)_wtoi(argv[1]);
	int nPresses = (argc >= 3) ? _wtoi(argv[2]) : 4;
	wprintf(L"target pid=%lu, presses=%d\n", g_dwTargetPid, nPresses);
	for (int i = 0; i < nPresses; i++) {
		int before = CountVisible();
		PressX1();
		Sleep(700);
		int after = CountVisible();
		wprintf(L"press %d: visible windows %d -> %d\n", i + 1, before, after);
	}
	return 0;
}
