/*
**  port_windows.cpp -- the logical window manager. See port_windows.h.
**
**  Design notes kept next to the code they justify:
**
**  * Dialog-unit conversion is fixed at the classic MS Sans Serif 8pt
**    base. Language.dll's 74 templates all carry that font (measured),
**    and the engine rescales everything afterwards anyway through its
**    own Resize_Dialog pass, so only self-consistency matters here.
**  * Control state (list box contents, check state, slider position) is
**    kept per window because the engine's owner-draw layer reads it
**    back through messages rather than drawing from its own model.
**  * Subclassing follows Win32 exactly: SetWindowLong(kGWL_WNDPROC)
**    returns the previous procedure and future messages go to the new
**    one; CallWindowProc reaches the stored previous one.
*/
#include "port_windows.h"

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>

#include "port_bridge.h"   /* OpentsMsg / opents_msg_push, for posting */
#include "port_input.h"    /* capture, for the push buttons' mouse hold */

/*
** DIAG (skirmish Name box): low-volume event log for the dialog layer --
** focus changes, clicks landing on the edit, characters inserted. Goes to
** /tmp/win_diag.txt so the click/focus/typing chain can be traced.
** Defined at global scope below the anonymous namespace, so engine-side
** diagnostics (msgroute, mapgen) can report through it too. Declared here,
** before the anonymous namespace opens, so calls inside it resolve to the
** one global function without becoming ambiguous.
*/
void Win_Diag(char const * fmt, ...);

namespace {

/* ------------------------------------------------------------------ */
/* Win32 constants this file needs. The shim defines these for the     */
/* engine's translation units; this file is deliberately compiled      */
/* without it, so the stable ABI values are restated here.             */
/* ------------------------------------------------------------------ */

enum : unsigned {
	kWM_NULL           = 0x0000,
	kWM_CREATE         = 0x0001,
	kWM_DESTROY        = 0x0002,
	kWM_MOVE           = 0x0003,
	kWM_SIZE           = 0x0005,
	kWM_NCDESTROY      = 0x0082,
	kWM_NCHITTEST      = 0x0084,
	kWM_SETFOCUS       = 0x0007,
	kWM_KILLFOCUS      = 0x0008,
	kWM_ENABLE         = 0x000A,
	kWM_SETTEXT        = 0x000C,
	kWM_GETTEXT        = 0x000D,
	kWM_GETTEXTLENGTH  = 0x000E,
	kWM_PAINT          = 0x000F,
	kWM_CLOSE          = 0x0010,
	kWM_ERASEBKGND     = 0x0014,
	kWM_SHOWWINDOW     = 0x0018,
	kWM_ACTIVATEAPP    = 0x001C,
	kWM_SETCURSOR      = 0x0020,
	kWM_MOUSEMOVE      = 0x0200,
	kWM_LBUTTONDOWN    = 0x0201,
	kWM_LBUTTONUP      = 0x0202,
	kWM_LBUTTONDBLCLK  = 0x0203,
	kWM_NEXTDLGCTL     = 0x0028,
	kWM_DRAWITEM       = 0x002B,
	kWM_MEASUREITEM    = 0x002C,
	kWM_KEYDOWN        = 0x0100,
	kWM_KEYUP          = 0x0101,
	kWM_CHAR           = 0x0102,
	kWM_SYSKEYDOWN     = 0x0104,
	kWM_INITDIALOG     = 0x0110,
	kWM_COMMAND        = 0x0111,
	kWM_SYSCOMMAND     = 0x0112,
	kWM_TIMER          = 0x0113,
	kWM_HSCROLL        = 0x0114,
	kWM_VSCROLL        = 0x0115,
	kWM_CTLCOLORMSGBOX = 0x0132,
	kWM_GETDLGCODE     = 0x0087,
	kWM_SETFONT        = 0x0030,
	kWM_GETFONT        = 0x0031,
	kWM_USER           = 0x0400,
};

enum : unsigned {
	kLB_ADDSTRING     = 0x0180,
	kLB_INSERTSTRING  = 0x0181,
	kLB_DELETESTRING  = 0x0182,
	kLB_RESETCONTENT  = 0x0184,
	kLB_SETCURSEL     = 0x0186,
	kLB_GETCURSEL     = 0x0188,
	kLB_GETTEXT       = 0x0189,
	kLB_GETTEXTLEN    = 0x018A,
	kLB_GETCOUNT      = 0x018B,
	kLB_GETTOPINDEX   = 0x018E,
	kLB_FINDSTRING    = 0x018F,
	kLB_SETCOLUMNWIDTH= 0x0194,
	kLB_SETTOPINDEX   = 0x0197,
	kLB_GETITEMDATA   = 0x0199,
	kLB_SETITEMDATA   = 0x019A,
};

enum : unsigned {
	kCB_GETEDITSEL     = 0x0140,
	kCB_ADDSTRING      = 0x0143,
	kCB_INSERTSTRING   = 0x014A,
	kCB_DELETESTRING   = 0x0144,
	kCB_RESETCONTENT   = 0x014B,
	kCB_SETCURSEL      = 0x014E,
	kCB_SHOWDROPDOWN   = 0x014F,
	kCB_GETITEMDATA    = 0x0150,
	kCB_SETITEMDATA    = 0x0151,
	kCB_GETCOUNT       = 0x0146,
	kCB_GETCURSEL      = 0x0147,
	kCB_GETLBTEXT      = 0x0148,
	kCB_GETLBTEXTLEN   = 0x0149,
	kCB_GETITEMHEIGHT  = 0x0154,
	kCB_SETTOPINDEX    = 0x0159,
	kCB_GETTOPINDEX    = 0x0158,
	kCB_FINDSTRING     = 0x014C,
};

enum : unsigned {
	kBM_GETCHECK = 0x00F0,
	kBM_SETCHECK = 0x00F1,
	kBM_GETSTATE = 0x00F2,
	kBM_SETSTATE = 0x00F3,
};

/* Trackbar (commctrl) and progress/scrollbar messages are kWM_USER-based. */
enum : unsigned {
	kTBM_GETPOS      = kWM_USER + 0,
	kTBM_GETRANGEMIN = kWM_USER + 1,
	kTBM_GETRANGEMAX = kWM_USER + 2,
	kTBM_SETPOS      = kWM_USER + 5,
	kTBM_SETRANGE    = kWM_USER + 6,
	kTBM_SETRANGEMIN = kWM_USER + 7,
	kTBM_SETRANGEMAX = kWM_USER + 8,
	kTBM_SETLINESIZE = kWM_USER + 23,
	kTBM_GETLINESIZE = kWM_USER + 24,
	kTBM_SETTHUMBLEN = kWM_USER + 26,
	kTBM_SETTICFREQ  = kWM_USER + 20,
	kTBM_SETPAGESIZE = kWM_USER + 21,
	kTBM_GETPAGESIZE = kWM_USER + 22,
};

enum : unsigned {
	kPBM_SETRANGE = kWM_USER + 1,
	kPBM_SETPOS   = kWM_USER + 2,
	kPBM_DELTAPOS = kWM_USER + 3,
	kPBM_SETSTEP  = kWM_USER + 4,
	kPBM_STEPIT   = kWM_USER + 5,
};

enum : unsigned {
	kSBM_SETPOS = 0x0404,
	kSBM_GETPOS = 0x0406,
};

enum : unsigned {
	kDWLP_MSGRESULT = 0,
};

enum : int {
	kGWL_WNDPROC     = -4,
	kGWL_HINSTANCE   = -6,
	kGWL_HWNDPARENT  = -8,
	kGWL_STYLE       = -16,
	kGWL_EXSTYLE     = -20,
	kGWL_ID          = -12,
	kGWL_USERDATA    = -21,
	/* The two dialog-private slots. Their values must mirror the engine's
	** DWLP_DLGPROC / DWLP_USER macros, which are computed from the size of
	** LRESULT/LPARAM -- 8 and 16 on a 64-bit build (== sizeof(void*)). */
	kDWLP_DLGPROC    = (int)sizeof(void*),
	kDWLP_USER       = (int)(2 * sizeof(void*)),
};

enum : unsigned {
	kGW_HWNDFIRST = 0,
	kGW_HWNDLAST  = 1,
	kGW_HWNDNEXT  = 2,
	kGW_HWNDPREV  = 3,
	kGW_CHILD     = 5,
};

/* WM_GETDLGCODE answers: which keys this control wants sent to it rather
** than used for dialog navigation. */
enum : unsigned {
	kDLGC_WANTARROWS = 0x0001,
	kDLGC_WANTTAB    = 0x0002,
	kDLGC_WANTALLKEYS= 0x0004,
	kDLGC_HASSETSEL  = 0x0008,
	kDLGC_WANTCHARS  = 0x0080,
	kDLGC_STATIC     = 0x0100,
	kDLGC_BUTTON     = 0x2000,
};

enum : unsigned long {
	kWS_CHILD    = 0x40000000UL,
	kWS_VISIBLE  = 0x10000000UL,
	kWS_DISABLED = 0x08000000UL,
	kWS_TABSTOP  = 0x00010000UL,
	kWS_GROUP    = 0x00020000UL,
	kDS_SETFONT  = 0x00000040UL,
};

enum : unsigned {
	kVK_TAB    = 0x09,
	kVK_RETURN = 0x0D,
	kVK_ESCAPE = 0x1B,
	kVK_SPACE  = 0x20,
	kVK_PRIOR  = 0x21,
	kVK_NEXT   = 0x22,
	kVK_UP     = 0x26,
	kVK_DOWN   = 0x28,
	kVK_BACK   = 0x08,
	kVK_DELETE = 0x2E,
	kVK_LEFT   = 0x25,
	kVK_RIGHT  = 0x27,
	kVK_HOME   = 0x24,
	kVK_END    = 0x23,
};

/* Edit-control messages the engine sends to read back selection and replace
** text. The shim defines them for the engine's translation units, but this
** file is compiled without it, so the stable values are restated here. */
enum : unsigned {
	kWM_SYSCHAR      = 0x0106,
	kEM_GETSEL       = 0x00B0,
	kEM_SETSEL       = 0x00B1,
	kEM_REPLACESEL   = 0x00C2,
};

enum : unsigned {
	kIDOK     = 1,
	kIDCANCEL = 2,
};

/* The class ordinals a DLGTEMPLATE uses in place of a name string. */
char const *const kAtomNames[6] = {
	"Button", "Edit", "Static", "ListBox", "ScrollBar", "ComboBox",
};

typedef intptr_t (*WinProc)(void *hwnd, unsigned message, uintptr_t wparam, intptr_t lparam);

/* ------------------------------------------------------------------ */

/* A registered class and the procedure its windows are created with. */
struct ClassProc {
	std::string name;
	WinProc     proc = nullptr;
};

std::vector<ClassProc> g_ClassProcs;


struct Win {
	std::string cls;
	std::string text;
	unsigned long style = 0;
	unsigned long ex_style = 0;
	int x = 0, y = 0, w = 0, h = 0;
	Win *parent = nullptr;
	/* The handle the parent was created with, even when that parent is the
	** native game window and therefore has no Win node. */
	OpentsWin parent_handle = nullptr;
	std::vector<Win *> children;
	unsigned id = 0;
	std::vector<uint8_t> creation;
	WinProc proc = nullptr;         /* what messages go to right now */
	WinProc prev_proc = nullptr;    /* what kGWL_WNDPROC used to be   */
	bool is_dialog = false;
	bool visible = true;
	bool dirty = false;             /* has something to paint */
	bool enabled = true;
	bool pressed = false;           /* a push button holding the mouse */
	intptr_t dlg_result = 0;
	uintptr_t user = 0;             /* kGWL_USERDATA */

