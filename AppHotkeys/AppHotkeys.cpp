//==============================================================================
//
//			CLaunch Plugin : App Hotkeys
//
//			While the CLaunch window is displayed, pressing one of the
//			configured hotkeys starts the program assigned to it in the
//			plugin settings, without navigating to the page that holds it.
//
//			- Up to 8 entries: hotkey + program path (+ parameters)
//			- Paths support environment variables and relative paths
//			  (resolved against the CLaunch folder, then the data folder)
//			- CLaunch is hidden after a launch, like a normal item execution
//			- Keys pass through untouched while the window is hidden, and
//			  entries with an empty hotkey or path are ignored
//
//==============================================================================
#include "AppHotkeys.h"
#include "resource.h"
#include <string>
#include <strsafe.h>
#pragma comment (lib, "shell32.lib")

using std::wstring;

//------------------------------------------------------------------------------
// Plugin title shown in the CLaunch plugin list
//------------------------------------------------------------------------------
#define PLUGIN_TITLE		_T("App Hotkeys")

//------------------------------------------------------------------------------
// Number of configurable hotkey entries
//------------------------------------------------------------------------------
#define APPHOT_COUNT		8

//------------------------------------------------------------------------------
// Key strings for the plugin settings (indexed: Hotkey0..7, Path0..7, Param0..7)
// Legacy keys of the former single-entry versions are migrated on load.
//------------------------------------------------------------------------------
static const LPCTSTR KEY_HOTKEY	= _T("Hotkey%d");
static const LPCTSTR KEY_PATH	= _T("Path%d");
static const LPCTSTR KEY_PARAM	= _T("Param%d");
static const LPCTSTR KEY_LEGACY_PATH	= _T("TargetPath");
static const LPCTSTR KEY_LEGACY_PARAM	= _T("TargetParam");
static const LPCTSTR KEY_LEGACY_HOTKEY	= _T("Hotkey");

//------------------------------------------------------------------------------
// Global state
//------------------------------------------------------------------------------
WORD		g_wHotkeys[APPHOT_COUNT] = { 0 };								// Configured hotkeys (0x0MVK, 0: unused)
wstring		g_sPaths[APPHOT_COUNT];											// Configured program paths
wstring		g_sParams[APPHOT_COUNT];										// Optional launch parameters
BOOL		g_bVisible		= FALSE;										// CLaunch window visibility (PEV_SHOW/PEV_HIDE)
WORD		g_wLastSwallowed	= 0;										// VK code of the currently held swallowed key
HHOOK		g_hKeyHook		= NULL;											// Low-level keyboard hook handle

//------------------------------------------------------------------------------
// Diagnostic log (written next to the DLL as AppHotkeys.log)
// * Off by default. Set AHD_DEBUG_LOG to 1 to enable it while debugging.
//------------------------------------------------------------------------------
#define AHD_DEBUG_LOG	0

#if AHD_DEBUG_LOG
void ClDebugLog(LPCTSTR pszText)
{
	TCHAR szPath[MAX_PATH];
	if (!GetModuleFileName(g_hInst, szPath, ARRAYSIZE(szPath))) {
		return;
	}
	LPTSTR pszName = _tcsrchr(szPath, _T('\\'));							// Find the file name part
	if (!pszName) {
		return;
	}
	pszName[1] = _T('\0');													// Keep the directory part only
	StringCchCat(szPath, ARRAYSIZE(szPath), _T("AppHotkeys.log"));
	WIN32_FILE_ATTRIBUTE_DATA fad;
	BOOL bEmpty = !GetFileAttributesEx(szPath, GetFileExInfoStandard, &fad) ||
						(fad.nFileSizeHigh == 0 && fad.nFileSizeLow == 0);
	HANDLE hFile = CreateFile(szPath, FILE_APPEND_DATA,
									FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
									OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return;
	}
	DWORD dwWritten = 0;
	if (bEmpty) {
		WriteFile(hFile, "\xef\xbb\xbf", 3, &dwWritten, NULL);				// UTF-8 BOM
	}
	TCHAR szLine[512];
	StringCchPrintf(szLine, ARRAYSIZE(szLine), _T("[%8u] %s\r\n"),
							GetTickCount(), pszText);
	char szUtf8[2048];
	int nLen = WideCharToMultiByte(CP_UTF8, 0, szLine, -1,
											szUtf8, ARRAYSIZE(szUtf8), NULL, NULL);
	if (nLen > 1) {
		WriteFile(hFile, szUtf8, (DWORD)(nLen - 1), &dwWritten, NULL);		// Exclude the terminating NUL
	}
	CloseHandle(hFile);
}
#else
#define ClDebugLog(pszText)	((void)0)
#endif

