//==============================================================================
//
//			CLaunch Plugin : XButton1 Toggle
//
//			Closes the CLaunch window with the mouse XButton1 (side button).
//
//			- CLaunch hidden  + XButton1 press : the event passes through and
//			  CLaunch's own hook shows the window (as configured by the user
//			  in the mouse button hook options)
//			- CLaunch visible + XButton1 press : the window is hidden and the
//			  event is swallowed so that CLaunch's "show" action does not
//			  fire again
//
//			The plugin installs its own low-level mouse hook (WH_MOUSE_LL)
//			and tracks the window visibility through PEV_SHOW / PEV_HIDE.
//
//==============================================================================
#include "X1Toggle.h"
#include "version.h"
#include <strsafe.h>

//------------------------------------------------------------------------------
// Plugin title shown in the CLaunch plugin list
//------------------------------------------------------------------------------
#define PLUGIN_TITLE		_T("XButton1 Toggle")

//------------------------------------------------------------------------------
// Plugin description shown in the about dialog (PRQ_ABOUT)
//------------------------------------------------------------------------------
#define PLUGIN_DESCRIPTION	_T("鼠标侧键 X1 关闭已显示的 CLaunch 窗口，与 CLaunch 自身的“X1 显示”动作配合成完整的显示/隐藏切换")

//------------------------------------------------------------------------------
// Global state
//------------------------------------------------------------------------------
HHOOK	g_hMouseHook	= NULL;													// Low-level mouse hook handle
BOOL	g_bVisible		= FALSE;												// CLaunch window visibility (synced with PEV_SHOW/PEV_HIDE)
BOOL	g_bSwallowX1Up	= FALSE;												// Swallow the XBUTTON1 up event following a swallowed down

//------------------------------------------------------------------------------
// Diagnostic log (written next to the DLL as X1Toggle.log)
// * Off by default. Set X1T_DEBUG_LOG to 1 to enable it while debugging.
//------------------------------------------------------------------------------
#define X1T_DEBUG_LOG	0

#if X1T_DEBUG_LOG
void ClDebugLog(LPCTSTR pszText)
{
	TCHAR szPath[MAX_PATH];
	if (!GetModuleFileName(g_hInst, szPath, ARRAYSIZE(szPath))) {
		return;
	}
	LPTSTR pszName = _tcsrchr(szPath, _T('\\'));								// Find the file name part
	if (!pszName) {
		return;
	}
	pszName[1] = _T('\0');														// Keep the directory part only
	StringCchCat(szPath, ARRAYSIZE(szPath), _T("X1Toggle.log"));
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
	StringCchPrintf(szLine, ARRAYSIZE(szLine), _T("[%8u] tid=%4u %s\r\n"),
							GetTickCount(), GetCurrentThreadId(), pszText);
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
// Description : Low-level mouse hook procedure
//			   : - Acts only on XBUTTON1 (the first side button)
//			   : - Must return quickly; do not add slow processing here
//			   :
// Parameters  : nCode	 Hook code
//			   : wParam  Mouse message
//			   : lParam  MSLLHOOKSTRUCT structure (WH_MOUSE_LL)
//			   :
// Return	   : 1 to swallow the event, otherwise pass to the next hook
//==============================================================================
LRESULT CALLBACK ClLowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
#if X1T_DEBUG_LOG
	static UINT uDbgCount = 0;
	if (uDbgCount < 6) {													// Log the first events of any kind to check hook liveness
		TCHAR szMsg[96];
		StringCchPrintf(szMsg, ARRAYSIZE(szMsg), _T("HOOK event: msg=0x%04X"), (UINT)wParam);
		ClDebugLog(szMsg);
		uDbgCount++;
	}
#endif
	if (nCode == HC_ACTION) {
		PMSLLHOOKSTRUCT pMse = (PMSLLHOOKSTRUCT)lParam; 						// LL hook uses MSLLHOOKSTRUCT (NOT MOUSEHOOKSTRUCTEX)
		if (HIWORD(pMse->mouseData) == XBUTTON1) {								// XButton1 ?
			if (wParam == WM_XBUTTONDOWN) {
				if (g_bVisible) {												// CLaunch is shown
					ClDebugLog(_T("X1 DOWN while visible -> hide + swallow"));
					CpiHideWindow();											// Hide the CLaunch window
					g_bSwallowX1Up = TRUE;
					return 1;													// Swallow the event so the "show" action won't re-fire
				}
				ClDebugLog(_T("X1 DOWN while hidden -> pass through"));
			}
			else if (wParam == WM_XBUTTONUP && g_bSwallowX1Up) {				// Up event of a swallowed down
				g_bSwallowX1Up = FALSE;
				ClDebugLog(_T("X1 UP (of swallowed down) -> swallow"));
				return 1;														// Swallow it as well
			}
		}
	}
	return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam); 				// Pass the event through
}

//==============================================================================
// Description : Install the low-level mouse hook (release the previous one
//			   : if it is still installed)
//			   : - Low-level hooks are called from the newest to the oldest.
//			   :   ClHook.dll's own hook consumes the XButton1 events, so this
//			   :   plugin re-installs its hook on PEV_SHOW / PEV_HIDE / PEV_HOOK
//			   :   to take the newest (first-called) position of the chain.
//			   :
// Return	   : None
//==============================================================================
void ClInstallMouseHook(void)
{
	if (g_hMouseHook) {
		UnhookWindowsHookEx(g_hMouseHook);
		g_hMouseHook = NULL;
	}
	g_hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, ClLowLevelMouseProc, g_hInst, 0);