	/* Control state, kept because the engine reads it back by message. */
	std::vector<std::string> items;
	std::vector<uintptr_t> item_data;
	int cursel = -1;
	int check = 0;
	int scroll_pos = 0;
	int tb_pos = 0;
	int tb_min = 0;
	int tb_max = 100;
	int tb_pagesize = 20;
	int tb_linesize = 1;
	int progress = 0;
	bool dropdown = false;
	int top_index = 0;

	/* Edit box caret / selection. Win32's real EDIT control keeps this and
	** inserts typed text itself; the port has no real control, so the logical
	** window must hold it and Default_Control_Proc does the editing. */
	int edit_sel_start = 0;
	int edit_sel_end = 0;
};

std::vector<Win *> g_Windows;      /* every live logical window          */
std::vector<Win *> g_MainChildren; /* top-level windows owned by the native one */
OpentsWin g_MainWindow = nullptr;  /* the native game window's handle    */
std::string g_MainClassName;       /* the class it was registered under  */
OpentsWin g_Focus = nullptr;


Win *As_Win(OpentsWin window)
{
	if (window == nullptr || window == g_MainWindow) {
		return nullptr;
	}
	for (Win * w : g_Windows) {
		if ((OpentsWin)w == window) {
			return w;
		}
	}
	return nullptr;
}

/*
** The child list that hangs off a handle.
**
** The native game window is deliberately not a Win node, so a dialog created
** against it cannot be registered through its parent pointer. It still has to
** belong to a list, though: with the owner dropped, a dialog had no parent and
** appeared in no child list, so GetTopWindow(MainWindow) answered NULL, the
** mouse hit test concluded that no control ever owned a click, and every click
** inside a dialog was delivered to the main window -- whose procedure has no
** mouse cases -- and thrown away. Dialog controls were simply not clickable.
*/
std::vector<Win *> *Child_List_Of(OpentsWin window)
{
	if (Win * w = As_Win(window)) {
		return &w->children;
	}
	if (window != nullptr && window == g_MainWindow) {
		return &g_MainChildren;
	}
	return nullptr;
}

/* UTF-16 (resource encoding) -> the engine's char strings, which are
** Windows-1252 by contract (GetACP() == 1252 in the shim). Characters
** above the 1252 repertoire degrade to '?' rather than vanishing. */
std::string Utf16_To_1252(unsigned short const *text, unsigned chars)
{
	static bool inited = false;
	static unsigned char high[32];
	if (!inited) {
		/* Windows-1252's 0x80..0x9F, which the engine's own
		** Windows_1252_High table also encodes (utf8.cpp). */
		static unsigned char const table[32] = {
			0x80, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
			0x89, 0x8A, 0x8B, 0x8C, 0x8E, 0x91, 0x92, 0x93,
			0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B,
			0x9C, 0x9E, 0x9F, 0x00, 0x00, 0x00, 0x00, 0x00,
		};
		memcpy(high, table, sizeof(table));
		inited = true;
	}

	std::string out;
	out.reserve(chars);
	for (unsigned i = 0; i < chars; i++) {
		unsigned short c = text[i];
		if (c < 0x80) {
			out += (char)c;
		} else if (c >= 0x80 && c <= 0x9F) {
			unsigned char mapped = high[c - 0x80];
			out += mapped ? (char)mapped : '?';
		} else if (c >= 0xA0 && c <= 0xFF) {
			out += (char)c;
		} else {
			out += '?';
		}
	}
	return out;
}

/* ------------------------------------------------------------------ */
/* The default procedure a logical control gets before it is           */
/* subclassed. It implements the control-message behaviour the engine  */
/* relies on -- which on Win32 is what the built-in classes do.        */
/* ------------------------------------------------------------------ */

Win *Parent_Of(Win * w)
{
	return w->parent;
}

void Notify_Parent(Win * w, unsigned code)
{
	if (w->parent == nullptr) {
		return;
	}
	Win_Diag("notify: cls=%s id=%lu code=%u", w->cls.c_str(), (unsigned long)w->id, code);
	/* kWM_COMMAND: MAKEWPARAM(id, code), lparam = control handle. */
	opents_win_send((OpentsWin)w->parent, kWM_COMMAND,
	                ((uintptr_t)w->id & 0xFFFF) | ((uintptr_t)(code & 0xFFFF) << 16),
	                (intptr_t)(OpentsWin)w);
}

intptr_t Default_Control_Proc(void *hwnd, unsigned message, uintptr_t wparam, intptr_t lparam)
{
	Win * w = As_Win((OpentsWin)hwnd);
	if (w == nullptr) {
		return 0;
	}
	if (message == 0x014F /* CB_SHOWDROPDOWN */) {
		Win_Diag("defproc CB_SHOWDROPDOWN: cls=%s id=%lu",
			w->cls.c_str(), (unsigned long)w->id);
	}

	/*
	** Trackbar, progress and scrollbar messages are dispatched on the class
	** before the general switch below.
	**
	** All three families live in the WM_USER+n space and deliberately
	** overlap -- TBM_GETRANGEMIN and PBM_SETRANGE are both 0x0401 -- and
	** Win32 tells them apart only by which class received them. A single
	** switch therefore cannot hold all three; it would be a duplicate-case
	** error, and resolving it by dropping one family would make that
	** control silently inert.
	*/
	if (strcasecmp(w->cls.c_str(), "msctls_trackbar32") == 0) {
		if (message == kWM_LBUTTONDOWN || message == kWM_LBUTTONUP) {
			Win_Diag("trackbar mouse: id=%lu msg=0x%X pos=(%d,%d) range=(%d..%d) pos=%d",
				(unsigned long)w->id, message,
				(int)(short)LOWORD(lparam), (int)(short)HIWORD(lparam),
				w->tb_min, w->tb_max, w->tb_pos);
		}
		if (message == kWM_COMMAND) {
			Win_Diag("trackbar command: id=%lu wparam=0x%zx", (unsigned long)w->id, wparam);
		}
		switch (message) {
		case kTBM_GETPOS:     return (intptr_t)w->tb_pos;
		case kTBM_SETPOS:
			w->tb_pos = (int)(intptr_t)lparam;
			return 0;
		case kTBM_SETRANGE:
			w->tb_min = (int)(short)(wparam & 0xFFFF);
			w->tb_max = (int)(short)((wparam >> 16) & 0xFFFF);
			return 0;
		case kTBM_SETRANGEMIN:
			w->tb_min = (int)(intptr_t)lparam;
			return 0;
		case kTBM_SETRANGEMAX:
			w->tb_max = (int)(intptr_t)lparam;
			return 0;
		case kTBM_GETRANGEMIN: return (intptr_t)w->tb_min;
		case kTBM_GETRANGEMAX: return (intptr_t)w->tb_max;
		case kTBM_SETPAGESIZE:
			w->tb_pagesize = (int)(intptr_t)lparam;
			return (intptr_t)w->tb_pagesize;
		case kTBM_GETPAGESIZE: return (intptr_t)w->tb_pagesize;
		case kTBM_SETLINESIZE:
			w->tb_linesize = (int)(intptr_t)lparam;
			return (intptr_t)w->tb_linesize;
		case kTBM_GETLINESIZE: return (intptr_t)w->tb_linesize;
		default: break;
		}
	}
	if (strcasecmp(w->cls.c_str(), "msctls_progress32") == 0) {
		switch (message) {
		case kPBM_SETRANGE: return 0;
		case kPBM_SETPOS:
			w->progress = (int)wparam;
			return (intptr_t)w->progress;
		case kPBM_DELTAPOS:
			w->progress += (int)wparam;
			return (intptr_t)w->progress;
		case kPBM_SETSTEP:  return 0;
		case kPBM_STEPIT:   return (intptr_t)w->progress;
		default: break;
		}
	}
	if (strcasecmp(w->cls.c_str(), "ScrollBar") == 0) {
		switch (message) {
		case kSBM_SETPOS:
			w->scroll_pos = (int)wparam;
			return (intptr_t)w->scroll_pos;
		case kSBM_GETPOS: return (intptr_t)w->scroll_pos;
		default: break;
		}
	}

	/*
	** Push buttons.
	**
	** On Win32 the button is a built-in class that takes the press, keeps the
	** mouse while it is held, and -- if the release comes back inside it --
	** tells its parent it was clicked. That notification is the WM_COMMAND a
	** dialog's OK is driven by. Nothing in the port did any of it, so every
	** button in the game drew correctly and did nothing when pressed: the
	** campaign dialog's OK could not commit a campaign and the player could
	** not get past it.
	**
	** Only the push styles are handled here. A check box or radio button is
	** the engine's own owner-draw procedure's business -- it already claims
	** the press and reports the new state -- and a group box takes nothing.
	*/
	if (strcasecmp(w->cls.c_str(), "Button") == 0) {
		const unsigned long kind = w->style & 0x0F;
		const bool is_push = (kind == 0x0 || kind == 0x1 || kind == 0xA || kind == 0xB);
		if (is_push && (w->style & kWS_DISABLED) == 0) {
			switch (message) {
			case kWM_LBUTTONDOWN:
				/* Holding the mouse is what makes the release land here even
				** if the pointer has wandered off the button. */
				opents_input_set_capture((void *)hwnd);
				w->pressed = true;
				w->dirty = true;
				return 0;

			case kWM_LBUTTONUP: {
				const bool was_pressed = w->pressed;
				w->pressed = false;
				w->dirty = true;
				if (opents_input_get_capture() == (OpentsWin)hwnd) {
					opents_input_set_capture(nullptr);
				}
				if (was_pressed) {
					const int x = (int)(short)(lparam & 0xFFFF);
					const int y = (int)(short)((lparam >> 16) & 0xFFFF);
					int cw = 0;
					int ch = 0;
					opents_win_get_client_size((OpentsWin)w, &cw, &ch);
					/* Released outside the button: the click is abandoned,
					** exactly as Win32 abandons it. */
					if (x >= 0 && y >= 0 && x < cw && y < ch) {
						Notify_Parent(w, 0 /* BN_CLICKED */);
					}
				}
				return 0;
			}

			default:
				break;
			}
		}
	}

	/*
	** Edit boxes.
	**
	** The engine's owner-draw layer subclasses a real Win32 EDIT control and
	** lets CallWindowProc do the typing, so its own procedure only bothers with
	** painting, tab and Return. The port has no real control -- the "previous
	** procedure" its owner-draw handler reaches is this one -- so the
	** insertion, the caret and the selection have to live here, or nothing the
	** player types into the skirmish Name field ever appears.
	*/
	if (strcasecmp(w->cls.c_str(), "Edit") == 0) {
		switch (message) {
		case kWM_LBUTTONDOWN:
			/* Win32's EDIT takes the focus when pressed. Without this the
			** engine's edit never becomes the keyboard target, so its
			** OD_ACTIVATE deferral can never resolve to a focused control. */
			Win_Diag("edit click: id=%lu text='%s' rect=(%d,%d,%d,%d)",
				(unsigned long)w->id, w->text.c_str(), w->x, w->y, w->w, w->h);
			/* Win32 also parks the caret at the clicked character. The click
			** coordinates arrive in viewport space and the glyph metrics live
			** in the engine's font layer, so the exact index is not cheap to
			** reconstruct here -- park the caret at the end of the text
			** instead, which is the common intent when clicking a name field.
			** Leaving it untouched put it at index 0 and every keystroke
			** prepended. */
			w->edit_sel_start = w->edit_sel_end = (int)w->text.size();
			opents_win_set_focus((OpentsWin)w);
			break;

		case kWM_CHAR: {
			/* Control characters (Tab, Return, Backspace) are the dialog
			** manager's business and are handled on their WM_KEYDOWN. */
			if (wparam < 0x20 || wparam == 0x7F) {
				break;
			}
			Win_Diag("edit char: id=%lu char=0x%02X before='%s'",
				(unsigned long)w->id, (unsigned)wparam, w->text.c_str());
			int s = w->edit_sel_start;
			int e = w->edit_sel_end;
			if (s > e) { int t = s; s = e; e = t; }
			w->text.replace((size_t)s, (size_t)(e - s), 1, (char)(unsigned char)wparam);
			w->edit_sel_start = w->edit_sel_end = s + 1;
			w->dirty = true;
			return 0;
		}

		case kWM_KEYDOWN: {
			int s = w->edit_sel_start;
			int e = w->edit_sel_end;
			if (s > e) { int t = s; s = e; e = t; }
			const int len = (int)w->text.size();
			switch (wparam) {
			case kVK_BACK:
				if (s == e) {
					if (s == 0) { return 0; }
					s--;
				}
				w->text.erase((size_t)s, (size_t)(e - s));
				w->edit_sel_start = w->edit_sel_end = s;
				w->dirty = true;
				return 0;
			case kVK_DELETE:
				if (s == e) {
					if (e >= len) { return 0; }
					e++;
				}
				w->text.erase((size_t)s, (size_t)(e - s));
				w->edit_sel_start = w->edit_sel_end = s;
				w->dirty = true;
				return 0;
			case kVK_LEFT:
				w->edit_sel_end = w->edit_sel_start = (s > 0 ? s - 1 : 0);
				return 0;
			case kVK_RIGHT:
				w->edit_sel_end = w->edit_sel_start = (e < len ? e + 1 : len);
				return 0;
			case kVK_HOME:
				w->edit_sel_start = w->edit_sel_end = 0;
				return 0;
			case kVK_END:
				w->edit_sel_start = w->edit_sel_end = len;
				return 0;
			default:
				break;
			}
			break;
		}

		/* EM_GETSEL: low word start, high word end, and the two pointers
		** carry the same pair -- which is what the engine's paint reads. */
		case kEM_GETSEL:
			if (wparam != 0) { *(int *)wparam = w->edit_sel_start; }
			if (lparam != 0) { *(int *)lparam = w->edit_sel_end; }
			return (intptr_t)(((unsigned)w->edit_sel_start & 0xFFFF) |
			                  (((unsigned)w->edit_sel_end & 0xFFFF) << 16));

		case kEM_SETSEL: {
			int s = (int)(intptr_t)wparam;
			int e = (int)(intptr_t)lparam;
			const int len = (int)w->text.size();
			if (s < 0) { s = 0; }
			if (e < 0) { e = len; }
			if (s > len) { s = len; }
			if (e > len) { e = len; }
			w->edit_sel_start = s;
			w->edit_sel_end = e;
			return 0;
		}

		case kEM_REPLACESEL: {
			int s = w->edit_sel_start;
			int e = w->edit_sel_end;
			if (s > e) { int t = s; s = e; e = t; }
			const char * rep = (lparam != 0) ? (const char *)lparam : "";
			w->text.replace((size_t)s, (size_t)(e - s), rep);
			w->edit_sel_start = w->edit_sel_end = s + (int)strlen(rep);
			w->dirty = true;
			return 0;
		}

		default:
			break;
		}
	}

	switch (message) {
	/*
	** Hit testing.
	**
	** The controls a player is not meant to press have to say so, or they
	** intercept the clicks aimed at whatever they decorate. Win32's group box
	** is defined to answer HTTRANSPARENT, and so is a static that has not asked
	** for SS_NOTIFY -- which is what lets a press reach a check box that a group
	** box frames or a label that overlaps it. The engine's own hit test
	** (`Child_From_Logical_Point`) is written for exactly that answer, but
	** nothing here ever gave it: a group box returned the default zero, so the
	** walk stopped at the frame. Because the engine declares those boxes late in
	** the template -- and a dialog gives the earlier control the higher z-order
	** -- the box sits on top of every control that follows it, so the option
	** check boxes and the game speed slider of the skirmish dialog were painted
	** and unreachable.
	*/
	case kWM_NCHITTEST:
		if (strcasecmp(w->cls.c_str(), "Button") == 0) {
			/* BS_GROUPBOX. */
			if ((w->style & 0x0F) == 0x7) {
				return HTTRANSPARENT;
			}
			return HTCLIENT;
		}
		if (strcasecmp(w->cls.c_str(), "Static") == 0) {
			/* SS_NOTIFY asks for the mouse; anything else lets it through. */
			return ((w->style & 0x0100) == 0) ? HTTRANSPARENT : HTCLIENT;
		}
		if ((w->style & kWS_DISABLED) != 0) {
			return HTTRANSPARENT;
		}
		return HTCLIENT;

	/* Text, for every class. */
	case kWM_GETTEXTLENGTH:
		return (intptr_t)w->text.size();
	case kWM_GETTEXT: {
		unsigned size = (unsigned)wparam;
		if (size == 0 || lparam == 0) {
			return 0;
		}
		unsigned n = (unsigned)w->text.size();
		if (n >= size) {
			n = size - 1;
		}
		memcpy((void *)lparam, w->text.c_str(), n);
		((char *)lparam)[n] = '\0';
		return (intptr_t)n;
	}
	case kWM_SETTEXT:
		w->text = lparam ? (char const *)lparam : "";
		return 1;

	case kWM_ENABLE:
		w->enabled = (wparam != 0);
		return 0;
	case kWM_SHOWWINDOW:
		w->visible = (wparam != 0);
		return 0;

	/* Buttons. Checking posts the notification a real Button would. */
	case kBM_GETCHECK:
		return (intptr_t)w->check;
	case kBM_SETCHECK:
		w->check = (int)wparam;
		return 0;
	case kBM_GETSTATE:
		return (intptr_t)(w->check ? 0x0004 : 0); /* BST_CHECKED */
	case kBM_SETSTATE:
		if (wparam) {
			Notify_Parent(w, 0 /* BN_CLICKED */);
		}
		return 0;

	/* List box. */
	case kLB_ADDSTRING:
	case kCB_ADDSTRING:
		if (lparam == 0) {
			return (intptr_t)-1;
		}
		w->items.push_back((char const *)lparam);
		w->item_data.push_back(0);
		return (intptr_t)(w->items.size() - 1);
	case kLB_INSERTSTRING:
	case kCB_INSERTSTRING: {
		uintptr_t at = wparam;
		if (at > w->items.size()) {
			at = w->items.size();
		}
		w->items.insert(w->items.begin() + at, lparam ? (char const *)lparam : "");
		w->item_data.insert(w->item_data.begin() + at, 0);
		return (intptr_t)at;
	}
	case kLB_DELETESTRING:
	case kCB_DELETESTRING: {
		uintptr_t at = wparam;
		if (at >= w->items.size()) {
			return (intptr_t)-1;
		}
		w->items.erase(w->items.begin() + at);
		w->item_data.erase(w->item_data.begin() + at);
		if (w->cursel >= (int)w->items.size()) {
			w->cursel = (int)w->items.size() - 1;
		}
		return (intptr_t)w->items.size();
	}
	case kLB_RESETCONTENT:
	case kCB_RESETCONTENT:
		w->items.clear();
		w->item_data.clear();
		w->cursel = -1;
		w->top_index = 0;
		return 0;
	case kLB_SETCURSEL:
	case kCB_SETCURSEL:
		if ((intptr_t)wparam >= (intptr_t)w->items.size()) {
			return (intptr_t)-1;
		}
		w->cursel = (int)wparam;
		return (intptr_t)w->cursel;
	case kLB_GETCURSEL:
	case kCB_GETCURSEL:
		return (intptr_t)w->cursel;
	case kLB_GETCOUNT:
	case kCB_GETCOUNT:
		return (intptr_t)w->items.size();
	case kLB_GETTEXTLEN:
	case kCB_GETLBTEXTLEN:
		if ((int)wparam >= (int)w->items.size()) {
			return (intptr_t)-1;
		}
		return (intptr_t)w->items[wparam].size();
	case kLB_GETTEXT:
	case kCB_GETLBTEXT:
		if ((int)wparam >= (int)w->items.size() || lparam == 0) {
			return (intptr_t)-1;
		}
		memcpy((void *)lparam, w->items[wparam].c_str(), w->items[wparam].size() + 1);
		return (intptr_t)w->items[wparam].size();
	case kLB_GETITEMDATA:
	case kCB_GETITEMDATA:
		if ((int)wparam >= (int)w->items.size()) {
			return (intptr_t)-1;
		}
		return (intptr_t)w->item_data[wparam];
	case kLB_SETITEMDATA:
	case kCB_SETITEMDATA:
		if ((int)wparam >= (int)w->items.size()) {
			return (intptr_t)-1;
		}
		w->item_data[wparam] = (uintptr_t)lparam;
		return (intptr_t)1;
	case kLB_FINDSTRING:
	case kCB_FINDSTRING: {
		/* wparam is the index to start after; match on prefix. */
		char const *prefix = (char const *)lparam;
		if (prefix == nullptr) {
			return (intptr_t)-1;
		}
		unsigned n = (unsigned)w->items.size();
		for (unsigned step = 0; step < n; step++) {
			unsigned at = ((unsigned)wparam + 1 + step) % n;
			if (strncasecmp(w->items[at].c_str(), prefix, strlen(prefix)) == 0) {
				return (intptr_t)at;
			}
		}
		return (intptr_t)-1;
	}
	case kLB_GETTOPINDEX:
	case kCB_GETTOPINDEX:
		return (intptr_t)w->top_index;
	case kLB_SETTOPINDEX:
	case kCB_SETTOPINDEX:
		w->top_index = (int)wparam;
		return 0;
	case kLB_SETCOLUMNWIDTH:
		return 0;
	case kCB_GETITEMHEIGHT:
		return 14;
	case kCB_SHOWDROPDOWN:
		w->dropdown = (wparam != 0);
		Win_Diag("combo dropdown: id=%lu -> %d (items=%d)",
			(unsigned long)w->id, (int)w->dropdown, (int)w->items.size());
		return 1;

	/* The dialog manager asks controls what keys they want. */
	case kWM_GETDLGCODE: {
		uintptr_t vk = wparam & 0xFF;
		if (w->cls == "Edit") {
			return kDLGC_WANTCHARS | kDLGC_WANTARROWS;
		}
		if (vk == kVK_TAB) {
			return 0;
		}
		return kDLGC_WANTARROWS;
	}

	default:
		break;
	}
	return 0;
}

/* The default procedure for dialog frames that the engine did not give
** a procedure for, and for messages a dialog proc chose not to handle. */
intptr_t Default_Dialog_Frame_Proc(void *hwnd, unsigned message, uintptr_t wparam, intptr_t lparam)
{
	Win * w = As_Win((OpentsWin)hwnd);
	if (w == nullptr) {
		return 0;
	}
	if (message == 0x014F /* CB_SHOWDROPDOWN */) {
		Win_Diag("defproc CB_SHOWDROPDOWN: cls=%s id=%lu",
			w->cls.c_str(), (unsigned long)w->id);
	}
	switch (message) {
	case kWM_GETTEXTLENGTH:
	case kWM_GETTEXT:
	case kWM_SETTEXT:
		return Default_Control_Proc(hwnd, message, wparam, lparam);
	case kWM_GETDLGCODE:
		return kDLGC_WANTARROWS;
	default:
		return 0;
	}
}

intptr_t Dispatch_To_Proc(Win * w, unsigned message, uintptr_t wparam, intptr_t lparam)
{
	if (w->proc != nullptr) {
		return w->proc((OpentsWin)w, message, wparam, lparam);
	}
	if (w->is_dialog) {
		return Default_Dialog_Frame_Proc((OpentsWin)w, message, wparam, lparam);
	}
	return Default_Control_Proc((OpentsWin)w, message, wparam, lparam);
}

/* ------------------------------------------------------------------ */
/* DLGTEMPLATE parsing. Both the classic form and DLGTEMPLATEEX are        */
/* handled: an earlier note here claimed the EX form never occurs, but six  */
/* of the game's dialogs are declared DIALOGEX, and reading one as a        */
/* classic template gives a frame with a garbage size.                      */
/* ------------------------------------------------------------------ */

unsigned short Read_U16(uint8_t const *p) { unsigned short v; memcpy(&v, p, 2); return v; }
unsigned Read_U32(uint8_t const *p) { unsigned v; memcpy(&v, p, 4); return v; }

unsigned Align2(unsigned at) { return (at + 1) & ~1u; }
unsigned Align4(unsigned at) { return (at + 3) & ~3u; }

/* A template field that is either a null-terminated UTF-16 string or
** 0x0000 (absent) / 0xFFFF + ordinal. Returns the decoded text and the
** offset past the field. */
std::string Read_Name_Or_Ordinal(uint8_t const *blob, unsigned size, unsigned &at, unsigned *ordinal)
{
	*ordinal = 0;
	if (at + 2 > size) {
		return std::string();
	}
	unsigned short first = Read_U16(blob + at);
	if (first == 0x0000) {
		at += 2;
		return std::string();
	}
	if (first == 0xFFFF) {
		if (at + 4 > size) {
			at = size;
			return std::string();
		}
		*ordinal = Read_U16(blob + at + 2);
		at += 4;
		return std::string();
	}
	unsigned start = at;
	while (at + 2 <= size && Read_U16(blob + at) != 0) {
		at += 2;
	}
	unsigned chars = (at - start) / 2;
	at = Align2(at + 2);
	return Utf16_To_1252((unsigned short const *)(blob + start), chars);
}

} // namespace

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

