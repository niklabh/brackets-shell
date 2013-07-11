/*************************************************************************
 *
 * ADOBE CONFIDENTIAL
 * ___________________
 *
 *  Copyright 2013 Adobe Systems Incorporated
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of Adobe Systems Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Adobe Systems Incorporated and its
 * suppliers and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Adobe Systems Incorporated.
 **************************************************************************/
#include "cef_main_window.h"
#include "cef_registry.h"
#include "resource.h"

extern HINSTANCE	        gInstance;
extern TCHAR		        gInitialUrl[];

extern CefRefPtr<ClientHandler> gHandler;

static const TCHAR			gWindowClassname[] = _T("cef_main_winow");
static const wchar_t		gWindowPostionFolder[] = L"Window Position";

static const wchar_t		gPrefLeft[] = L"Left";
static const wchar_t		gPrefTop[] = L"Top";
static const wchar_t		gPrefWidth[] = L"Width";
static const wchar_t		gPrefHeight[] = L"Height";

static const wchar_t		gPrefRestoreLeft[] = L"Restore Left";
static const wchar_t		gPrefRestoreTop[] = L"Restore Top";
static const wchar_t		gPrefRestoreRight[] = L"Restore Right";
static const wchar_t		gPrefRestoreBottom[]	= L"Restore Bottom";
static const wchar_t		gPrefShowState[] = L"Show State";

static const long			gMinWindowWidth = 390;
static const long			gMinWindowHeight = 200;

void cef_main_window::RegisterWndClass()
{
	static ATOM classAtom = 0;
	if (classAtom == 0) 
	{
		DWORD menuId = IDC_CEFCLIENT;
		WNDCLASSEX wcex;

		::ZeroMemory (&wcex, sizeof (wcex));
		wcex.cbSize = sizeof(WNDCLASSEX);

		wcex.style         = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc   = DefWindowProc;
		wcex.cbClsExtra    = 0;
		wcex.cbWndExtra    = 0;
		wcex.hInstance     = hInstance;
		wcex.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CEFCLIENT));
		wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
		wcex.lpszMenuName  = MAKEINTRESOURCE(menuId);
		wcex.lpszClassName = ::gWindowClassname;
		wcex.hIconSm       = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

		classAtom = RegisterClassEx(&wcex);
	}
}

BOOL cef_main_window::Create() 
{
	RegisterWndClass();

	int left   = CW_USEDEFAULT;
	int top    = CW_USEDEFAULT;
	int width  = CW_USEDEFAULT;
	int height = CW_USEDEFAULT;
	int showCmd = SW_SHOW;

	RestoreWindowRect(left, top, width, height, showCmd);

	DWORD styles = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_EX_COMPOSITED;
	
	if (showCmd == SW_MAXIMIZE)
	  styles |= WS_MAXIMIZE;

	static TCHAR szTitle[100];
	LoadString(::g_hInst, IDS_APP_TITLE, szTitle, _countof(szTitle));	

	if (!cef_window::CreateWindow(::gWindowClassname, szTitle,
								styles, left, top, width, height, NULL, NULL, hInstance, NULL)) 
	{
		return false;
	}

	NativeMenuModel::getInstance(m_hWnd).setTag(L"help", HELP_MENUITEMTAG);
	HMENU mainMenu = GetMenu();  
	NativeMenuModel::getInstance(m_hWnd).setOsItem(HELP_MENUITEMTAG, (void*)mainMenu);
	
	RestoreWindowPlacement(showCmd);
	UpdateWindow();
}

void cef_main_window::PostNonClientDestory()
{
	delete this;
}


BOOL cef_main_window::HandleCreate() 
{
	// Create the single static handler class instance
    gHandler = new ClientHandler();
    gHandler->SetMainHwnd(m_hWnd);

    RECT rect;
    int x = 0;

    GetClientRect(&rect);

    CefWindowInfo info;
    CefBrowserSettings settings;

    settings.web_security = STATE_DISABLED;

    // Initialize window info to the defaults for a child window
    info.SetAsChild(m_hWnd, rect);

    // Creat the new child browser window
    CefBrowserHost::CreateBrowser(info,
        static_cast<CefRefPtr<CefClient> >(gHandler),
        gInitialUrl, settings);

	return TRUE;
}

CefWindowHandle cef_main_window::SafeGetCefBrowserHwnd()
{
	if (gHandler.get() && gHandler->GetBrowser())
		return gHandler->GetBrowser()->GetHost()->GetWindowHandle();
	return NULL;
}


BOOL cef_main_window::HandleEraseBackground()
{
	return (SafeGetCefBrowserHwnd() != NULL);
}