//==============================================================================
// Description : Get the CLaunch.exe directory
//			   :
// Return	   : Directory path with a trailing backslash (empty on failure)
//==============================================================================
static wstring ClGetCLaunchDir(void)
{
	wchar_t szPath[MAX_PATH];
	DWORD dwLen = GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath));		// CLaunch.exe (we run in its process)
	if (dwLen == 0 || dwLen >= ARRAYSIZE(szPath)) {
		return L"";
	}
	wstring sDir(szPath, dwLen);
	size_t nPos = sDir.find_last_of(L'\\');
	if (nPos == wstring::npos) {
		return L"";
	}
	return sDir.substr(0, nPos + 1);
}

//==============================================================================
// Description : Resolve an entry's program path
//			   : - Expands environment variables
//			   : - Relative paths are resolved against the CLaunch folder,
//			   :   then against the data folder
//			   :
// Parameters  : iEntry  Entry index
//			   : psFile  Receives the resolved path
//			   :
// Return	   : TRUE : Resolved
//			   : FALSE: Nothing usable configured
//==============================================================================
static BOOL ClResolveTarget(int iEntry, wstring *psFile)
{
	wstring s = g_sPaths[iEntry];
	while (!s.empty() && (s.front() == L'"' || s.front() == L' ')) {
		s.erase(s.begin());													// Strip leading quote / space
	}
	while (!s.empty() && (s.back() == L'"' || s.back() == L' ')) {
		s.pop_back();														// Strip trailing quote / space
	}
	if (s.empty()) {
		return FALSE;
	}
	wchar_t szExp[MAX_PATH * 2];
	if (ExpandEnvironmentStringsW(s.c_str(), szExp, ARRAYSIZE(szExp)) == 0 ||
			szExp[0] == L'\0') {
		return FALSE;
	}
	s = szExp;
	BOOL bRelative = !(s.size() >= 2 && s[1] == L':') &&
						s[0] != L'\\' && s[0] != L'/';
	if (bRelative) {
		wstring sCand = ClGetCLaunchDir() + s;								// Relative to the CLaunch folder
		if (GetFileAttributesW(sCand.c_str()) != INVALID_FILE_ATTRIBUTES) {
			s = sCand;
		}
		else {
			LPCTSTR pszData = CpiGetDataFolder();							// Relative to the data folder
			if (pszData && *pszData) {
				wstring sData = wstring(pszData) + L"\\" + s;
				if (GetFileAttributesW(sData.c_str()) != INVALID_FILE_ATTRIBUTES) {
					s = sData;
				}
			}
		}
	}
	if (s.empty()) {
		return FALSE;
	}
	*psFile = s;
	return TRUE;
}

//==============================================================================
// Description : Check whether the modifier keys match an entry's hotkey
//			   : - The modifier bits live in the high byte of the 0x0MVK
//			   :   hotkey word and must be masked from the full WORD
//			   :   (MKBIT_xxx are 0x0100 - 0x0800)
//==============================================================================
static BOOL ClMatchMods(WORD wHotkey)
{
	BOOL bCtrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? TRUE : FALSE;
	BOOL bShift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? TRUE : FALSE;
	BOOL bAlt = (GetAsyncKeyState(VK_MENU) & 0x8000) ? TRUE : FALSE;
	BOOL bWin = ((GetAsyncKeyState(VK_LWIN) & 0x8000) ||
					(GetAsyncKeyState(VK_RWIN) & 0x8000)) ? TRUE : FALSE;
	BOOL wCtrl = (wHotkey & MKBIT_CTRL) ? TRUE : FALSE;
	BOOL wShift = (wHotkey & MKBIT_SHIFT) ? TRUE : FALSE;
	BOOL wAlt = (wHotkey & MKBIT_ALT) ? TRUE : FALSE;
	BOOL wWin = (wHotkey & MKBIT_WIN) ? TRUE : FALSE;
#if AHD_DEBUG_LOG
	{
		TCHAR szMsg[192];
		StringCchPrintf(szMsg, ARRAYSIZE(szMsg),
								_T("mod state: ctrl=%d shift=%d alt=%d win=%d / want: ctrl=%d shift=%d alt=%d win=%d"),
								(int)bCtrl, (int)bShift, (int)bAlt, (int)bWin,
								(int)wCtrl, (int)wShift, (int)wAlt, (int)wWin);
		ClDebugLog(szMsg);
	}
#endif
	return (bCtrl == wCtrl && bShift == wShift && bAlt == wAlt && bWin == wWin);
}

