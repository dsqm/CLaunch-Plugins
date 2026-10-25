// String table dumper: prints all STRINGTABLE entries (ID + text) of a DLL,
// written as UTF-8 so CJK text survives console redirection.
#include <windows.h>
#include <stdio.h>

static FILE *g_fp;

static BOOL CALLBACK EnumStrTable(HMODULE h, LPCWSTR type, LPWSTR name, LONG_PTR lp)
{
	HRSRC r = FindResourceW(h, name, (LPCWSTR)RT_STRING);
	if (!r) return TRUE;
	HGLOBAL g = LoadResource(h, r);
	WORD *p = (WORD *)LockResource(g);
	if (!p) return TRUE;
	UINT idBase = (((UINT)(ULONG_PTR)name) - 1) * 16;
	for (int i = 0; i < 16; i++) {
		WORD len = p[0];
		if (len > 0) {
			wchar_t buf[1024];
			int n = WideCharToMultiByte(CP_UTF8, 0, p + 1, len, buf, sizeof(buf), NULL, NULL);
			if (n > 0) {
				buf[n] = 0;
				fprintf(g_fp, "%u\t%s\n", idBase + (UINT)i, buf);
			}
		}
		p += 1 + len;
	}
	return TRUE;
}

int wmain(int argc, wchar_t **argv)
{
	if (argc < 2) { wprintf(L"usage: strdump.exe <dll> [out.txt]\n"); return 2; }
	HMODULE h = LoadLibraryExW(argv[1], NULL, LOAD_LIBRARY_AS_DATAFILE);
	if (!h) { wprintf(L"LoadLibraryEx failed: %u\n", GetLastError()); return 1; }
	g_fp = stdout;
	if (argc >= 3) {
		g_fp = _wfopen(argv[2], L"wb");
		if (!g_fp) return 1;
		fprintf(g_fp, "\xef\xbb\xbf");
	}
	EnumResourceNamesW(h, (LPCWSTR)RT_STRING, (ENUMRESNAMEPROCW)EnumStrTable, 0);
	FreeLibrary(h);
	if (g_fp != stdout) fclose(g_fp);
	return 0;
}
