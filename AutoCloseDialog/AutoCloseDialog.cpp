//==============================================================================
//
//			CLaunch Plugin : Auto-close Unfocused Dialogs
//
//			Closes leftover CLaunch dialog windows (item properties, message
//			boxes, ...) when the input focus moves to another application.
//
//			An unfocused modal dialog stays hidden behind other windows and
//			blocks summoning CLaunch (the "show" action cannot bring up the
//			main window while a dialog is open), so the dialog is closed as
//			soon as it loses the focus.
//
//			Windows whose title contains an options-dialog keyword (the
//			CLaunch options dialog and its sub-dialogs) are never closed,
//			so unsaved option edits are not discarded.
//
//==============================================================================
#include "AutoCloseDialog.h"
#include "version.h"
#include <shlwapi.h>
#include <strsafe.h>
#pragma comment (lib, "shlwapi.lib")

//------------------------------------------------------------------------------
// Plugin title shown in the CLaunch plugin list
//------------------------------------------------------------------------------
#define PLUGIN_TITLE		_T("Auto-close Unfocused Dialogs")

//------------------------------------------------------------------------------
// Plugin description shown in the about dialog (PRQ_ABOUT)
//------------------------------------------------------------------------------
#define PLUGIN_DESCRIPTION	_T("对话框（项目属性等）失去焦点时自动关闭，避免残留后台卡住 CLaunch 无法召唤")

//------------------------------------------------------------------------------
// Title keywords of the windows that must NOT be closed, nor the dialogs they
// own (the CLaunch options dialog, the icon settings dialog, ...; matched
// case-insensitively)
//------------------------------------------------------------------------------
static const LPCTSTR s_pszKeepTitles[] = {
	_T("选项"),															// zh-CN options
	_T("選項"),															// zh-TW options
	_T("オプション"),													// ja options
	_T("Options"),														// en options
	_T("Opzioni"),														// it options
	_T("Opciones"),														// es options
	_T("옵션"),															// ko options
	_T("設定"),															// ja options & sub-dialogs (incl. icon settings)
	_T("图标设置"),														// zh-CN icon settings
	_T("圖示"),															// zh-TW icon settings
	_T("Icon"),															// en icon settings
};

//------------------------------------------------------------------------------
// Global state
//------------------------------------------------------------------------------
HWINEVENTHOOK	g_hWinEventHook	= NULL;									// Foreground-change event hook

//------------------------------------------------------------------------------
// Diagnostic log (written next to the DLL as AutoCloseDialog.log)
// * Off by default. Set ACD_DEBUG_LOG to 1 to enable it while debugging.
//------------------------------------------------------------------------------
#define ACD_DEBUG_LOG	0

#if ACD_DEBUG_LOG
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
	StringCchCat(szPath, ARRAYSIZE(szPath), _T("AutoCloseDialog.log"));
	//--------------------------------------------------------------------------
	// Write in UTF-8 (with BOM) so the log stays readable in any text editor
	//--------------------------------------------------------------------------
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
// Description : Check whether one window's title matches a keep keyword
//			   :
// Parameters  : hWnd  Window handle
//			   :
// Return	   : TRUE : Matched
//			   : FALSE: Not matched
//==============================================================================
static BOOL ClTitleMatchesKeep(HWND hWnd)
{
	TCHAR szTitle[160];
	if (GetWindowText(hWnd, szTitle, ARRAYSIZE(szTitle)) <= 0) {
		return FALSE;
	}
	for (int i = 0; i < ARRAYSIZE(s_pszKeepTitles); i++) {
		if (StrStrI(szTitle, s_pszKeepTitles[i])) {
			return TRUE;
		}
	}
	return FALSE;
}

//==============================================================================
// Description : Check whether the window must be kept open
//			   : - TRUE when the window itself or one of its owner windows
//			   :   (up to the main window) matches a keep keyword; dialogs
//			   :   spawned by a kept window (e.g. the file picker opened from
//			   :   the icon settings dialog) are kept open as well
//			   :
// Parameters  : hWnd  Window handle
//			   :
// Return	   : TRUE : Keep the window open
//			   : FALSE: Close it
//==============================================================================
static BOOL ClKeepDialog(HWND hWnd)
{
	HWND h = hWnd;
	HWND hwMain = CpiGetHandle();
	while (h && h != hwMain) {
		if (ClTitleMatchesKeep(h)) {
			return TRUE;
		}
		h = GetWindow(h, GW_OWNER);											// Walk up the owner chain
	}
	return FALSE;
}