/*
** Diagnostic: a low-volume event log for the dialog layer -- focus changes,
** clicks landing on a control, characters inserted -- so the click/focus/typing
** chain can be traced. Lives at global scope so engine-side code can report
** through it too.
**
** Writes to /tmp/win_diag.txt, and is OFF unless OPENTS_DIALOG_DIAG is set in
** the environment. It has to be: the call sites sit in the message pump, so
** leaving it on would open a file and fflush on every message in a normal
** session, and write to /tmp on a machine that never asked for a log.
*/
static FILE * g_WinDiag = nullptr;
static bool g_WinDiagEnabled = false;

void Win_Diag(char const * fmt, ...)
{
	if (!g_WinDiagEnabled) {
		// Read once, on the first call, rather than on every message.
		if (getenv("OPENTS_DIALOG_DIAG") == nullptr) {
			return;
		}
		g_WinDiagEnabled = true;
	}

	if (g_WinDiag == nullptr) {
		g_WinDiag = fopen("/tmp/win_diag.txt", "w");
	}
	if (g_WinDiag != nullptr) {
		va_list args;
		va_start(args, fmt);
		vfprintf(g_WinDiag, fmt, args);
		va_end(args);
		fputc('\n', g_WinDiag);
		fflush(g_WinDiag);
	}
}

