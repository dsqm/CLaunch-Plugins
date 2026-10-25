//==============================================================================
//
//			CLaunch Plugin Header File
//
//			Copyright (C) 2005-2026 Pyonkichi. All rights reserved.
//
//==============================================================================
#include <windows.h>
#include <tchar.h>

//------------------------------------------------------------------------------
// Supported CLaunch core version
// * Set this to the CLaunch core version your plugin supports.
//------------------------------------------------------------------------------
#define SUPPORT_VERSION 	0x04020300
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Limits
//------------------------------------------------------------------------------
#define MAX_PLUGINNAME		256 												// Maximum length of plugin name

//------------------------------------------------------------------------------
// Generic macros
//------------------------------------------------------------------------------
#if !defined(ARRAYSIZE)
 #define ARRAYSIZE(A)		(sizeof(A) / sizeof((A)[0]))						// Get number of elements in an array
#endif

//------------------------------------------------------------------------------
// Request codes from CLaunch to the plugin
//------------------------------------------------------------------------------
#define PRQ_GETTITLE		0x00000001											// Request to get plugin title
#define PRQ_INIT			0x00000002											// Plugin initialization request
#define PRQ_EXIT			0x00000003											// Plugin termination request
#define PRQ_IDCHANGE		0x00000004											// Notification that plugin ID has changed
#define PRQ_INITITEM		0x00000005											// Request to initialize plugin item data
#define PRQ_PREEXECUTE		0x00000006											// Pre-execute request for plugin item
#define PRQ_EXECUTE 		0x00000007											// Plugin item execution request
#define PRQ_ABOUT			0x00000008											// Request to show plugin version information dialog
#define PRQ_SETTING 		0x00000009											// Request to show plugin settings dialog
#define PRQ_ITEMPROP		0x0000000a											// Request to show plugin item property dialog

//------------------------------------------------------------------------------
// Event notification codes from CLaunch
//------------------------------------------------------------------------------
#define PEV_SHOW			0x00010001											// CLaunch shown
#define PEV_HIDE			0x00010002											// CLaunch hidden
#define PEV_EXECUTE 		0x00010003											// Item executed
#define PEV_PAGECHANGED 	0x00010004											// Page changed
#define PEV_PAGEADDED		0x00010005											// Page added
#define PEV_PAGEDELETED 	0x00010006											// Page deleted
#define PEV_LANGCHANGED 	0x00010007											// Language changed
#define PEV_VIEWMODE		0x00010008											// View mode changed
#define PEV_PINNED			0x00010009											// Window pin state changed
#define PEV_RESTRICTED		0x0001000a											// Restricted mode changed
#define PEV_HOOK			0x0001000b											// Hook state changed


//------------------------------------------------------------------------------
// Additional option flags when a plugin item is executed
// (Values passed in uOption parameter of ClPluginPreExecute() and
//	ClPluginExecute().)
//------------------------------------------------------------------------------
#define EXE_ALTERMODE		0x00000001											// Alternate execution mode (button long-press)

//------------------------------------------------------------------------------
// Window display operation code
// (Values passed in uSwOpe parameter of CpiShowWindow().)
//------------------------------------------------------------------------------
#define	SCW_NOMOVE			0x00000000											// Current position
#define	SCW_AROUNDCURSOR	0x00000001											// Around the mouse cursor
#define	SCW_LOWERRIGHTCURS	0x00000002											// Lower-right of the mouse cursor
#define	SCW_DISPLAYCENTER	0x00000003											// Center of the display
#define	SCW_SPECIFYXY		0x00000004											// At specified coordinates
#define	SCW_ADJUSTONLY		0x00000005											// Adjust position only

#define	SCW_AUTOHIDE		0x00000100											// Auto-hide flag
#define	SCW_FORCESHOW		0x00000200											// Force-show flag
#define	SCW_ADJUST			0x00000400											// Adjust position flag
#define	SCW_NOACTIVATE		0x00000800											// Non-activating flag

#define GETWINPOS(pos)		(pos & 0x000000ff)									// Extract display position index

//------------------------------------------------------------------------------
// Initial display page
// (Values passed in iPage parameter of CpiShowWindow().)
//------------------------------------------------------------------------------
#define PN_MEMORY			0xffff												// Display the previously used page

