#pragma once
/* Shim <winuser.h> (Win32 user/GDI messages & dialog constants) for the
 * non-Windows experimental OpenTS build. Only the symbols the engine
 * references are provided. Real windowing is out of scope for the headless
 * core; these let the dialog/GUI translation units parse. */
#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "code/port/shim/winuser.h may only be included under OPENTS_EXPERIMENTAL_NONWIN32"
#endif
#include "windows_stub.h"

/* Dialog control / result IDs. */
#define IDOK                1
#define IDCANCEL            2
#define IDABORT             3
#define IDRETRY             4
#define IDIGNORE            5
#define IDYES               6
#define IDNO                7

/* Window message constants (Win32 values). */
#define WM_NULL             0x0000
#define WM_CREATE           0x0001
#define WM_DESTROY          0x0002
#define WM_MOVE             0x0003
#define WM_SIZE             0x0005
#define WM_SETFOCUS         0x0007
#define WM_KILLFOCUS        0x0008
#define WM_ENABLE           0x000A
#define WM_SETTEXT          0x000C
#define WM_GETTEXT          0x000D
#define WM_GETTEXTLENGTH    0x000E
#define WM_PAINT            0x000F
#define WM_CLOSE            0x0010
#define WM_QUIT             0x0012
#define WM_ERASEBKGND       0x0014
#define WM_SETCURSOR        0x0020
#define WM_DRAWITEM         0x002B
#define WM_MEASUREITEM      0x002C
#define WM_DELETEITEM       0x002D
#define WM_COMMAND          0x0111
#define WM_SYSCOMMAND       0x0112
#define WM_TIMER            0x0113
#define WM_HSCROLL          0x0114
#define WM_VSCROLL          0x0115
#define WM_INITDIALOG       0x0110
#define WM_INITMENU         0x0116
#define WM_MOUSEMOVE        0x0200
#define WM_LBUTTONDOWN      0x0201
#define WM_LBUTTONUP        0x0202
#define WM_LBUTTONDBLCLK    0x0203
#define WM_RBUTTONDOWN      0x0204
#define WM_RBUTTONUP        0x0205
#define WM_RBUTTONDBLCLK    0x0206
#define WM_MBUTTONDOWN      0x0207
#define WM_MBUTTONUP        0x0208
#define WM_MBUTTONDBLCLK    0x0209
#define WM_MOUSEWHEEL       0x020A
#define WM_MOUSEHWHEEL      0x020E
#define WM_KEYDOWN          0x0100
#define WM_KEYUP            0x0101
#define WM_CHAR             0x0102
#define WM_SYSKEYDOWN       0x0104
#define WM_SYSKEYUP         0x0105
#define WM_SYSCHAR          0x0106
#define WM_NCDESTROY        0x0082
#define WM_NCHITTEST        0x0084
#define WM_NCPAINT          0x0085
#define WM_CAPTURECHANGED   0x0215
#define WM_MOVING           0x0216
#define WM_ENTERSIZEMOVE    0x0231
#define WM_EXITSIZEMOVE     0x0232
#define WM_CONTEXTMENU      0x007B
#define WM_HELP             0x0053
#define WM_USER             0x0400

/* Button/style bits used by dialog templates. */
#define BS_PUSHBUTTON       0x00000000L
#define BS_DEFPUSHBUTTON    0x00000001L
#define BS_CHECKBOX         0x00000002L
#define BS_GROUPBOX         0x00000007L
#define WS_CHILD            0x40000000L
#define WS_VISIBLE          0x10000000L
#define WS_CAPTION          0x00C00000L
#define SS_LEFT             0x00000000L
#define SS_CENTER           0x00000001L
#define ES_LEFT             0x00000000L
#define ES_MULTILINE        0x00000004L
#define LBS_NOTIFY          0x00000001L

/* GetWindow / GetParent flags. */
#define GW_CHILD            5
#define GW_HWNDNEXT         2
#define GW_OWNER            4

/* PeekMessage flags. */
#define PM_NOREMOVE         0x0000
#define PM_REMOVE           0x0001
#define PM_NOYIELD          0x0002