/*
**  The default procedure, in a form the engine can hold and call back.
**
**  Win32 hands the class's own procedure to whoever asks for kGWL_WNDPROC
**  on an unsubclassed window, and control procedures call that for every
**  message they do not handle themselves. Controls here start with no
**  procedure of their own, so this is the equivalent answer: it reaches
**  whichever built-in default the window kind wants.
*/
extern "C" intptr_t opents_win_def_proc(void *hwnd, unsigned message, uintptr_t wparam, intptr_t lparam)
{
	Win * w = As_Win(hwnd);
	if (w == nullptr) {
		return 0;
	}
	if (w->is_dialog) {
		return Default_Dialog_Frame_Proc(hwnd, message, wparam, lparam);
	}
	return Default_Control_Proc(hwnd, message, wparam, lparam);
}

extern "C" OpentsWin opents_win_create(char const *class_name, char const *text,
                                       unsigned long style, unsigned long ex_style,
                                       int x, int y, int width, int height,
                                       OpentsWin parent, unsigned id,
                                       void const *creation_data, unsigned creation_size)
{
	Win * w = new Win;
	w->cls = class_name ? class_name : "";
	w->text = text ? text : "";
	w->style = style;
	w->ex_style = ex_style;
	w->x = x;
	w->y = y;
	w->w = width;
	w->h = height;
	w->id = id;
	w->visible = (style & kWS_VISIBLE) != 0;
	w->enabled = (style & kWS_DISABLED) == 0;

	if (creation_data != nullptr && creation_size != 0) {
		w->creation.assign((uint8_t const *)creation_data, (uint8_t const *)creation_data + creation_size);
	}

	w->parent = As_Win(parent);
	w->parent_handle = (w->parent != nullptr) ? (OpentsWin)w->parent : parent;
	if (std::vector<Win *> *owner_children = Child_List_Of(parent)) {
		owner_children->push_back(w);
	}

	g_Windows.push_back(w);
	return (OpentsWin)w;
}