//------------------------------------------------------------------------------
// Status flags
// (Values passed in uBit parameter of CpiGetStatus().)
//------------------------------------------------------------------------------
#define CS_VIEWMODE 		0x00000001											// View mode (0: view mode 1 / 1: view mode 2)
#define CS_PINNED			0x00000002											// Window pinned state
#define CS_RESTRICT 		0x00000004											// Restricted mode state
#define CS_SEARCHWND		0x00000008											// Search window visible state
#define CS_HOOK 			0x00010000											// Hooking state

//------------------------------------------------------------------------------
// Modifier key bit flags
//	Key code (WORD value) format used by CpiCreateHotkeyEdit(),
//	CpiSetKeyToHotkeyEdit(), CpiGetKeyFromHotkeyEdit(): 0x0MVK
//	 M : Modifier key bit flags	(4-bit)
//	 VK: Virtual key code		(8-bit)
//------------------------------------------------------------------------------
#define MKBIT_SHIFT			0x0100												// Shift
#define MKBIT_CTRL			0x0200												// Ctrl
#define MKBIT_ALT			0x0400												// Alt
#define MKBIT_WIN			0x0800												// Win

#define	GetVK(code)				((WORD)(code & 0x00ff))							// Extract virtual key code
#define	SetModKeys(code, mods)	((WORD)(GetVK(code) | (mods << 8)))				// Set modifier key flags

//------------------------------------------------------------------------------
// Window control commands
// (Values passed in uCmd parameter of CpiWindowOperations().)
//------------------------------------------------------------------------------
#define WOPE_ACTIVATE		0													// Activate
#define WOPE_CLOSE			1													// Close
#define WOPE_TERMINATE		2													// Close (force terminate)
#define WOPE_MAXIMIZE		3													// Maximize
#define WOPE_MINIMIZE		4													// Minimize
#define WOPE_RESTORE		5													// Restore to original size
#define WOPE_HIDE			6													// Hide
#define WOPE_SHOW			7													// Show
#define WOPE_BOTTOM 		8													// Send to bottom
#define WOPE_TOPMOST		9													// Make topmost

//------------------------------------------------------------------------------
// Global variables
//------------------------------------------------------------------------------
int 		g_iPluginId = -1;													// Plugin ID
HINSTANCE	g_hInst 	= NULL; 												// Instance handle of plugin DLL
LPVOID		*g_pCpiFunc = NULL; 												// Pointer to CPI (CLaunch Plugin Interface) function address table


//_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
//_/																		  _/
//_/																		  _/
//_/							CPI Functions								  _/
//_/																		  _/
//_/																		  _/
//_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/

//------------------------------------------------------------------------------
// Function table index
//------------------------------------------------------------------------------
// ---- 1. Core / Information (0x00 - 0x0F) ------------------------------------
#define IDX_GET_VERSION 		0x00											// Get CLaunch version
#define IDX_GET_HANDLE			0x01											// Get CLaunch window handle and instance handle
#define IDX_GET_STATUS			0x02											// Get CLaunch status
#define IDX_GET_PAGENUM 		0x03											// Get number of pages
#define IDX_GET_CURRENTPAGE 	0x04											// Get currently displayed page index
#define IDX_GET_LANGUAGE		0x05											// Get current language
#define IDX_GET_DATAFOLDER		0x06											// Get path of data folder
// 0x07 - 0x0F : reserved for future core/info APIs

// ---- 2. CLaunch control (0x10 - 0x1F) ---------------------------------------
#define IDX_SHOW_WINDOW 		0x10											// Show CLaunch
#define IDX_HIDE_WINDOW 		0x11											// Hide CLaunch
#define IDX_CHANGE_VIEWMODE 	0x12											// Change view mode
#define IDX_SET_PIN 			0x13											// Set/release window pin
#define IDX_SET_RESTRICTEDMODE	0x14											// Enter/leave restricted mode
#define IDX_UNHOOK				0x15											// Release/restart hook
#define IDX_SET_CAPTION 		0x16											// Change caption
// 0x17 - 0x1F : reserved

// ---- 3. Skin (0x20 - 0x2F) --------------------------------------------------
#define IDX_GET_SKINFOLDER		0x20											// Get path of skin folder
#define IDX_GET_CURRENTSKIN 	0x21											// Get file name of current skin
#define IDX_GET_SKINTITLE		0x22											// Get title of skin specified by file name
#define IDX_CHANGE_SKIN 		0x23											// Change to skin specified by file name
// 0x24 - 0x2F : reserved

