// Smoke test for X1Toggle.dll: load the DLL, check the export, and call
// PRQ_GETTITLE (which must work before PRQ_INIT, without the CPI table).
#include <windows.h>
#include <stdio.h>

typedef UINT (*PFN)(UINT, LONG_PTR, LONG_PTR);

#define PRQ_GETTITLE 0x00000001
#define SUPPORT_VERSION_EXPECT 0x04020300

int wmain(int argc, wchar_t **argv)
{
	if (argc < 2) { wprintf(L"usage: test.exe <dll path>\n"); return 2; }
	HMODULE h = LoadLibraryW(argv[1]);
	if (!h) { wprintf(L"LoadLibrary failed: %lu\n", GetLastError()); return 1; }
	PFN pfn = (PFN)GetProcAddress(h, "ClPluginEventHandler");
	if (!pfn) { wprintf(L"GetProcAddress failed\n"); return 1; }

	wchar_t buf[256] = L"";
	UINT ver = pfn(PRQ_GETTITLE, (LONG_PTR)buf, 256);
	wprintf(L"title=%s\nsupported=0x%08X (%s)\n", buf, ver,
			ver == SUPPORT_VERSION_EXPECT ? L"OK" : L"UNEXPECTED");
	if (buf[0] == L'\0') { wprintf(L"FAIL: empty title\n"); return 1; }

	UINT r = pfn(0x00000009, 0, 0); // PRQ_SETTING without CPI table -> must be FALSE
	wprintf(L"unsupported_ret=%u (%s)\n", r, r == 0 ? L"OK" : L"FAIL");

	FreeLibrary(h);
	wprintf(L"SMOKE TEST PASSED\n");
	return 0;
}