extern "C" void opents_win_destroy(OpentsWin window)
{
	Win * w = As_Win(window);
	if (w == nullptr) {
		return;
	}

	/* Children go first, deepest last, so parent pointers stay valid. */
	while (!w->children.empty()) {
		opents_win_destroy((OpentsWin)w->children.back());
	}

	/*
	**  Win32's DestroyWindow notifies a window as it goes -- WM_DESTROY first,
	**  then WM_NCDESTROY -- and the engine's owner-draw dialog layer hangs real
	**  bookkeeping off both: Default_Dialog_Proc drops the frame from the
	**  modeless-dialog list and decrements _dialog_count (the game-screen
	**  repaint and the sidebar both gate on that count), CtrlProc_Internal
	**  prunes the window from its modal stack, and On_WM_NCDESTROY frees the
	**  cached background and takes the window out of the owner-draw
	**  dictionaries. Tearing the window out of the lists and freeing it with no
	**  notification left _dialog_count stuck above zero, so once any dialog had
	**  been shown the game never repainted after it closed and the previous
	**  dialog stayed on the screen. Dispatch before the handle is forgotten so
	**  the procedure can still look itself up.
	*/
	Dispatch_To_Proc(w, kWM_DESTROY, 0, 0);
	Dispatch_To_Proc(w, kWM_NCDESTROY, 0, 0);

	if (g_Focus == window) {
		g_Focus = nullptr;
	}

	for (size_t i = 0; i < g_Windows.size(); i++) {
		if (g_Windows[i] == w) {
			g_Windows.erase(g_Windows.begin() + i);
			break;
		}
	}
	if (std::vector<Win *> *owner_children = Child_List_Of(w->parent_handle)) {
		for (size_t i = 0; i < owner_children->size(); i++) {
			if ((*owner_children)[i] == w) {
				owner_children->erase(owner_children->begin() + i);
				break;
			}
		}
	}
	delete w;
}

/*
**  The height a control's window actually gets, in pixels.
**
**  A COMBOBOX's template height is the height of its dropped list, not of the
**  control. Win32 opens the combo at its closed height -- one item plus the
**  frame -- and keeps the template height for the list it drops. The port
**  handed the template height straight to the window, so every combo carried an
**  invisible box as much as 120 pixels tall below it, and anything the dialog
**  put under a combo became unreachable: the skirmish dialog's colour combo
**  sits directly beneath its side combo, so the side combo's box contained the
**  colour combo's arrow and swallowed every press meant for it. The closed
**  height below is the one the combo's own paint draws.
*/
static int Control_Height(std::string const & cls, int units)
{
	if (cls == "ComboBox") {
		return 24;
	}
	return opents_dialog_units_y(units);
}