// ---- 4. Resource / Dialog (0x30 - 0x3F) -------------------------------------
#define IDX_LOAD_STRING 		0x30											// Load string resource for current language
#define IDX_DIALOG_BOX			0x31											// Show dialog box using dialog resource for current language
#define IDX_CREATE_HKEDIT		0x32											// Create a hotkey edit control
#define IDX_SET_KEYTOHKEDIT 	0x33											// Set a key code to the hotkey edit control
#define IDX_GET_KEYFROMHKEDIT	0x34											// Get the key code entered in the hotkey edit control
// 0x35 - 0x3F : reserved

// ---- 5. Plugin config (0x40 - 0x4F) -----------------------------------------
#define IDX_REG_BUTTONITEM		0x40											// Register plugin as a button item
#define IDX_SET_INTDATA 		0x41											// Set integer data for plugin
#define IDX_SET_STRDATA 		0x42											// Set string data for plugin
#define IDX_GET_INTDATA 		0x43											// Get integer data set for plugin
#define IDX_GET_STRDATA 		0x44											// Get string data set for plugin
#define IDX_SAVE_SETTINGS		0x45											// Save plugin settings to settings file
#define IDX_SET_ITEMINT 		0x46											// Set integer data for plugin item
#define IDX_SET_ITEMSTR 		0x47											// Set string data for plugin item
#define IDX_GET_ITEMINT 		0x48											// Get integer data set for plugin item
#define IDX_GET_ITEMSTR 		0x49											// Get string data set for plugin item
#define IDX_DELETE_DATA			0x4A											// Delete data set for plugin
#define IDX_DELETE_ITEMINT		0x4B											// Delete integer data for plugin items from the specified index onward
#define IDX_DELETE_ITEMSTR		0x4C											// Delete string data for plugin items from the specified index onward
// 0x4D - 0x4F : reserved

// ---- 6. Others (0x50 - 0x5F) ------------------------------------------------
#define IDX_WINDOW_CONTROL		0x50											// Perform specified operation on the specified window
// 0x51 - 0x5F : reserved

//------------------------------------------------------------------------------
// Function type declarations
//------------------------------------------------------------------------------
// ---- 1. Core / Information --------------------------------------------------
typedef DWORD (*LPGET_VERSION)(void);											// IDX_GET_VERSION
typedef HWND (*LPGET_HANDLE)(HINSTANCE*);										// IDX_GET_HANDLE
typedef UINT (*LPGET_STATUS)(void);												// IDX_GET_STATUS
typedef int (*LPGET_PAGENUM)(void);												// IDX_GET_PAGENUM
typedef int (*LPGET_CURRENTPAGE)(void);											// IDX_GET_CURRENTPAGE
typedef LPCTSTR (*LPGET_LANGUAGE)(LPCTSTR*);									// IDX_GET_LANGUAGE
typedef LPCTSTR (*LPGET_DATAFOLDER)(void);										// IDX_GET_DATAFOLDER

// ---- 2. CLaunch control -----------------------------------------------------
typedef void (*LPSHOW_WINDOW)(UINT, int, int, int);								// IDX_SHOW_WINDOW
typedef void (*LPHIDE_WINDOW)(void);											// IDX_HIDE_WINDOW
typedef void (*LPCHANGE_VIEWMODE)(int);											// IDX_CHANGE_VIEWMODE
typedef void (*LPSET_PIN)(BOOL);												// IDX_SET_PIN
typedef void (*LPSET_RESTRICTEDMODE)(BOOL);										// IDX_SET_RESTRICTEDMODE
typedef void (*LPUNHOOK)(BOOL);													// IDX_UNHOOK
typedef BOOL (*LPSET_CAPTION)(LPCTSTR, BOOL);									// IDX_SET_CAPTION

// ---- 3. Skin ----------------------------------------------------------------
typedef LPCTSTR (*LPGET_SKINFOLDER)(void);										// IDX_GET_SKINFOLDER
typedef LPCTSTR (*LPGET_CURRENTSKIN)(void);										// IDX_GET_CURRENTSKIN
typedef LPCTSTR (*LPGET_SKINTITLE)(LPCTSTR);									// IDX_GET_SKINTITLE
typedef BOOL (*LPCHANGE_SKIN)(LPCTSTR);											// IDX_CHANGE_SKIN

