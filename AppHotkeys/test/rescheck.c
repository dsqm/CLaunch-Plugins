// Resource checker: loads a DLL as a data file, prints the dialog (ID 100)
// and version resource contents as readable text.
#include <windows.h>
#include <stdio.h>

static void DumpTextRuns(const wchar_t *ws, DWORD cch)
{
	wchar_t buf[512];
	DWORD nRun = 0;
	for (DWORD i = 0; i <= cch; i++) {
		if (i < cch && ws[i] >= 0x20 && ws[i] != 0x7F) {
			if (nRun < 500) buf[nRun++] = ws[i];
		}
		else {
			if (nRun >= 3) {
				buf[nRun] = 0;
				wprintf(L"  | %s\n", buf);
			}
			nRun = 0;
		}
	}
}

int wmain(int argc, wchar_t **argv)
{
	if (argc < 2) { wprintf(L"usage: rescheck.exe <dll>\n"); return 2; }
	HMODULE h = LoadLibraryExW(argv[1], NULL, LOAD_LIBRARY_AS_DATAFILE);
	if (!h) { wprintf(L"LoadLibraryEx failed: %u\n", GetLastError()); return 1; }

	HRSRC rDlg = FindResourceW(h, MAKEINTRESOURCEW(100), (LPCWSTR)RT_DIALOG);
	if (rDlg) {
		DWORD size = SizeofResource(h, rDlg);
		HGLOBAL g = LoadResource(h, rDlg);
		const wchar_t *ws = (const wchar_t *)LockResource(g);
		wprintf(L"dialog: size=%lu\n", size);
		if (ws && size) {
			DumpTextRuns(ws, size / 2);
			if (argc >= 3) {											// Optionally dump the raw bytes
				HANDLE hf = CreateFileW(argv[2], GENERIC_WRITE, 0, NULL,
												CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hf != INVALID_HANDLE_VALUE) {
					DWORD dwW = 0;
					WriteFile(hf, ws, size, &dwW, NULL);
					CloseHandle(hf);
					wprintf(L"dumped to %s\n", argv[2]);
				}
			}
		}
	}
	else {
		wprintf(L"dialog NOT found (err=%u)\n", GetLastError());
	}

	HRSRC rVer = FindResourceW(h, MAKEINTRESOURCEW(1), (LPCWSTR)RT_VERSION);
	if (rVer) {
		wprintf(L"version: size=%lu\n", SizeofResource(h, rVer));
	}
	else {
		wprintf(L"version NOT found (err=%u)\n", GetLastError());
	}
	FreeLibrary(h);
	return 0;
}