extern "C" OpentsWin opents_win_create_dialog(void const *template_bytes, unsigned template_size,
                                              OpentsWin owner, void *dlg_proc)
{
	uint8_t const *blob = (uint8_t const *)template_bytes;
	if (blob == nullptr || template_size < 18) {
		return nullptr;
	}

	/*
	**  The template arrives in one of two shapes, and the difference is not
	**  cosmetic.
	**
	**  The classic DLGTEMPLATE opens with the window style. DLGTEMPLATEEX --
	**  which is what a .rc asks for with DIALOGEX -- opens with a version word
	**  and the 0xFFFF signature, and then carries a help id before that style,
	**  a 32-bit item id per item instead of 16, and a point size *and* a weight
	**  in its font block. Six of the game's dialogs are DIALOGEX (keyboard
	**  options, dropship limits, the two multiplayer connect screens, the wait
	**  progress bar and the multiplayer map selector).
	**
	**  Read as a classic template, the version word became the low half of the
	**  style and the signature the high half, and every later field slid: the map
	**  selector's size came out as 24576 by 9 pixels instead of 540 by 325, so
	**  its frame was composed onto a surface 24576 wide and its edge glow walked
	**  off the end of that buffer.
	*/
	bool const is_ex = (Read_U16(blob) == 0x0001 && Read_U16(blob + 2) == 0xFFFF);
	if (is_ex && template_size < 26) {
		return nullptr;
	}

	unsigned long style;
	unsigned cdit;
	int x;
	int y;
	int cx;
	int cy;
	unsigned at;

	if (is_ex) {
		style = Read_U32(blob + 12);
		cdit = Read_U16(blob + 16);
		x = (short)Read_U16(blob + 18);
		y = (short)Read_U16(blob + 20);
		cx = (short)Read_U16(blob + 22);
		cy = (short)Read_U16(blob + 24);
		at = 26;
	} else {
		style = Read_U32(blob + 0);
		/* unsigned long ex_style = Read_U32(blob + 4); */
		cdit = Read_U16(blob + 8);
		x = (short)Read_U16(blob + 10);
		y = (short)Read_U16(blob + 12);
		cx = (short)Read_U16(blob + 14);
		cy = (short)Read_U16(blob + 16);
		at = 18;
	}

	unsigned ordinal = 0;
	(void)Read_Name_Or_Ordinal(blob, template_size, at, &ordinal); /* menu  */
	(void)Read_Name_Or_Ordinal(blob, template_size, at, &ordinal); /* class */
	(void)Read_Name_Or_Ordinal(blob, template_size, at, &ordinal); /* title */
	if (style & kDS_SETFONT) {
		/* The point size, then the face name. The extended form puts the
		** weight, the italic flag and the character set in between. */
		unsigned extra = is_ex ? 6 : 2;
		if (at + extra > template_size) {
			return nullptr;
		}
		at += extra;
		(void)Read_Name_Or_Ordinal(blob, template_size, at, &ordinal);
	}

	/* The frame. "Dialog" is the class name the engine's own dialog
	** layer sees from GetClassName, matching Win32's #32770 semantics
	** for everything it tests (which is only "is this a dialog").
	**
	** It is created against its owner, which is the main game window. Win32
	** would not have needed to say so, but this registry does: a dialog that
	** hangs off nothing cannot be found by the hit test, so the player's
	** clicks never reach the controls inside it. */
	Win * frame = (Win *)opents_win_create("Dialog", nullptr, style, 0,
	                                       opents_dialog_units_x(x), opents_dialog_units_y(y),
	                                       opents_dialog_units_x(cx), opents_dialog_units_y(cy),
	                                       owner, 0, nullptr, 0);
	frame->is_dialog = true;
	frame->proc = (WinProc)dlg_proc;

	/* Items, each DWORD-aligned. The extended form leads with a help id and
	** carries the control id as a DWORD; the classic form leads with the style
	** and keeps the id a WORD, so its header is six bytes shorter. */
	unsigned const item_header = is_ex ? 24 : 18;
	for (unsigned i = 0; i < cdit && at + item_header <= template_size; i++) {
		at = Align4(at);
		unsigned long istyle;
		unsigned long iex;
		int ix;
		int iy;
		int icx;
		int icy;
		unsigned iid;
		if (is_ex) {
			iex = Read_U32(blob + at + 4);
			istyle = Read_U32(blob + at + 8);
			ix = (short)Read_U16(blob + at + 12);
			iy = (short)Read_U16(blob + at + 14);
			icx = (short)Read_U16(blob + at + 16);
			icy = (short)Read_U16(blob + at + 18);
			iid = Read_U32(blob + at + 20);
		} else {
			istyle = Read_U32(blob + at);
			iex = Read_U32(blob + at + 4);
			ix = (short)Read_U16(blob + at + 8);
			iy = (short)Read_U16(blob + at + 10);
			icx = (short)Read_U16(blob + at + 12);
			icy = (short)Read_U16(blob + at + 14);
			iid = Read_U16(blob + at + 16);
		}
		at += item_header;

		std::string cls = Read_Name_Or_Ordinal(blob, template_size, at, &ordinal);
		if (cls.empty() && ordinal != 0) {
			unsigned index = ordinal - 0x80;
			cls = (index < 6) ? kAtomNames[index] : "Unknown";
		}
		std::string text = Read_Name_Or_Ordinal(blob, template_size, at, &ordinal);

		unsigned csize = 0;
		if (at + 2 <= template_size) {
			csize = Read_U16(blob + at);
			at += 2;
		}
		std::vector<uint8_t> creation;
		if (csize != 0 && at + csize <= template_size) {
			creation.assign(blob + at, blob + at + csize);
		}
		at += csize;

		Win * child = (Win *)opents_win_create(cls.c_str(), text.c_str(), istyle, iex,
		                                       opents_dialog_units_x(ix), opents_dialog_units_y(iy),
		                                       opents_dialog_units_x(icx), Control_Height(cls, icy),
		                                       (OpentsWin)frame, iid, creation.data(), (unsigned)creation.size());
		(void)child;
	}

	/* kWM_INITDIALOG: wParam is the control to focus (the engine's
	** dialog procs ignore it), lParam is the creation argument. */
	Dispatch_To_Proc(frame, kWM_INITDIALOG, 0, 0);

	return (OpentsWin)frame;
}

extern "C" void opents_win_set_main_window(OpentsWin native_hwnd)
{
	g_MainWindow = native_hwnd;
}

extern "C" OpentsWin opents_win_main_window(void)
{
	return g_MainWindow;
}

extern "C" void opents_win_register_class_name(char const *name)
{
	g_MainClassName = name ? name : "";
}

/*
**  Class procedure registry.
**
**  Win32 keeps the procedure with the class, so two windows of different
**  classes get different procedures for the same message. The engine
**  depends on this: it registers its main window class first and then a
**  second class, "ComboDropWin", for the dropped list of a combo box.
**  Keeping only the most recent registration -- as a single global would --
**  sends every queued message to ComboDropWinCtrlProc, including the ones
**  meant for the main window, and that procedure reads state that only a
**  drop-down window has.
*/
extern "C" void opents_win_register_class(char const *name, void *proc)
{
	if (name == nullptr || proc == nullptr) {
		return;
	}
	for (ClassProc & entry : g_ClassProcs) {
		if (strcasecmp(entry.name.c_str(), name) == 0) {
			entry.proc = (WinProc)proc;
			return;
		}
	}
	g_ClassProcs.push_back(ClassProc{name, (WinProc)proc});
}

/* The procedure for a window, chosen by its class. */
extern "C" void *opents_win_proc_for(OpentsWin window)
{
	char const * cls = nullptr;
	Win * w = As_Win(window);
	if (w != nullptr) {
		cls = w->cls.c_str();
	} else if (window != nullptr && window == g_MainWindow) {
		cls = g_MainClassName.c_str();
	}
	if (cls == nullptr) {
		return nullptr;
	}
	for (ClassProc const & entry : g_ClassProcs) {
		if (strcasecmp(entry.name.c_str(), cls) == 0) {
			return (void *)entry.proc;
		}
	}
	return nullptr;
}

extern "C" int opents_win_class_matches_main(char const *name)
{
	if (g_MainClassName.empty() || name == nullptr) {
		return 0;
	}
	return strcmp(name, g_MainClassName.c_str()) == 0;
}