//==============================================================================
// Description : Start the program of an entry
//			   : - CLaunch is hidden afterwards, like a normal item execution
//			   :
// Return	   : TRUE : Started (the key must be swallowed)
//			   : FALSE: Nothing launched (the key passes through)
//==============================================================================
static BOOL ClLaunchEntry(int iEntry)
{
	wstring sFile;
	if (!ClResolveTarget(iEntry, &sFile)) {
		ClDebugLog(_T("hotkey pressed: target path is invalid -> passthrough"));
		return FALSE;
	}
	ClDebugLog(_T("hotkey pressed: starting target"));
	SHELLEXECUTEINFOW sei = { sizeof(SHELLEXECUTEINFOW) };
	sei.lpFile = sFile.c_str();
	sei.lpParameters = g_sParams[iEntry].empty() ? NULL : g_sParams[iEntry].c_str();
	sei.nShow = SW_SHOWDEFAULT;
	BOOL bOk = ShellExecuteExW(&sei);
#if AHD_DEBUG_LOG
	if (!bOk || (UINT_PTR)sei.hInstApp <= 32) {
		TCHAR szMsg[160];
		StringCchPrintf(szMsg, ARRAYSIZE(szMsg),
								_T("ShellExecuteEx FAILED: bOk=%d, err=%lu, hInstApp=%p"),
								(int)bOk, GetLastError(), (void *)sei.hInstApp);
		ClDebugLog(szMsg);
	}
#endif
	if (bOk) {
		CpiHideWindow();													// Hide CLaunch like a normal item execution
	}
	return TRUE;
}

//==============================================================================
// Description : Low-level keyboard hook procedure
//			   : - Acts only while the CLaunch window is displayed
//			   : - Key repeats of a swallowed down are swallowed as well
//			   :   without relaunching
//			   : - Must return quickly; do not add slow processing here
//==============================================================================
LRESULT CALLBACK ClLowLevelKeyProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION) {
		PKBDLLHOOKSTRUCT pKey = (PKBDLLHOOKSTRUCT)lParam;
		WORD vk = (WORD)pKey->vkCode;
#if AHD_DEBUG_LOG
		//--------------------------------------------------------------------
		// Log every event of a configured base key with the full state
		//--------------------------------------------------------------------
		BOOL bBaseKey = FALSE;
		for (int i = 0; i < APPHOT_COUNT; i++) {
			if (g_wHotkeys[i] != 0 && vk == GetVK(g_wHotkeys[i])) {
				bBaseKey = TRUE;
				break;
			}
		}
		if (bBaseKey) {
			TCHAR szMsg[160];
			StringCchPrintf(szMsg, ARRAYSIZE(szMsg),
									_T("KEY %s vk=0x%02X: visible=%d, lastSwallowed=0x%02X"),
									(wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) ?
											_T("down") : _T("up"),
									vk, (int)g_bVisible, g_wLastSwallowed);
			ClDebugLog(szMsg);
		}
#endif
		//--------------------------------------------------------------------
		// Release the repeat guard on the key up regardless of visibility:
		// after a launch the window hides before the key is released, so the
		// up event always arrives while the window is hidden. The up belongs
		// to a swallowed down and must be swallowed as well.
		//--------------------------------------------------------------------
		if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
			if (vk == g_wLastSwallowed) {
				g_wLastSwallowed = 0;										// Release the repeat guard
				ClDebugLog(_T("swallowed matching key up"));
				return 1;													// Swallow the matching up
			}
		}
		//--------------------------------------------------------------------
		// Launch entries only while the CLaunch window is displayed
		//--------------------------------------------------------------------
		else if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && g_bVisible) {
			for (int i = 0; i < APPHOT_COUNT; i++) {
				if (g_wHotkeys[i] == 0 || g_sPaths[i].empty() ||
						vk != GetVK(g_wHotkeys[i]) || !ClMatchMods(g_wHotkeys[i])) {
					continue;
				}
				if (vk == g_wLastSwallowed) {								// Key repeat of a swallowed down
					ClDebugLog(_T("swallowed as key repeat"));
					return 1;
				}
				if (ClLaunchEntry(i)) {
					g_wLastSwallowed = vk;									// Remember for the repeat keys
					return 1;												// Swallow the key
				}
			}
		}
	}
	return CallNextHookEx(g_hKeyHook, nCode, wParam, lParam); 				// Pass the key through
}

