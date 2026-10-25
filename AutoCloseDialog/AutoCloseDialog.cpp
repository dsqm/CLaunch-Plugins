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
#include <shlwapi.h>
#include <strsafe.h>
#pragma comment (lib, "shlwapi.lib")

//------------------------------------------------------------------------------
// Plugin title shown in the CLaunch plugin list
//------------------------------------------------------------------------------
#define PLUGIN_TITLE		_T("Auto-close Unfocused Dialogs")

//------------------------------------------------------------------------------
// Title keywords of the windows that must NOT be closed
// (the CLaunch options dialog and its sub-dialogs; matched case-insensitively)
//------------------------------------------------------------------------------
static const LPCTSTR s_pszKeepTitles[] = {
	_T("选项"),															// zh-CN
	_T("選項"),															// zh-TW
	_T("オプション"),													// ja
	_T("Options"),														// en
	_T("Opzioni"),														// it
	_T("Opciones"),														// es
	_T("옵션"),															// ko
	_T("設定"),															// ja sub-dialogs
};

//------------------------------------------------------------------------------
// Global state
//------------------------------------------------------------------------------
HWINEVENTHOOK	g_hWinEventHook	= NULL;									// Foreground-change event hook

//------------------------------------------------------------------------------
// Diagnostic log (written next to the DLL as AutoCloseDialog.log)
// * Off by default. Set ACD_DEBUG_LOG to 1 to enable it while debugging.
//------------------------------------------------------------------------------
#define ACD_DEBUG_LOG	1

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
	HANDLE hFile = CreateFile(szPath, FILE_APPEND_DATA,
									FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
									OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return;
	}
	TCHAR szLine[512];
	StringCchPrintf(szLine, ARRAYSIZE(szLine), _T("[%8u] %s\r\n"),
							GetTickCount(), pszText);
	DWORD dwWritten = 0;
	WriteFile(hFile, szLine, (DWORD)lstrlen(szLine) * sizeof(TCHAR), &dwWritten, NULL);
	CloseHandle(hFile);
}
#else
#define ClDebugLog(pszText)	((void)0)
#endif

//==============================================================================
// Description : Check whether the window must be kept open
//			   : (its title contains an options-dialog keyword)
//			   :
// Parameters  : hWnd  Window handle
//			   :
// Return	   : TRUE : Keep the window open
//			   : FALSE: Close it
//==============================================================================
static BOOL ClIsOptionsDialog(HWND hWnd)
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
	if (ClIsOptionsDialog(hWnd)) {
		return TRUE;														// Options dialog: keep it open
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