/* The native window is not in the logical tree, so queries that reach it
** have to be recognised before the tree lookup. The shim uses this to
** route geometry to the real Cocoa window and everything else to the
** class name the engine registered. */
extern "C" int opents_win_is_main(OpentsWin window)
{
	return (window != nullptr && window == g_MainWindow) ? 1 : 0;
}

extern "C" int opents_win_get_main_class(char *buffer, unsigned size)
{
	if (buffer == nullptr || size == 0) {
		return 0;
	}
	unsigned n = (unsigned)g_MainClassName.size();
	if (n >= size) {
		n = size - 1;
	}
	memcpy(buffer, g_MainClassName.c_str(), n);
	buffer[n] = '\0';
	return (int)n;
}

extern "C" OpentsWin opents_win_get_parent(OpentsWin window)
{
	Win * w = As_Win(window);
	if (w == nullptr) {
		return nullptr;
	}
	/* A window owned by the native game window keeps no Win parent, but its
	** owner is still the answer -- the geometry code anchors a dialog's
	** parent chain on it. */
	return (w->parent != nullptr) ? (OpentsWin)w->parent : w->parent_handle;
}

extern "C" OpentsWin opents_win_find_child(OpentsWin window, unsigned id)
{
	std::vector<Win *> *kids = Child_List_Of(window);
	if (kids == nullptr) {
		return nullptr;
	}
		for (Win * c : *kids) {
		if (c->id == id) {
			return (OpentsWin)c;
		}
	}
	return nullptr;
}

extern "C" int opents_win_get_class_name(OpentsWin window, char *buffer, unsigned size)
{
	Win * w = As_Win(window);
	if (w == nullptr || buffer == nullptr || size == 0) {
		return 0;
	}
	unsigned n = (unsigned)w->cls.size();
	if (n >= size) {
		n = size - 1;
	}
	memcpy(buffer, w->cls.c_str(), n);
	buffer[n] = '\0';
	return (int)n;
}

extern "C" int opents_win_is_kind_of(OpentsWin window, char const *class_name)
{
	Win * w = As_Win(window);
	if (w == nullptr || class_name == nullptr) {
		return 0;
	}
	return strcasecmp(w->cls.c_str(), class_name) == 0;
}

extern "C" OpentsWin opents_win_get_window(OpentsWin window, unsigned relationship)
{
	/*
	** GW_CHILD goes down a level, so it needs no parent of its own -- and the
	** main window has none. Leaving it out made GetTopWindow answer NULL for
	** every window, which is what stopped the hit test from ever finding a
	** dialog control.
	*/
	if (relationship == kGW_CHILD) {
		std::vector<Win *> *kids = Child_List_Of(window);
		return (kids != nullptr && !kids->empty()) ? (OpentsWin)kids->front() : nullptr;
	}

	Win * w = As_Win(window);
	if (w == nullptr) {
		return nullptr;
	}

	/* The list this window sits in -- its parent's, wherever that parent is. */
	std::vector<Win *> *siblings = Child_List_Of(w->parent_handle);
	if (siblings == nullptr) {
		return nullptr;
	}
	for (size_t i = 0; i < siblings->size(); i++) {
		if ((*siblings)[i] != w) {
			continue;
		}
		switch (relationship) {
		case kGW_HWNDFIRST:
			return (OpentsWin)siblings->front();
		case kGW_HWNDLAST:
			return (OpentsWin)siblings->back();
		case kGW_HWNDNEXT:
			return (i + 1 < siblings->size()) ? (OpentsWin)(*siblings)[i + 1] : nullptr;
		case kGW_HWNDPREV:
			return (i > 0) ? (OpentsWin)(*siblings)[i - 1] : nullptr;
		default:
			return nullptr;
		}
	}
	return nullptr;
}


extern "C" uintptr_t opents_win_get_long(OpentsWin window, int index)
{
	Win * w = As_Win(window);
	if (w == nullptr) {
		return 0;
	}
	switch (index) {
	case kGWL_STYLE:     return w->style;
	case kGWL_EXSTYLE:   return w->ex_style;
	case kGWL_ID:        return w->id;
	case kGWL_USERDATA:  return w->user;
	case kGWL_WNDPROC:
		/*
		** A window that has never been subclassed has no procedure of its
		** own, but on Win32 GetWindowLong still answers with the class's
		** default one -- and the engine stores that answer and later
		** calls it. Hand back the trampoline below instead of null, so
		** CallWindowProc has something to reach.
		*/
		if (w->proc != nullptr) {
			return (uintptr_t)w->proc;
		}
		return (uintptr_t)&opents_win_def_proc;
	case kDWLP_MSGRESULT: return (uintptr_t)w->dlg_result;
	case kDWLP_DLGPROC:
		/*
		** Win32 records the dialog procedure at DWLP_DLGPROC. The engine only
		** ever tests it for non-zero, to tell a real dialog (one that paints
		** its own backdrop from a WM_PAINT handler) apart from an ordinary
		** control. The logical window keeps no separate slot for it, but the
		** frame is flagged is_dialog at creation, so answer with a non-null
		** sentinel there and 0 everywhere else -- that is what lets
		** CtrlProc_Internal route the dialog's WM_PAINT to its paint path
		** instead of short-circuiting it with a bare ValidateRect.
		*/
		return w->is_dialog ? (uintptr_t)&opents_win_def_proc : 0;
	case kDWLP_USER:
		/*
		** Per-instance dialog user data (the ChooseCampaignStruct for the
		** campaign dialog, etc.). It shares the window's single user pointer
		** with GWL_USERDATA -- fine, a window uses at most one of them.
		*/
		return w->user;
	case kGWL_HWNDPARENT:
		/*
		** The owner the window was created against -- the native main game
		** window for a dialog frame, since that window has no Win node of its
		** own. Center_Window_Within_Window reads this to centre a dialog, so
		** answering 0 left dialogs at their template position; for the in-game
		** Options dialog that was y=480, entirely below the 640x480 view, and
		** the menu never appeared.
		*/
		return (uintptr_t)w->parent_handle;
	default:            return 0;
	}
}

extern "C" uintptr_t opents_win_set_long(OpentsWin window, int index, uintptr_t value)
{
	Win * w = As_Win(window);
	if (w == nullptr) {
		return 0;
	}
	uintptr_t previous = opents_win_get_long(window, index);
	switch (index) {
	case kGWL_STYLE:
		w->style = (unsigned long)value;
		w->visible = (w->style & kWS_VISIBLE) != 0;
		w->enabled = (w->style & kWS_DISABLED) == 0;
		break;
	case kGWL_EXSTYLE:
		w->ex_style = (unsigned long)value;
		break;
	case kGWL_ID:
		w->id = (unsigned)value;
		break;
	case kGWL_USERDATA:
		w->user = value;
		break;
	case kGWL_WNDPROC:
		w->prev_proc = w->proc;
		w->proc = (WinProc)value;
		break;
	case kDWLP_MSGRESULT:
		w->dlg_result = (intptr_t)value;
		break;
	case kDWLP_USER:
		w->user = value;
		break;
	case kGWL_HWNDPARENT:
		w->parent_handle = (OpentsWin)value;
		break;
	default:
		break;
	}
	return previous;
}

extern "C" void opents_win_get_rect(OpentsWin window, int *x, int *y, int *w, int *h)
{
	/* Not named 'w': that is the caller's out-parameter for the width. */
	Win * win = As_Win(window);
	if (win == nullptr) {
		return;
	}
	if (x) *x = win->x;
	if (y) *y = win->y;
	if (w) *w = win->w;
	if (h) *h = win->h;
}

extern "C" void opents_win_set_rect(OpentsWin window, int x, int y, int w, int h)
{
	Win * win = As_Win(window);
	if (win == nullptr) {
		return;
	}
	win->x = x;
	win->y = y;
	win->w = w;
	win->h = h;
}

extern "C" void opents_win_get_client_size(OpentsWin window, int *w, int *h)
{
	/* A logical window has no frame to subtract; client == window. */
	opents_win_get_rect(window, nullptr, nullptr, w, h);
}

extern "C" int opents_win_is_valid(OpentsWin window)
{
	return As_Win(window) != nullptr || (window != nullptr && window == g_MainWindow);
}

extern "C" int opents_win_is_visible(OpentsWin window)
{
	Win * w = As_Win(window);
	if (w == nullptr) {
		return window == g_MainWindow;
	}
	return w->visible && (w->parent == nullptr || opents_win_is_visible((OpentsWin)w->parent));
}