//==============================================================================
// Description : Install the low-level keyboard hook (release the previous
//			   : one if it is still installed)
//			   : - Low-level hooks are called from the newest to the oldest;
//			   :   re-installing keeps this plugin's hook ahead of
//			   :   ClHook.dll's hook, so the trigger keys can be swallowed
//==============================================================================
void ClInstallKeyHook(void)
{
	if (g_hKeyHook) {
		UnhookWindowsHookEx(g_hKeyHook);
		g_hKeyHook = NULL;
	}
	g_hKeyHook = SetWindowsHookEx(WH_KEYBOARD_LL, ClLowLevelKeyProc, g_hInst, 0);
#if AHD_DEBUG_LOG
	{
		TCHAR szMsg[128];
		StringCchPrintf(szMsg, ARRAYSIZE(szMsg), _T("HOOK install: %s (err=%lu)"),
								g_hKeyHook ? _T("ok") : _T("FAILED"),
								g_hKeyHook ? 0 : GetLastError());
		ClDebugLog(szMsg);
	}
#endif
}

//==============================================================================
// Description : Retrieve setting data
//==============================================================================
static void ClRetrieveSettings(void)
{
	TCHAR szKey[32];
	for (int i = 0; i < APPHOT_COUNT; i++) {
		StringCchPrintf(szKey, ARRAYSIZE(szKey), KEY_HOTKEY, i);
		g_wHotkeys[i] = (WORD)CpiGetIntData(NULL, szKey, 0);
		StringCchPrintf(szKey, ARRAYSIZE(szKey), KEY_PATH, i);
		LPCTSTR pszPath = CpiGetStrData(NULL, szKey);
		if (pszPath) {
			g_sPaths[i] = pszPath;
		}
		StringCchPrintf(szKey, ARRAYSIZE(szKey), KEY_PARAM, i);
		LPCTSTR pszParam = CpiGetStrData(NULL, szKey);
		if (pszParam) {
			g_sParams[i] = pszParam;
		}
	}
	//--------------------------------------------------------------------------
	// Migrate the single-entry configuration of the former versions
	//--------------------------------------------------------------------------
	LPCTSTR pszLegacyPath = CpiGetStrData(NULL, KEY_LEGACY_PATH);
	if (pszLegacyPath && *pszLegacyPath && g_sPaths[0].empty()) {
		LPCTSTR pszLegacyParam = CpiGetStrData(NULL, KEY_LEGACY_PARAM);
		g_sPaths[0] = pszLegacyPath;
		if (pszLegacyParam) {
			g_sParams[0] = pszLegacyParam;
		}
		g_wHotkeys[0] = (WORD)CpiGetIntData(NULL, KEY_LEGACY_HOTKEY, 0);
		ClDebugLog(_T("legacy single-entry configuration migrated to entry 0"));
	}
}

//==============================================================================
// Description : Store setting data
//==============================================================================
static void ClStoreSettings(void)
{
	TCHAR szKey[32];
	for (int i = 0; i < APPHOT_COUNT; i++) {
		StringCchPrintf(szKey, ARRAYSIZE(szKey), KEY_HOTKEY, i);
		CpiSetIntData(NULL, szKey, (int)g_wHotkeys[i]);
		StringCchPrintf(szKey, ARRAYSIZE(szKey), KEY_PATH, i);
		CpiSetStrData(NULL, szKey, g_sPaths[i].c_str());
		StringCchPrintf(szKey, ARRAYSIZE(szKey), KEY_PARAM, i);
		CpiSetStrData(NULL, szKey, g_sParams[i].c_str());
	}
	CpiSaveSettings();														// Save settings to file
#if AHD_DEBUG_LOG
	for (int i = 0; i < APPHOT_COUNT; i++) {
		if (g_wHotkeys[i] != 0 && !g_sPaths[i].empty()) {
			TCHAR szMsg[512];
			StringCchPrintf(szMsg, ARRAYSIZE(szMsg),
									_T("entry %d stored: hotkey=0x%04X, path=\"%s\", param=\"%s\""),
									i, g_wHotkeys[i], g_sPaths[i].c_str(), g_sParams[i].c_str());
			ClDebugLog(szMsg);
		}
	}
#endif
}