BOOL cef_main_window::HandleSetFocus()
{
    // Pass focus to the browser window
    CefWindowHandle hwnd = SafeGetCefBrowserHwnd();
    if (hwnd) 
	{
		::PostMessage(hwnd, WM_SETFOCUS, wParam, NULL);
		return TRUE;
	}
	}
	return FALSE;
}

BOOL cef_main_window::HandlePaint()
{
	PAINTSTRUCT* ps;
	BeginPaint(ps);
	EndPaint(ps);
}

BOOL cef_main_window::HandleGetMinMaxInfo(LPMINMAXINFO mmi)
{
	mmi->ptMinTrackSize.x = ::gMinWindowWidth;
	mmi->ptMinTrackSize.y = ::gMinWindowHeight;
}

BOOL cef_main_window::HandleDestroy()
{
	::PostQuitMessage(0);
	return TRUE;
}

BOOL cef_main_window::HandleClose()
{
    SaveWindowRect();

	if (gHandler.get()) 
	{
        // If we already initiated the browser closing, then let default window proc handle it.
        HWND hwnd = SafeGetCefBrowserHwnd();
        HANDLE closing = GetProp(hwnd, CLOSING_PROP);
        if (closing) 
		{
			RemoveProp(hwnd, CLOSING_PROP);
		} 
		else 
		{
			gHandler->QuittingApp(true);
			gHandler->DispatchCloseToNextBrowser();
		}
		return TRUE;
    } 
		
	return FALSE;
}

BOOL cef_main_window::HandleExitCommand()
{
	if (gHandler.get()) {
		gHandler->QuittingApp(true);
		gHandler->DispatchCloseToNextBrowser();
	} else {
		DestroyWindow();
	}
	return TRUE;
}


BOOL cef_main_window::HandleSize(BOOL bMinimize)
{
	// Minimizing resizes the window to 0x0 which causes our layout to go all
	// screwy, so we just ignore it.
	CefWindowHandle hwnd = SafeGetCefBrowserHwnd();
	if (hwnd && !bMinimize) 
	{
		RECT rect;
		::GetClientRect(hWnd, &rect);

		HDWP hdwp = BeginDeferWindowPos(1);
		hdwp = DeferWindowPos(hdwp, hwnd, NULL, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER);
		EndDeferWindowPos(hdwp);
	}
      
	return FALSE;

}

BOOL cef_main_window::HandleInitMenuPopup(HMENU hMenuPopup)
{
    int count = ::GetMenuItemCount(hMenuPopup);
    void* menuParent = ::getMenuParent(gHandler->GetBrowser());
    for (int i = 0; i < count; i++) {
        UINT id = ::GetMenuItemID(menu, i);

        bool enabled = NativeMenuModel::getInstance(menuParent).isMenuItemEnabled(id);
        UINT flagEnabled = enabled ? MF_ENABLED : (MF_GRAYED | MF_DISABLED);
        EnableMenuItem(menu, id,  flagEnabled | MF_BYCOMMAND);

        bool checked = NativeMenuModel::getInstance(menuParent).isMenuItemChecked(id);
        UINT flagChecked = checked ? MF_CHECKED : MF_UNCHECKED;
        ::CheckMenuItem(menu, id, flagChecked | MF_BYCOMMAND);
    }
	return FALSE;
}

BOOL cef_main_winow::HandleCommand(UINT commandId)
{
	switch (commandId) 
	{
	case IDM_EXIT:
		HandleExitCommand();
		return TRUE;
	default:
        ExtensionString commandId = NativeMenuModel::getInstance(::getMenuParent(gHandler->GetBrowser())).getCommandId(commandId);
        if (commandId.size() > 0) 
		{
            gHandler->SendJSCommand(gHandler->GetBrowser(), commandId);
			return TRUE;
        }
	}

	return FALSE;
}

LRESULT cef_main_window::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) 
	{
	case WM_CREATE:
		if (HandleCreate())
			return 0L;
		break;
	case WM_ERASEBKGND:
		if (HandleEraseBackground())
			return 0L;
		break;
	case WM_SETFOCUS:
		if (HandleSetFocus())
			return 0L;
		break;
	case WM_PAINT:
		if (HandlePaint())
			return 0L;
		break;
	case WM_GETMINMAXINFO:
		if (HandleGetMinMaxInfo((LPMINMAXINFO) lParam))
			return 0L;
		break;
	case WM_DESTROY:
		if (HandleDestroy())
			return 0L;
		break;
	case WM_CLOSE:
		if (HandleClose())
			return 0L;
		break;
	case WM_SIZE:
		if (HandleSize(wParam == SIZE_MINIMIZED))
			return 0L;
		break;
	case WM_INITMENUPOPUP:
		if (HandleInitMenuPopup((HMENU)wParam)
			return 0L;
		break;
	case WM_COMMAND:
		if (HandleCommand(LOWORD(wParam)))
			return 0L;
		break;
	}

    return cef_window::WindowProc(message, wParam, lParam);
}