/*
**  Showing a window paints it, then its visible children.
**
**  Win32 gives every window its own WM_PAINT: a dialog frame paints its
**  backdrop, and each control paints itself when the system gets to it.
**  The port has no system doing that sweep, and the engine's owner-draw
**  layer only ever draws inside WM_PAINT, so showing a dialog has to walk
**  the tree -- parent first, because the frame's Draw_Dialog_Back lays
**  down the background the controls draw over.
*/
static void Paint_Tree(Win * w)
{
	if (w->cls == "ComboDropWin") {
		Win_Diag("painttree: ComboDropWin visible=%d rect=(%d,%d,%d,%d)",
			(int)w->visible, w->x, w->y, w->w, w->h);
	}
	w->dirty = true;
	Dispatch_To_Proc(w, kWM_PAINT, 0, 0);
	for (Win * child : w->children) {
		if (child->visible) {
			Paint_Tree(child);
		}
	}
}

extern "C" void opents_win_set_visible(OpentsWin window, int visible)
{
	Win * w = As_Win(window);
	if (w == nullptr) {
		return;
	}
	bool was = w->visible;
	w->visible = visible != 0;

	/*
	** Becoming visible is what produces a paint. On Windows the system adds
	** the window to the update region and delivers WM_PAINT when the queue
	** goes quiet; the port has no such sweep, and the engine's dialog layer
	** only draws inside WM_PAINT, so the message is delivered directly --
	** after the flag is set, because its handler asks GetUpdateRect whether
	** there is anything to paint.
	*/
	if (w->visible && !was) {
		Paint_Tree(w);
	}
}

/* The update region, in the window's own client coordinates. Win32 keeps it
** until the window is validated; the engine reads it inside WM_PAINT to size
** the area it redraws. */
extern "C" int opents_win_get_update(OpentsWin window, int *x, int *y, int *w, int *h)
{
	Win * win = As_Win(window);
	if (win == nullptr || !win->dirty || !win->visible) {
		return 0;
	}
	if (x) *x = 0;
	if (y) *y = 0;
	if (w) *w = win->w;
	if (h) *h = win->h;
	return 1;
}

extern "C" void opents_win_validate(OpentsWin window)
{
	Win * win = As_Win(window);
	if (win != nullptr) {
		win->dirty = false;
	}
}

extern "C" void opents_win_repaint_tree(OpentsWin window)
{
	Win * w = As_Win(window);
	if (w != nullptr) {
		Paint_Tree(w);
	}
}

extern "C" int opents_win_is_enabled(OpentsWin window)
{
	Win * w = As_Win(window);
	if (w == nullptr) {
		return 1;
	}
	return w->enabled && (w->parent == nullptr || opents_win_is_enabled((OpentsWin)w->parent));
}

extern "C" void opents_win_set_enabled(OpentsWin window, int enabled)
{
	Win * w = As_Win(window);
	if (w != nullptr) {
		w->enabled = enabled != 0;
	}
}

extern "C" int opents_win_get_text(OpentsWin window, char *buffer, unsigned size)
{
	Win * w = As_Win(window);
	if (w == nullptr || buffer == nullptr || size == 0) {
		return 0;
	}
	unsigned n = (unsigned)w->text.size();
	if (n >= size) {
		n = size - 1;
	}
	memcpy(buffer, w->text.c_str(), n);
	buffer[n] = '\0';
	return (int)n;
}

extern "C" void opents_win_set_text(OpentsWin window, char const *text)
{
	Win * w = As_Win(window);
	if (w != nullptr) {
		w->text = text ? text : "";
	}
}

extern "C" int opents_win_get_text_length(OpentsWin window)
{
	Win * w = As_Win(window);
	return (w != nullptr) ? (int)w->text.size() : 0;
}

extern "C" intptr_t opents_win_send(OpentsWin window, unsigned message, uintptr_t wparam, intptr_t lparam)
{
	/* The native main window gets no logical routing: its procedure is
	** the engine's, reached through the port queue by the pump. */
	if (window == g_MainWindow || As_Win(window) == nullptr) {
		return 0;
	}
	return Dispatch_To_Proc(As_Win(window), message, wparam, lparam);
}

extern "C" void opents_win_post(OpentsWin window, unsigned message, uintptr_t wparam, intptr_t lparam)
{
	OpentsMsg msg;
	msg.hwnd = window;
	msg.message = message;
	msg.wParam = wparam;
	msg.lParam = lparam;
	opents_msg_push(msg);
}

extern "C" int opents_win_enum_children(OpentsWin parent, OpentsEnumProc proc, intptr_t lparam)
{
	std::vector<Win *> *kids = Child_List_Of(parent);
	if (kids == nullptr || proc == nullptr) {
		return 0;
	}
	/* Copy first: the engine's callbacks can create and destroy. */
	std::vector<Win *> children = *kids;
	for (Win * c : children) {
		if (proc((OpentsWin)c, lparam) == 0) {
			return 0;
		}
	}
	return 1;
}

extern "C" void opents_win_end_dialog(OpentsWin dialog, intptr_t result)
{
	Win * w = As_Win(dialog);
	if (w == nullptr) {
		return;
	}
	w->dlg_result = result;
	w->is_dialog = false;    /* the loop below stops treating it as live */
	Dispatch_To_Proc(w, kWM_COMMAND, ((uintptr_t)kIDCANCEL & 0xFFFF) | (1u << 31), 0);
}

extern "C" intptr_t opents_win_dialog_result(OpentsWin dialog)
{
	Win * w = As_Win(dialog);
	return w ? w->dlg_result : 0;
}

extern "C" OpentsWin opents_win_next_tab_item(OpentsWin dialog, OpentsWin from, int previous, int search_group)
{
	Win * w = As_Win(dialog);
	if (w == nullptr || w->children.empty()) {
		return nullptr;
	}

	/* Walk the child list in creation order, which is z-order here and
	** matches the tab order a template author wrote. */
	int count = (int)w->children.size();
	int start = 0;
	if (from != nullptr) {
		Win * f = As_Win(from);
		for (int i = 0; i < count; i++) {
			if (w->children[i] == f) {
				start = previous ? i - 1 : i + 1;
				break;
			}
		}
	} else if (previous) {
		start = count - 1;
	}

	for (int step = 0; step <= count; step++) {
		int at = ((start + step) % count + count) % count;
		Win * c = w->children[at];
		if ((c->style & kWS_TABSTOP) && c->visible && c->enabled) {
			return (OpentsWin)c;
		}
	}
	return nullptr;
}

extern "C" OpentsWin opents_win_set_focus(OpentsWin window)
{
	OpentsWin previous = g_Focus;
	g_Focus = window;
	Win * w = As_Win(window);
	Win_Diag("focus -> %s id=%lu (%p) [prev %p]",
		(w != nullptr) ? w->cls.c_str() : "(native/null)",
		(w != nullptr) ? (unsigned long)w->id : 0ul,
		(void *)window, (void *)previous);
	return previous;
}

extern "C" OpentsWin opents_win_get_focus(void)
{
	return g_Focus;
}

extern "C" int opents_win_is_dialog_message(OpentsWin dialog, unsigned message,
                                            uintptr_t *wparam, intptr_t *lparam)
{
	Win * w = As_Win(dialog);
	if (w == nullptr || message != kWM_KEYDOWN) {
		return 0;
	}

	switch (wparam ? (*wparam & 0xFF) : 0) {
	case kVK_TAB: {
		/* Shift is bit 24 of Win32's GetKeyState high word; the port
		** tracks modifiers in port_input, but a plain walk suffices for
		** the engine's dialogs, which never mix Shift-Tab with focus
		** logic of their own. */
		OpentsWin next = opents_win_next_tab_item(dialog, g_Focus, 0, 0);
		if (next != nullptr) {
			g_Focus = next;
		}
		return 1;
	}
	case kVK_RETURN:
		opents_win_send(dialog, kWM_COMMAND, ((uintptr_t)kIDOK & 0xFFFF) | (1u << 31), 0);
		return 1;
	case kVK_ESCAPE:
		opents_win_send(dialog, kWM_COMMAND, ((uintptr_t)kIDCANCEL & 0xFFFF) | (1u << 31), 0);
		return 1;
	default:
		return 0;
	}
}

extern "C" int opents_dialog_units_x(int units)
{
	return units * 3 / 2;      /* 1.5 px per unit: 6 px / 4 */
}

extern "C" int opents_dialog_units_y(int units)
{
	return units * 13 / 8;     /* 1.625 px per unit: 13 px / 8 */
}