// ---- 4. Resource / Dialog ---------------------------------------------------
typedef int (*LPLOAD_STRING)(HINSTANCE, UINT, LPTSTR, int);						// IDX_LOAD_STRING
typedef INT_PTR (*LPDIALOG_BOX)(HINSTANCE, UINT, HWND, DLGPROC, LPARAM);		// IDX_DIALOG_BOX
typedef BOOL (*LPCREATE_HKEDIT)(HWND, int, WORD);								// IDX_CREATE_HKEDIT
typedef void (*LPSET_KEYTOHKEDIT)(HWND, int, WORD);								// IDX_SET_KEYTOHKEDIT
typedef WORD (*LPGET_KEYFROMHKEDITE)(HWND, int);								// IDX_GET_KEYFROMHKEDIT

// ---- 5. Plugin config -------------------------------------------------------
typedef void (*LPREG_BUTTONITEM)(int, LPCTSTR);									// IDX_REG_BUTTONITEM
typedef void (*LPSET_INTDATA)(int, LPCTSTR, LPCTSTR, int);						// IDX_SET_INTDATA
typedef LPCTSTR (*LPSET_STRDATA)(int, LPCTSTR, LPCTSTR, LPCTSTR);				// IDX_SET_STRDATA
typedef int (*LPGET_INTDATA)(int, LPCTSTR, LPCTSTR, int);						// IDX_GET_INTDATA
typedef LPCTSTR (*LPGET_STRDATA)(int, LPCTSTR, LPCTSTR);						// IDX_GET_STRDATA
typedef void (*LPSAVE_SETTINGS)(int);											// IDX_SAVE_SETTINGS
typedef void (*LPSET_ITEMINT)(UINT, int, int);									// IDX_SET_ITEMINT
typedef LPCTSTR (*LPSET_ITEMSTR)(UINT, int, LPCTSTR);							// IDX_SET_ITEMSTR
typedef int (*LPGET_ITEMINT)(UINT, int, int);									// IDX_GET_ITEMINT
typedef LPCTSTR (*LPGET_ITEMSTR)(UINT, int);									// IDX_GET_ITEMSTR
typedef void (*LPDELETE_DATA)(int, LPCTSTR, LPCTSTR);							// IDX_DELETE_DATA
typedef void (*LPDELETE_ITEMINT)(UINT, int);									// IDX_DELETE_ITEMINT
typedef void (*LPDELETE_ITEMSTR)(UINT, int);									// IDX_DELETE_ITEMSTR

// ---- 6. Others --------------------------------------------------------------
typedef BOOL (*LPWINDOW_CONTROL)(HWND, UINT);									// IDX_WINDOW_CONTROL

//------------------------------------------------------------------------------
// Wrapper functions
//------------------------------------------------------------------------------
// ---- 1. Core / Information --------------------------------------------------
//==============================================================================
// Description : [CPI function]
//			   : Get CLaunch version
//			   :
// Parameters  : None
//			   :
// Return	   : DWORD version value
//==============================================================================
inline DWORD CpiGetVersion(void) {
	return ((LPGET_VERSION)g_pCpiFunc[IDX_GET_VERSION])();
}
//==============================================================================
// Description : [CPI function]
//			   : Get CLaunch window handle and instance handle
//			   :
// Parameters  : phInst  Pointer that receives instance handle (optional)
//			   :
// Return	   : CLaunch window handle
//==============================================================================
inline HWND CpiGetHandle(HINSTANCE *phInst=NULL) {
	return ((LPGET_HANDLE)g_pCpiFunc[IDX_GET_HANDLE])(phInst);
}
//==============================================================================
// Description : [CPI function]
//			   : Get CLaunch status
//			   :
// Parameters  : None
//			   :
// Return	   : CLaunch status flags
//==============================================================================
inline UINT CpiGetStatus(void) {
	return ((LPGET_STATUS)g_pCpiFunc[IDX_GET_STATUS])();
}
//==============================================================================
// Description : [CPI function]
//			   : Get number of pages
//			   :
// Parameters  : None
//			   :
// Return	   : Number of pages
//==============================================================================
inline int CpiGetPageNum(void) {
	return ((LPGET_PAGENUM)g_pCpiFunc[IDX_GET_PAGENUM])();
}
//==============================================================================
// Description : [CPI function]
//			   : Get current page index
//			   :
// Parameters  : None
//			   :
// Return	   : Index of currently displayed page
//==============================================================================
inline int CpiGetCurrentPage(void) {
	return ((LPGET_CURRENTPAGE)g_pCpiFunc[IDX_GET_CURRENTPAGE])();
}
//==============================================================================
// Description : [CPI function]
//			   : Get current language
//			   :
// Parameters  : pszName  Pointer that receives language display name
//			   :		  (optional)
//			   :
// Return	   : File name of language module (not including path)
//==============================================================================
inline LPCTSTR CpiGetLanguage(LPCTSTR *pszName=NULL) {
	return ((LPGET_LANGUAGE)g_pCpiFunc[IDX_GET_LANGUAGE])(pszName);
}
//==============================================================================
// Description : [CPI function]
//			   : Get path of data folder
//			   :
// Parameters  : None
//			   :
// Return	   : Path of data folder
//==============================================================================
inline LPCTSTR CpiGetDataFolder(void) {
	return ((LPGET_DATAFOLDER)g_pCpiFunc[IDX_GET_DATAFOLDER])();
}