//==============================================================================
// Description : Plugin initialization
//			   :
// Parameters  : id 	 Plugin registration ID (-1: temporary registration)
//			   : pvFunc  Pointer to the CPI function address table
//			   :
// Return	   : TRUE : Succeeded
//			   : FALSE: Failed
//==============================================================================
BOOL ClPluginInit(int id, LPVOID *pvFunc)
{
	//--------------------------------------------------------------------------
	// Check CLaunch version
	//--------------------------------------------------------------------------
	g_pCpiFunc = pvFunc;													// Save pointer to CPI function address table
	if (CpiGetVersion() < SUPPORT_VERSION) {								// Is the core version too old?
		ClDebugLog(_T("INIT: CLaunch core version too old -> FALSE"));
		return FALSE;														// Abort initialization
	}
	if (id < 0) {															// Temporary registration?
		ClDebugLog(_T("INIT: temporary registration (id < 0)"));
		return TRUE;														// Return success
	}
	//--------------------------------------------------------------------------
	// Real registration
	//--------------------------------------------------------------------------
	g_iPluginId = id;
	ClRetrieveSettings();													// Retrieve setting data
	ClInstallKeyHook();														// Install the keyboard hook
#if AHD_DEBUG_LOG
	{
		TCHAR szMsg[160];
		StringCchPrintf(szMsg, ARRAYSIZE(szMsg),
								_T("INIT: real registration id=%d"), id);
		ClDebugLog(szMsg);
	}
#endif
	return TRUE;
}

//==============================================================================
// Description : Plugin termination
//			   :
// Return	   : TRUE : Succeeded
//			   : FALSE: Failed
//==============================================================================
BOOL ClPluginExit(void)
{
	if (g_hKeyHook) {
		UnhookWindowsHookEx(g_hKeyHook);									// Must release before the DLL is unloaded
		g_hKeyHook = NULL;
	}
	for (int i = 0; i < APPHOT_COUNT; i++) {
		g_sPaths[i].clear();
		g_sParams[i].clear();
		g_wHotkeys[i] = 0;
	}
	g_bVisible = FALSE;
	g_wLastSwallowed = 0;
	g_iPluginId = -1;														// Reset plugin ID
	ClDebugLog(_T("EXIT"));
	return TRUE;															// Succeeded
}

//==============================================================================
// Description : Plugin settings dialog procedure
//			   :
// Parameters  : hDlg	 Window handle
//			   : uMsg	 Message
//			   : wParam  wParam
//			   : lParam  lParam
//			   :
// Return	   : Result of message processing
//==============================================================================
LRESULT CALLBACK ClPluginSettingDlg(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	  //------------------------------------------------------------------------
	  // Dialog initialization
	  //------------------------------------------------------------------------
	  case WM_INITDIALOG:
		for (int i = 0; i < APPHOT_COUNT; i++) {
			SetDlgItemText(hDlg, IDC_PATH0 + i, g_sPaths[i].c_str());		// Current program path
			SetDlgItemText(hDlg, IDC_PARAM0 + i, g_sParams[i].c_str());		// Current parameters
			CpiCreateHotkeyEdit(hDlg, IDC_HOTKEY0 + i, g_wHotkeys[i]);		// Current trigger hotkey
		}
		return TRUE;														// No control has been given focus
	  //------------------------------------------------------------------------
	  // WM_COMMAND
	  //------------------------------------------------------------------------
	  case WM_COMMAND:
		switch (LOWORD(wParam)) {
		  //--------------------------------------------------------------------
		  // OK Button
		  //--------------------------------------------------------------------
		  case IDOK:
			{
				TCHAR szBuf[MAX_PATH * 2];
				for (int i = 0; i < APPHOT_COUNT; i++) {
					GetDlgItemText(hDlg, IDC_PATH0 + i, szBuf, ARRAYSIZE(szBuf));
					g_sPaths[i] = szBuf;
					GetDlgItemText(hDlg, IDC_PARAM0 + i, szBuf, ARRAYSIZE(szBuf));
					g_sParams[i] = szBuf;
					g_wHotkeys[i] = CpiGetKeyFromHotkeyEdit(hDlg, IDC_HOTKEY0 + i);
				}
				ClStoreSettings();											// Save settings to file
			}
			EndDialog(hDlg, TRUE);											// Close dialog box
			break;
		  //--------------------------------------------------------------------
		  // Cancel Button
		  //--------------------------------------------------------------------
		  case IDCANCEL:
			EndDialog(hDlg, FALSE); 										// Close dialog box
			break;
		}
		return TRUE;														// Message processed
	}
	return FALSE;															// Message not processed
}