void cef_main_window::SaveWindowRect()
{
	WINDOWPLACEMENT wp;
	::ZeroMemory(&wp, sizeof(wp));
	wp.length = sizeof(wp);
	 
	if (GetWindowPlacement(&wp))
	{
		// Only save window positions for "restored" and "maximized" states.
		// If window is closed while "minimized", we don't want it to open minimized
		// for next session, so don't update registry so it opens in previous state.
		if (wp.showCmd == SW_SHOWNORMAL || wp.showCmd == SW_SHOW || wp.showCmd == SW_MAXIMIZE)
		{
			RECT rect;
			if (GetWindowRect(&rect))
			{
				::WriteRegistryInt(gWindowPostionFolder, gPrefLeft,   rect.left);
				::WriteRegistryInt(gWindowPostionFolder, gPrefTop,    rect.top);
				::WriteRegistryInt(gWindowPostionFolder, gPrefWidth,  rect.right - rect.left);
				::WriteRegistryInt(gWindowPostionFolder, gPrefHeight, rect.bottom - rect.top);
			}

			if (wp.showCmd == SW_MAXIMIZE)
			{
				// When window is maximized, we also store the "restore" size
				::WriteRegistryInt(gWindowPostionFolder, gPrefRestoreLeft,   wp.rcNormalPosition.left);
				::WriteRegistryInt(gWindowPostionFolder, gPrefRestoreTop,    wp.rcNormalPosition.top);
				::WriteRegistryInt(gWindowPostionFolder, gPrefRestoreRight,  wp.rcNormalPosition.right);
				::WriteRegistryInt(gWindowPostionFolder, gPrefRestoreBottom, wp.rcNormalPosition.bottom);
			}

			// Maximize is the only special case we handle
			::WriteRegistryInt(gWindowPostionFolder, gPrefShowState,
							 (wp.showCmd == SW_MAXIMIZE) ? SW_MAXIMIZE : SW_SHOW);
		}
	}
}


void cef_main_window::RestoreWindowRect(int& left, int& top, int& width, int& height, int& showCmd)
{
	::GetRegistryInt(gWindowPostionFolder, gPrefLeft,  			NULL, left);
	::GetRegistryInt(gWindowPostionFolder, gPrefTop,   			NULL, top);
	::GetRegistryInt(gWindowPostionFolder, gPrefWidth, 			NULL, width);
	::GetRegistryInt(gWindowPostionFolder, gPrefHeight,			NULL, height);
	::GetRegistryInt(gWindowPostionFolder, gWindowPostionFolder,	NULL, showCmd);
}

void cef_main_window::RestoreWindowPlacement(int showCmd)
{
	if (showCmd == SW_MAXIMIZE)
	{
		WINDOWPLACEMENT wp;
		::ZeroMemory(&wp, sizeof (wp));
		wp.length = sizeof(WINDOWPLACEMENT);

		wp.flags	= 0;
		wp.showCmd	= SW_MAXIMIZE;
		wp.ptMinPosition.x	= -1;
		wp.ptMinPosition.y	= -1;
		wp.ptMaxPosition.x	= -1;
		wp.ptMaxPosition.y	= -1;

		wp.rcNormalPosition.left	= CW_USEDEFAULT;
		wp.rcNormalPosition.top		= CW_USEDEFAULT;
		wp.rcNormalPosition.right	= CW_USEDEFAULT;
		wp.rcNormalPosition.bottom	= CW_USEDEFAULT;

		GetRegistryInt(gWindowPostionFolder, gPrefRestoreLeft,	NULL, (int&)wp.rcNormalPosition.left);
		GetRegistryInt(gWindowPostionFolder, gPrefRestoreTop,		NULL, (int&)wp.rcNormalPosition.top);
		GetRegistryInt(gWindowPostionFolder, gPrefRestoreRight,	NULL, (int&)wp.rcNormalPosition.right);
		GetRegistryInt(gWindowPostionFolder, gPrefRestoreBottom,	NULL, (int&)wp.rcNormalPosition.bottom);

		// This returns FALSE on failure, but not sure what we could in that case
		SetWindowPlacement(&wp);
	}
}