// ---- 2. CLaunch control -----------------------------------------------------
//==============================================================================
// Description : [CPI function]
//			   : Show CLaunch
//			   :
// Parameters  : uSwOpe	 Display operation code
//			   :		 (default: SCW_AROUNDCURSOR | SCW_AUTOHIDE)
//			   : x		 X coordinate of display position (default: 0)
//			   : y		 Y coordinate of display position (default: 0)
//			   : iPage	 Initial display page (default: PN_MEMORY)
//			   :
// Return	   : None
//==============================================================================
inline void CpiShowWindow(UINT uSwOpe=SCW_AROUNDCURSOR|SCW_AUTOHIDE, int x=0, int y=0, int iPage=PN_MEMORY) {
	((LPSHOW_WINDOW)g_pCpiFunc[IDX_SHOW_WINDOW])(uSwOpe, x, y, iPage);
}
//==============================================================================
// Description : [CPI function]
//			   : Hide CLaunch
//			   :
// Parameters  : None
//			   :
// Return	   : None
//==============================================================================
inline void CpiHideWindow(void) {
	((LPHIDE_WINDOW)g_pCpiFunc[IDX_HIDE_WINDOW])();
}
//==============================================================================
// Description : [CPI function]
//			   : Change view mode
//			   :
// Parameters  : iViewMode	View mode (0:view mode 1 / 1:view mode 2)
//			   :
// Return	   : None
//==============================================================================
inline void CpiChangeViewMode(int iViewMode) {
	((LPCHANGE_VIEWMODE)g_pCpiFunc[IDX_CHANGE_VIEWMODE])(iViewMode);
}
//==============================================================================
// Description : [CPI function]
//			   : Set/release window pin
//			   :
// Parameters  : bSet  TRUE : Pin window
//			   :	   FALSE: Unpin window
//			   :
// Return	   : None
//==============================================================================
inline void CpiSetPin(BOOL bSet) {
	((LPSET_PIN)g_pCpiFunc[IDX_SET_PIN])(bSet);
}
//==============================================================================
// Description : [CPI function]
//			   : Enter/leave restricted mode
//			   :
// Parameters  : bSet  TRUE : Enter restricted mode
//			   :	   FALSE: Leave restricted mode
//			   :
// Return	   : None
//==============================================================================
inline void CpiSetRestrictedMode(BOOL bSet) {
	((LPSET_RESTRICTEDMODE)g_pCpiFunc[IDX_SET_RESTRICTEDMODE])(bSet);
}
//==============================================================================
// Description : [CPI function]
//			   : Release/restart hook
//			   :
// Parameters  : bUnhook  TRUE : Release hook
//			   :		  FALSE: Restart hook
//			   :
// Return	   : None
//==============================================================================
inline void CpiUnhook(BOOL bUnhook) {
	((LPUNHOOK)g_pCpiFunc[IDX_UNHOOK])(bUnhook);
}
//==============================================================================
// Description : [CPI function]
//			   : Change caption
//			   :
// Parameters  : pszText  Pointer to caption text
//			   : bSave	  TRUE : Overwrite "Caption" in "Design 1" options
//			   :		  FALSE: Do not overwrite (temporary display change)
//			   :
// Return	   : TRUE : Succeeded
//			   : FALSE: Failed
//==============================================================================
inline BOOL CpiSetCaption(LPCTSTR pszText, BOOL bSave) {
	return ((LPSET_CAPTION)g_pCpiFunc[IDX_SET_CAPTION])(pszText, bSave);
}