/* Font weight. */
#define FW_NORMAL           400
#define FW_BOLD             700

/* SNDMSG: the engine's winfix.h macros (Slider_SetRange, etc.) expand to
 * SNDMSG(...). On Windows it is <windows.h>'s SendMessage alias; mirror that. */
#ifndef SNDMSG
#define SNDMSG SendMessage
#endif

/* Dialog window-procedure pointer type. */
typedef LRESULT (CALLBACK *DLGPROC)(HWND, UINT, WPARAM, LPARAM);

/* PeekMessage -- non-destructive peek at the front of the port's message queue. */
inline BOOL PeekMessage(MSG *lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg) {
	(void)hWnd; (void)wMsgFilterMin; (void)wMsgFilterMax;
	/*
	**	On Win32, PeekMessage is how the application receives events from the
	**	operating system, so this is where the platform queue has to be drained.
	**	Here that queue is Cocoa's: an NSApplication that is never pumped never
	**	posts its windows to the window server, and a window that exists but was
	**	never composited stays 0x0 and off screen -- visible to CGWindowList as a
	**	zero-size entry, and drawing to nothing. Draining is cheap when there is
	**	nothing pending (the fetch returns immediately rather than waiting), and
	**	the engine calls this several times per frame from every path that has to
	**	stay responsive, including the menu engine's own delay loop.
	*/
	opents_window_pump();
	OpentsMsg m;
	bool const have = opents_msg_peek(&m);

	/*
	**	PM_REMOVE takes the message out of the queue, PM_NOREMOVE leaves it. The
	**	engine's own handler peeks and then GetMessage-pops, so it never needed
	**	the flag; but the dialog waits (WS_Wait_Dialog, and the multiplayer
	**	connect screens) pump the queue with PeekMessage(PM_REMOVE) alone and no
	**	GetMessage. Ignoring the flag left the front message in place for ever,
	**	so those loops read the same message over and over and never gave the
	**	dialog a chance to paint or to be dismissed -- which is what the
	**	multiplayer map selector did as soon as it opened.
	*/
	if (have && (wRemoveMsg & PM_REMOVE) != 0) {
		opents_msg_pop(&m);
	}

	if (have) {
		if (lpMsg != nullptr) {
			lpMsg->hwnd = (HWND)m.hwnd;
			lpMsg->message = m.message;
			lpMsg->wParam = (WPARAM)m.wParam;
			lpMsg->lParam = (LPARAM)m.lParam;
			lpMsg->time = 0;
			lpMsg->pt.x = 0;
			lpMsg->pt.y = 0;
		}
		return TRUE;
	}
	return FALSE;
}

/* Extra message / cursor / window helpers referenced by the dialog layer. */
#define WM_XBUTTONDOWN      0x020B
#define WM_XBUTTONUP        0x020C
#define WM_MOUSEACTIVATE    0x0021
#define WM_ACTIVATE         0x0006
#define WM_ACTIVATEAPP      0x001C
#define WM_SETCURSOR        0x0020

/* Returns the new display count, so a caller counting down to zero to hide the
 * cursor sees it decrease. */
inline BOOL ShowCursor(BOOL bShow) { return (BOOL)opents_window_show_cursor(bShow ? 1 : -1); }
inline BOOL GetWindowRect(HWND hWnd, RECT *lpRect) {
	if (lpRect == nullptr) return FALSE;
	/* The native window has a real on-screen frame; logical windows report
	 * their rect in the shared desktop space -- parent chain summed in
	 * game-frame units, anchored at the main window's client origin -- so
	 * that Get_Display_Rect's subtraction yields game coordinates. */
	if (hWnd != nullptr && opents_win_is_main((void *)hWnd)) {
		int x = 0, y = 0, w = 0, h = 0;
		opents_window_get_window_rect((void *)hWnd, &x, &y, &w, &h);
		lpRect->left = x; lpRect->top = y;
		lpRect->right = x + w; lpRect->bottom = y + h;
		return TRUE;
	}
	int x = 0, y = 0, w = 0, h = 0;
	Opents_Logical_Screen_Rect(hWnd, &x, &y, &w, &h);
	lpRect->left = x; lpRect->top = y;
	lpRect->right = x + w; lpRect->bottom = y + h;
	return TRUE;
}
inline HWND GetWindow(HWND hWnd, UINT uCmd) { return (HWND)opents_win_get_window((void *)hWnd, (unsigned)uCmd); }
inline UINT GetACP(void) { return 1252; }
/* CP_ACP now comes from windows_stub.h (included above). */
#define ANSI_CHARSET        0