#if X1T_DEBUG_LOG
	{
		TCHAR szMsg[128];
		StringCchPrintf(szMsg, ARRAYSIZE(szMsg), _T("HOOK install: %s (err=%lu)"),
								g_hMouseHook ? _T("ok") : _T("FAILED"),
								g_hMouseHook ? 0 : GetLastError());
		ClDebugLog(szMsg);
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
	g_pCpiFunc = pvFunc;														// Save pointer to CPI function address table
	if (CpiGetVersion() < SUPPORT_VERSION) {									// Is the core version too old?
		ClDebugLog(_T("INIT: CLaunch core version too old -> FALSE"));
		return FALSE;															// Abort initialization
	}
	//--------------------------------------------------------------------------
	// If this is only a temporary registration so that the plugin appears
	// in the plugin list of the CLaunch options, finish here.
	//--------------------------------------------------------------------------
	if (id < 0) {																// Temporary registration?
		ClDebugLog(_T("INIT: temporary registration (id < 0)"));
		return TRUE;															// Return success
	}
	//--------------------------------------------------------------------------
	// From here on is the processing when the plugin is actually registered
	// (not a temporary registration).
	//--------------------------------------------------------------------------
	g_iPluginId = id;															// Save plugin registration ID
	//--------------------------------------------------------------------------
	// Sync the initial visibility state
	// (The window can be open while the plugin is enabled in the options)
	//--------------------------------------------------------------------------
	HWND hwCLaunch = CpiGetHandle();
	g_bVisible		= (hwCLaunch && IsWindowVisible(hwCLaunch)) ? TRUE : FALSE;
	g_bSwallowX1Up	= FALSE;
	//--------------------------------------------------------------------------
	// Install the low-level mouse hook
	// - Installed on CLaunch's main thread, which pumps messages
	//--------------------------------------------------------------------------
	ClInstallMouseHook();
#if X1T_DEBUG_LOG
	{
		TCHAR szMsg[160];
		StringCchPrintf(szMsg, ARRAYSIZE(szMsg),
								_T("INIT: real registration id=%d, visible=%d"),
								id, (int)g_bVisible);
		ClDebugLog(szMsg);
	}
#endif
	return (g_hMouseHook != NULL);												// Hook installation result
}

//==============================================================================
// Description : Plugin termination
//			   :
// Return	   : TRUE : Succeeded
//			   : FALSE: Failed
//==============================================================================
BOOL ClPluginExit(void)
{
	//--------------------------------------------------------------------------
	// Release the mouse hook
	//--------------------------------------------------------------------------
	if (g_hMouseHook) {
		UnhookWindowsHookEx(g_hMouseHook);										// Must release before the DLL is unloaded
		g_hMouseHook = NULL;
	}
	ClDebugLog(_T("EXIT: hook released"));
	g_bVisible		= FALSE;
	g_bSwallowX1Up	= FALSE;
	g_iPluginId		= -1;														// Reset plugin ID
	return TRUE;																// Succeeded
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
	switch (uEvent) {															// Select processing by event code
	 //=========================================================================
	 // Requests from CLaunch
	 //=========================================================================
	  // Request to get plugin title
	  //------------------------------------------------------------------------
	  case PRQ_GETTITLE:
		ClDebugLog(_T("PRQ_GETTITLE (plugin enumerated by CLaunch)"));
		if (lpParam1) {															// Destination buffer specified?
			StringCchCopy((LPTSTR)lpParam1, (int)lpParam2, PLUGIN_TITLE);		// Copy the plugin name
		}
		return SUPPORT_VERSION; 												// Return the supported core version
	  //------------------------------------------------------------------------
	  // Plugin initialization request
	  //------------------------------------------------------------------------
	  case PRQ_INIT:
		return ClPluginInit((int)lpParam1, (LPVOID*)lpParam2);					// Plugin initialization
	  //------------------------------------------------------------------------
	  // Plugin termination request
	  //------------------------------------------------------------------------
	  case PRQ_EXIT:
		return ClPluginExit();													// Plugin termination
	  //------------------------------------------------------------------------
	  // Plugin ID change notification
	  //------------------------------------------------------------------------
	  case PRQ_IDCHANGE:
		g_iPluginId = (int)lpParam1;											// Save new plugin ID
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
	 //=========================================================================
	 // Event notifications from CLaunch
	 //=========================================================================
	  // CLaunch shown
	  //------------------------------------------------------------------------
	  case PEV_SHOW:
		g_bVisible = TRUE;														// Update visibility state
		ClDebugLog(_T("PEV_SHOW -> visible = 1"));
		ClInstallMouseHook();													// Take the newest position of the hook chain
		break;
	  //------------------------------------------------------------------------
	  // CLaunch hidden
	  //------------------------------------------------------------------------
	  case PEV_HIDE:
		g_bVisible = FALSE; 													// Update visibility state
		ClDebugLog(_T("PEV_HIDE -> visible = 0"));
		ClInstallMouseHook();													// Take the newest position of the hook chain
		break;
	  //------------------------------------------------------------------------
	  // Hook state changed
	  //------------------------------------------------------------------------
	  case PEV_HOOK:
#if X1T_DEBUG_LOG
		{
			TCHAR szMsg[96];
			StringCchPrintf(szMsg, ARRAYSIZE(szMsg),
									_T("PEV_HOOK status=0x%08X -> re-hook"), (UINT)lpParam1);
			ClDebugLog(szMsg);
		}
#endif
		ClInstallMouseHook();													// Take the newest position of the hook chain
		break;
	  //------------------------------------------------------------------------
	  // Unsupported requests / events
	  // (This plugin registers no button item and has no settings UI)
	  //------------------------------------------------------------------------
	  default:
		return FALSE;															// Failed / unsupported event
	}
	return TRUE;																// Succeeded
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
		g_hInst = hInstDLL; 													// Save DLL instance handle
	}
	return TRUE;
}