// ---- 3. Skin ----------------------------------------------------------------
//==============================================================================
// Description : [CPI function]
//			   : Get path of skin folder
//			   :
// Parameters  : None
//			   :
// Return	   : Path of skin folder
//==============================================================================
inline LPCTSTR CpiGetSkinFolder(void) {
	return ((LPGET_SKINFOLDER)g_pCpiFunc[IDX_GET_SKINFOLDER])();
}
//==============================================================================
// Description : [CPI function]
//			   : Get file name of current skin
//			   :
// Parameters  : None
//			   :
// Return	   : Skin file name
//==============================================================================
inline LPCTSTR CpiGetCurrentSkin(void) {
	return ((LPGET_CURRENTSKIN)g_pCpiFunc[IDX_GET_CURRENTSKIN])();
}
//==============================================================================
// Description : [CPI function]
//			   : Get title of skin specified by file name
//			   :
// Parameters  : pszFileName  Skin file name
//			   :
// Return	   : Skin title
//==============================================================================
inline LPCTSTR CpiGetSkinTitle(LPCTSTR pszFileName) {
	return ((LPGET_SKINTITLE)g_pCpiFunc[IDX_GET_SKINTITLE])(pszFileName);
}
//==============================================================================
// Description : [CPI function]
//			   : Change to skin specified by file name
//			   :
// Parameters  : pszFileName  Skin file name
//			   :
// Return	   : TRUE : Succeeded
//			   : FALSE: Failed
//==============================================================================
inline BOOL CpiChangeSkin(LPCTSTR pszFileName) {
	return ((LPCHANGE_SKIN)g_pCpiFunc[IDX_CHANGE_SKIN])(pszFileName);
}

// ---- 4. Resource / Dialog ---------------------------------------------------
//==============================================================================
// Description : [CPI function]
//			   : Get string resource corresponding to current language
//			   : - If the resource contains a string for the language selected
//			   :   in CLaunch, that string is returned.
//			   :   Otherwise, Win32 API LoadString() is called.
//			   :
// Parameters  : hInst	 Instance handle of module that contains the string
//			   :		 resource (normally plugin module instance)
//			   : uID	 String resource ID
//			   : pszBuf  Pointer to buffer that receives the string resource
//			   : iBufLen Number of characters that can be stored in pszBuf
//			   :		 (including NULL)
//			   :
// Return	   : Number of characters copied (excluding NULL)
//==============================================================================
inline int CpiLoadString(HINSTANCE hInst, UINT uID, LPTSTR pszBuf, int iBufLen) {
	return ((LPLOAD_STRING)g_pCpiFunc[IDX_LOAD_STRING])(hInst, uID, pszBuf, iBufLen);
}
//==============================================================================
// Description : [CPI function]
//			   : Display dialog box using dialog resource for current language
//			   : - If the resource contains a dialog for the language selected
//			   :   in CLaunch, that dialog is used.
//			   :   Otherwise, Win32 API DialogBox() or DialogBoxParam() is
//			   :   called.
//			   :
// Parameters  : hInst	  Instance handle of module that contains the dialog
//			   :		  resource (normally plugin module instance)
//			   : uID	  Dialog resource ID
//			   : hwParent Handle of owner window
//			   : pDlgProc Dialog box procedure
//			   : lParam   Parameter passed to dialog box procedure (optional)
//			   :
// Return	   : Dialog box result
//==============================================================================
inline INT_PTR CpiDialogBox(HINSTANCE hInst, UINT uID, HWND hwParent, DLGPROC pDlgProc, LPARAM lParam=NULL) {
	return ((LPDIALOG_BOX)g_pCpiFunc[IDX_DIALOG_BOX])(hInst, uID, hwParent, pDlgProc, lParam);
}

//==============================================================================
// Description : [CPI function]
//			   : Create a hotkey edit control
//			   :
// Parameters  : hDlg	  Handle to the dialog box containing the edit control
//			   : iEditId  ID of the edit control
//			   : wKey	  Initial key code to display
//			   :
// Return	   : TRUE : Succeeded
//			   : FALSE: Failed
//==============================================================================
inline BOOL CpiCreateHotkeyEdit(HWND hDlg, int iEditId, WORD wKey) {
	return ((LPCREATE_HKEDIT)g_pCpiFunc[IDX_CREATE_HKEDIT])(hDlg, iEditId, wKey);
}

//==============================================================================
// Description : [CPI function]
//			   : Set a key code to the hotkey edit control
//			   :
// Parameters  : hDlg	  Handle to the dialog box containing the edit control
//			   : iEditId  ID of the edit control
//			   : wKey	  Key code to set
//			   :
// Return	   : None
//==============================================================================
inline void CpiSetKeyToHotkeyEdit(HWND hDlg, int iEditId, WORD wKey) {
	((LPSET_KEYTOHKEDIT)g_pCpiFunc[IDX_SET_KEYTOHKEDIT])(hDlg, iEditId, wKey);
}