//==============================================================================
// Description : Plugin event handler (exported function)
//			   :
// Parameters  : uEvent   Event code
//			   : lpParam1 Parameter 1
//			   : lpParam2 Parameter 2
//			   :
// Return	   : Request result or value depending on event type
//==============================================================================
UINT ClPluginEventHandler(UINT uEvent, LONG_PTR lpParam1, LONG_PTR lpParam2)
{
	switch (uEvent) {														// Select processing by event code
	  //------------------------------------------------------------------------
	  // Request to get plugin title
	  //------------------------------------------------------------------------
	  case PRQ_GETTITLE:
		ClDebugLog(_T("PRQ_GETTITLE (plugin enumerated by CLaunch)"));
		if (lpParam1) {														// Destination buffer specified?
			StringCchCopy((LPTSTR)lpParam1, (int)lpParam2, PLUGIN_TITLE);	// Copy the plugin name
		}
		return SUPPORT_VERSION; 											// Return the supported core version
	  //------------------------------------------------------------------------
	  // Plugin initialization request
	  //------------------------------------------------------------------------
	  case PRQ_INIT:
		return ClPluginInit((int)lpParam1, (LPVOID*)lpParam2);				// Plugin initialization
	  //------------------------------------------------------------------------
	  // Plugin termination request
	  //------------------------------------------------------------------------
	  case PRQ_EXIT:
		return ClPluginExit();												// Plugin termination
	  //------------------------------------------------------------------------
	  // Plugin ID change notification
	  //------------------------------------------------------------------------
	  case PRQ_IDCHANGE:
		g_iPluginId = (int)lpParam1;										// Save new plugin ID
		break;
	  //------------------------------------------------------------------------
	  // Request to show plugin settings dialog
	  //------------------------------------------------------------------------
	  case PRQ_SETTING:
		CpiDialogBox(g_hInst, IDD_SETTINGS, (HWND)lpParam1,
												(DLGPROC)ClPluginSettingDlg);	// Show plugin settings dialog
		break;
	  //------------------------------------------------------------------------
	  // Event notifications: track the window visibility and re-install the
	  // hook to keep it at the newest (first-called) position of the chain
	  //------------------------------------------------------------------------
	  case PEV_SHOW:
		g_bVisible = TRUE;
		ClDebugLog(_T("PEV_SHOW -> visible=1"));
		ClInstallKeyHook();
		break;
	  case PEV_HIDE:
		g_bVisible = FALSE;
		ClDebugLog(_T("PEV_HIDE -> visible=0"));
		ClInstallKeyHook();
		break;
	  case PEV_HOOK:
		ClInstallKeyHook();
		break;
	  //------------------------------------------------------------------------
	  // Unsupported requests / events
	  // (This plugin registers no button item and has no about dialog)
	  //------------------------------------------------------------------------
	  default:
		return FALSE;														// Failed / unsupported event
	}
	return TRUE;															// Succeeded
}

//==============================================================================
// Description : DLL notification entry point
//			   :
// Parameters  : hinstDLL	  Handle to the DLL module
//			   : dwReason	  Reason for calling this function
//			   : lpvReserved  Reserved
//			   :
// Return	   : TRUE : Success
//			   : FALSE: Failure
//==============================================================================
BOOL APIENTRY DllMain(HINSTANCE hInstDLL, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH) {
		g_hInst = hInstDLL; 												// Save DLL instance handle
	}
	return TRUE;
}