/* Dialog / control helpers used by the near-green dialog TUs. */
/* DWLP_USER / DWLP_DLGPROC / DWLP_MSGRESULT come from windows_stub.h (line 9
   above), which gets them right. Repeating them here shadowed DWLP_USER with 0,
   colliding with DWLP_MSGRESULT -- the two are distinct slots in Win32 and a
   dialog that stores its user data in one would have read the other back. */
inline HWND GetDlgItem(HWND hDlg, int nIDDlgItem) {
	return (HWND)opents_win_find_child((void *)hDlg, (unsigned)nIDDlgItem);
}
/* Declared here rather than through video.h because this header is included
 * before it, and the definition the engine gives matches this one. */
void Video_Mark_Dirty(void);
inline BOOL ValidateRect(HWND hWnd, const RECT * /*lpRect*/) {
	opents_win_validate((void *)hWnd);
	/*
	 * On Win32 validating means "this area is now correct on the screen", so the
	 * compositor's frame is by definition out of date the moment it runs. The
	 * dialog loop draws into surfaces and then validates; without this the
	 * dirty flag stays down and Video_Present_If_Dirty never presents, leaving
	 * the previous frame -- whatever the menu drew last -- on the glass.
	 */
	Video_Mark_Dirty();
	return TRUE;
}
inline BOOL TranslateMessage(const MSG * /*lpMsg*/) { return TRUE; }
inline HWND GetCapture(void) { return (HWND)opents_input_get_capture(); }
inline HWND SetActiveWindow(HWND /*hWnd*/) { return NULL_HANDLE; }
inline BOOL IsWindowVisible(HWND hWnd) { return opents_win_is_visible((void *)hWnd) ? TRUE : FALSE; }
inline BOOL IsDialogMessage(HWND hDlg, MSG *lpMsg) {
	/* Modeless dialogs consume Tab/Return/Escape before the general procedure
	 * sees them. The manager reports whether it took the message. */
	if (hDlg == nullptr || lpMsg == nullptr) return FALSE;
	uintptr_t wParam = (uintptr_t)lpMsg->wParam;
	intptr_t lParam = (intptr_t)lpMsg->lParam;
	int taken = opents_win_is_dialog_message((void *)hDlg, (unsigned)lpMsg->message, &wParam, &lParam);
	if (taken) {
		lpMsg->wParam = (WPARAM)wParam;
		lpMsg->lParam = (LPARAM)lParam;
	}
	return taken ? TRUE : FALSE;
}
/* CreateDirectory now comes from windows_stub.h (included above). */
inline BOOL EnumDisplaySettings(const char * /*lpszDeviceName*/, DWORD /*iModeNum*/, void * /*lpDevMode*/) { return FALSE; }
inline BOOL GetFileTime(HANDLE /*hFile*/, void * /*lpCreationTime*/, void * /*lpLastAccessTime*/, void * /*lpLastWriteTime*/) { return FALSE; }
/* WNDPROC now comes from windows_stub.h (included above). */
#define MAKEPOINTS(l)       (*((POINTS *)&(l)))
#define CB_RESETCONTENT     0x014B
#define WM_XBUTTONDBLCLK    0x020D
#define CP_UTF8             65001
#define OUT_RASTER_PRECIS   6

inline int TranslateAccelerator(HWND /*hWnd*/, void * /*hAccel*/, MSG * /*lpMsg*/) { return 0; }