//==============================================================================
// Description : [CPI function]
//			   : Get the key code entered in the hotkey edit control
//			   :
// Parameters  : hDlg	  Handle to the dialog box containing the edit control
//			   : iEditId  ID of the edit control
//			   :
// Return	   : Currently entered key code
//==============================================================================
inline WORD CpiGetKeyFromHotkeyEdit(HWND hDlg, int iEditId) {
	return ((LPGET_KEYFROMHKEDITE)g_pCpiFunc[IDX_GET_KEYFROMHKEDIT])(hDlg, iEditId);
}

// ---- 5. Plugin config -------------------------------------------------------
//==============================================================================
// Description : [CPI function]
//			   : Register plugin as a button item
//			   :
// Parameters  : pszRegName  Plugin item name
//			   :
// Return	   : None
//==============================================================================
inline void CpiRegisterButtonItem(LPCTSTR pszRegName) {
	((LPREG_BUTTONITEM)g_pCpiFunc[IDX_REG_BUTTONITEM])(g_iPluginId, pszRegName);
}
//==============================================================================
// Description : [CPI function]
//			   : Set integer data for plugin
//			   : - If pszSect is NULL, data is stored in [General] section.
//			   :
// Parameters  : pszSect  Section name in INI file
//			   : pszKey   Data key name in INI file
//			   : iData	  Integer value to set
//			   :
// Return	   : None
//==============================================================================
inline void CpiSetIntData(LPCTSTR pszSect, LPCTSTR pszKey, int iData) {
	((LPSET_INTDATA)g_pCpiFunc[IDX_SET_INTDATA])(g_iPluginId, pszSect, pszKey, iData);
}
//==============================================================================
// Description : [CPI function]
//			   : Set string data for plugin
//			   : - If pszSect is NULL, data is stored in [General] section.
//			   :
// Parameters  : pszSect  Section name in INI file
//			   : pszKey   Data key name in INI file
//			   : pszData  String data to set
//			   :
// Return	   : Address of buffer where string data is stored
//			   : (NULL if saving failed)
//==============================================================================
inline LPCTSTR CpiSetStrData(LPCTSTR pszSect, LPCTSTR pszKey, LPCTSTR pszData) {
	return ((LPSET_STRDATA)g_pCpiFunc[IDX_SET_STRDATA])(g_iPluginId, pszSect, pszKey, pszData);
}
//==============================================================================
// Description : [CPI function]
//			   : Get integer data set for plugin
//			   : - If pszSect is NULL, data is read from [General] section.
//			   :
// Parameters  : pszSect   Section name in INI file
//			   : pszKey    Data key name in INI file
//			   : iDefault  Value returned when retrieval fails (default: 0)
//			   :
// Return	   : Integer data set for plugin (iDefault on failure)
//==============================================================================
inline int CpiGetIntData(LPCTSTR pszSect, LPCTSTR pszKey, int iDefault=0) {
	return ((LPGET_INTDATA)g_pCpiFunc[IDX_GET_INTDATA])(g_iPluginId, pszSect, pszKey, iDefault);
}
//==============================================================================
// Description : [CPI function]
//			   : Get string data set for plugin
//			   : - If pszSect is NULL, data is read from [General] section.
//			   :
// Parameters  : pszSect  Section name in INI file
//			   : pszKey   Data key name in INI file
//			   :
// Return	   : Pointer to buffer that contains string data (NULL on failure)
//==============================================================================
inline LPCTSTR CpiGetStrData(LPCTSTR pszSect, LPCTSTR pszKey) {
	return ((LPGET_STRDATA)g_pCpiFunc[IDX_GET_STRDATA])(g_iPluginId, pszSect, pszKey);
}
//==============================================================================
// Description : [CPI function]
//			   : Save plugin settings to settings file
//			   :
// Parameters  : None
//			   :
// Return	   : None
//==============================================================================
inline void CpiSaveSettings(void) {
	((LPSAVE_SETTINGS)g_pCpiFunc[IDX_SAVE_SETTINGS])(g_iPluginId);
}
//==============================================================================
// Description : [CPI function]
//			   : Set integer data for plugin item
//			   :
// Parameters  : uItemId  Item identifier
//			   : idx	  Data index (slot number in an array-like structure)
//			   :		  * It is recommended to use sequential zero-based
//			   :			indices. Large index values will allocate unused
//			   :			slots and increase configuration file size.
//			   : iData	  Integer value to set
//			   :
// Return	   : None
//==============================================================================
inline void CpiSetItemInt(UINT uItemId, int idx, int iData) {
	((LPSET_ITEMINT)g_pCpiFunc[IDX_SET_ITEMINT])(uItemId, idx, iData);
}
//==============================================================================
// Description : [CPI function]
//			   : Set string data for plugin item
//			   :
// Parameters  : uItemId  Item identifier
//			   : idx	  Data index (slot number in an array-like structure)
//			   :		  * It is recommended to use sequential zero-based
//			   :			indices. Large index values will allocate unused
//			   :			slots and increase configuration file size.
//			   : pszData  String data to set
//			   :
// Return	   : Address of buffer where string data is stored
//			   : (NULL if saving failed)
//==============================================================================
inline LPCTSTR CpiSetItemStr(UINT uItemId, int idx, LPCTSTR pszData) {
	return ((LPSET_ITEMSTR)g_pCpiFunc[IDX_SET_ITEMSTR])(uItemId, idx, pszData);
}
//==============================================================================
// Description : [CPI function]
//			   : Get integer data set for plugin item
//			   :
// Parameters  : uItemId   Item identifier
//			   : idx	   Data index (slot number in an array-like structure)
//			   : iDefault  Value returned when retrieval fails (default: 0)
//			   :
// Return	   : Integer data set for item (iDefault on failure)
//==============================================================================
inline int CpiGetItemInt(UINT uItemId, int idx, int iDefault=0) {
	return ((LPGET_ITEMINT)g_pCpiFunc[IDX_GET_ITEMINT])(uItemId, idx, iDefault);
}
//==============================================================================
// Description : [CPI function]
//			   : Get string data set for plugin item
//			   :
// Parameters  : uItemId  Item identifier
//			   : idx	  Data index (slot number in an array-like structure)
//			   :
// Return	   : Pointer to buffer that contains string data
//			   :		   (NULL if retrieval failed)
//==============================================================================
inline LPCTSTR CpiGetItemStr(UINT uItemId, int idx) {
	return ((LPGET_ITEMSTR)g_pCpiFunc[IDX_GET_ITEMSTR])(uItemId, idx);
}
//==============================================================================
// Description : [CPI function]
//			   : Delete data set for plugin
//			   : - If pszSect is NULL, data is deleted from [General] section.
//			   :
// Parameters  : pszSect  Section name in INI file
//			   : pszKey   Data key name in INI file
//			   :
// Return	   : None
//==============================================================================
inline void CpiDeleteData(LPCTSTR pszSect, LPCTSTR pszKey) {
	((LPDELETE_DATA)g_pCpiFunc[IDX_DELETE_DATA])(g_iPluginId, pszSect, pszKey);
}