//==============================================================================
// Description : Close one leftover dialog window
//			   : (called back by EnumWindows)
//			   :
// Parameters  : hWnd	 Top-level window handle
//			   : lParam  CLaunch process ID
//			   :
// Return	   : TRUE to continue the enumeration
//==============================================================================
static BOOL CALLBACK ClCloseDialogEnumProc(HWND hWnd, LPARAM lParam)
{
	DWORD dwPid = 0;
	GetWindowThreadProcessId(hWnd, &dwPid);
	if (dwPid != (DWORD)lParam || !IsWindowVisible(hWnd)) {
		return TRUE;														// Not CLaunch's / not visible
	}
	if (hWnd == CpiGetHandle()) {
		return TRUE;														// Main window itself
	}
	TCHAR szClass[64];
	if (!GetClassName(hWnd, szClass, ARRAYSIZE(szClass)) ||
			lstrcmp(szClass, _T("#32770")) != 0) {
		return TRUE;														// Not a dialog box
	}
	if (ClKeepDialog(hWnd)) {
#if ACD_DEBUG_LOG
		{
			TCHAR szTitle[160], szMsg[320];
			GetWindowText(hWnd, szTitle, ARRAYSIZE(szTitle));
			StringCchPrintf(szMsg, ARRAYSIZE(szMsg),
									_T("keep dialog: \"%s\" (class %s)"), szTitle, szClass);
			ClDebugLog(szMsg);
		}
#endif
		return TRUE;														// Options / icon settings: keep it open
	}
#if ACD_DEBUG_LOG
	{
		TCHAR szTitle[160], szMsg[320];
		GetWindowText(hWnd, szTitle, ARRAYSIZE(szTitle));
		StringCchPrintf(szMsg, ARRAYSIZE(szMsg),
								_T("close dialog: \"%s\" (class %s)"), szTitle, szClass);
		ClDebugLog(szMsg);
	}
#endif
	PostMessage(hWnd, WM_CLOSE, 0, 0);										// Close as if cancelled
	return TRUE;
}

//==============================================================================
// Description : WinEvent callback: the foreground window has changed
//			   :
// Parameters  : (standard WinEventProc parameters)
//			   :
// Return	   : None
//==============================================================================
static void CALLBACK ClWinEventProc(HWINEVENTHOOK hWinEventHook, DWORD dwEvent,
									HWND hWnd, LONG idObject, LONG idChild,
									DWORD dwEventThread, DWORD dwmsEventTime)
{
	if (dwEvent != EVENT_SYSTEM_FOREGROUND || idObject != OBJID_WINDOW || !hWnd) {
		return;
	}
	//--------------------------------------------------------------------------
	// Ignore focus changes that stay inside the CLaunch process
	//--------------------------------------------------------------------------
	DWORD dwCLaunchPid = 0, dwFgPid = 0;
	HWND hwMain = CpiGetHandle();
	if (!hwMain) {
		return;
	}
	GetWindowThreadProcessId(hwMain, &dwCLaunchPid);
	GetWindowThreadProcessId(hWnd, &dwFgPid);
	if (dwFgPid == dwCLaunchPid) {
		return;																// Still focused on CLaunch
	}
	//--------------------------------------------------------------------------
	// Focus moved to another application: close the leftover dialogs
	//--------------------------------------------------------------------------
	EnumWindows(ClCloseDialogEnumProc, (LPARAM)dwCLaunchPid);
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
	// Real registration: watch the foreground window changes
	//--------------------------------------------------------------------------
	g_iPluginId = id;
	if (g_hWinEventHook) {
		UnhookWinEvent(g_hWinEventHook);
		g_hWinEventHook = NULL;
	}
	g_hWinEventHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND,
											EVENT_SYSTEM_FOREGROUND,
											NULL, ClWinEventProc, 0, 0,
											WINEVENT_OUTOFCONTEXT);
#if ACD_DEBUG_LOG
	{
		TCHAR szMsg[128];
		StringCchPrintf(szMsg, ARRAYSIZE(szMsg),
								_T("INIT: real registration id=%d, winEventHook=%s (err=%lu)"),
								id,
								g_hWinEventHook ? _T("installed") : _T("FAILED"),
								g_hWinEventHook ? 0 : GetLastError());
		ClDebugLog(szMsg);
	}
#endif
	return (g_hWinEventHook != NULL);										// Hook installation result
}

//==============================================================================
// Description : Plugin termination
//			   :
// Return	   : TRUE : Succeeded
//			   : FALSE: Failed
//==============================================================================
BOOL ClPluginExit(void)
{
	if (g_hWinEventHook) {
		UnhookWinEvent(g_hWinEventHook);									// Must release before the DLL is unloaded
		g_hWinEventHook = NULL;
	}
	g_iPluginId = -1;														// Reset plugin ID
	return TRUE;															// Succeeded
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
	  // Request to show the plugin about dialog (version + description)
	  //------------------------------------------------------------------------
	  case PRQ_ABOUT:
		{
			TCHAR szMsg[512];
			StringCchPrintf(szMsg, ARRAYSIZE(szMsg),
									_T("版本 %s\n\n%s"),
									PLUGIN_VERSION_T, PLUGIN_DESCRIPTION);
			MessageBox((HWND)lpParam1, szMsg, PLUGIN_TITLE,
									MB_OK | MB_ICONINFORMATION);
		}
		break;
	  //------------------------------------------------------------------------
	  // Unsupported requests / events
	  // (This plugin registers no button item and has no settings UI)
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