//==============================================================================
// Description : [CPI function]
//			   : Delete integer data for plugin items from the specified index
//			   : onward
//			   : * Individual entries cannot be deleted
//			   :   (to preserve index consistency)
//			   :
// Parameters  : uItemId  Item identifier
//			   : idx	  Data index to start deletion
//			   :
// Return	   : None
//==============================================================================
inline void CpiDeleteItemInt(UINT uItemId, int idx) {
	((LPDELETE_ITEMINT)g_pCpiFunc[IDX_DELETE_ITEMINT])(uItemId, idx);
}

//==============================================================================
// Description : [CPI function]
//			   : Delete string data for plugin items from the specified index
//			   : onward
//			   : * Individual entries cannot be deleted
//			   :   (to preserve index consistency)
//			   :
// Parameters  : uItemId  Item identifier
//			   : idx	  Data index to start deletion
//			   :
// Return	   : None
//==============================================================================
inline void CpiDeleteItemStr(UINT uItemId, int idx) {
	((LPDELETE_ITEMSTR)g_pCpiFunc[IDX_DELETE_ITEMSTR])(uItemId, idx);
}

// ---- 6. Others --------------------------------------------------------------
//==============================================================================
// Description : [CPI function]
//			   : Perform specified operation on the specified window
//			   :
// Parameters  : hWnd  Handle of target window
//			   : uCmd  Window control command
//			   :
// Return	   : TRUE : Succeeded
//			   : FALSE: Failed
//==============================================================================
inline BOOL CpiWindowOperations(HWND hWnd, UINT uCmd) {
	return ((LPWINDOW_CONTROL)g_pCpiFunc[IDX_WINDOW_CONTROL])(hWnd, uCmd);
}

