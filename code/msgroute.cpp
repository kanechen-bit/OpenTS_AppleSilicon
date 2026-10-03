/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "always.h"

#include "msgroute.h"

#include "vidscale.h"
#include "win.h"
#include "dbgprint.h"

#include <vector>
#include <windowsx.h>
#include <cstdio>


// Set while a re-targeted message is being delivered, so the receiving procedure's own
// call passes it straight through instead of routing it a second time.
static bool _RoutingMouseMessage = false;


void Route_Reset_Mouse_Reentry(void)
{
	/*
	**  A dialog opened from inside a click's own delivery -- the multiplayer map
	**  picker is raised by the Multiplay Map button's WM_LBUTTONUP -- runs its
	**  WS_Wait_Dialog pump nested within the SendMessage that is still delivering
	**  that click. Every message the nested pump dispatches is a fresh one from
	**  the queue, yet the guard below was still set, so the whole time the picker
	**  was open every click passed through here unrouted and the dialog was
	**  inert. Messages reaching Windows_Procedure are always fresh (re-targeted
	**  ones go straight to the control's own procedure), so clearing the guard
	**  here cannot reintroduce the double-routing it guards against.
	*/
	_RoutingMouseMessage = false;
}


/// <summary>
/// Does this message carry a mouse position in its lParam?
/// </summary>
static bool Is_Mouse_Coordinate_Message(UINT message)
{
	switch (message) {
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MBUTTONDBLCLK:
		case WM_MOUSEWHEEL:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
		case WM_XBUTTONDBLCLK:
			return(true);

		default:
			return(false);
	}
}


/// <summary>
/// Is this message's position measured from the corner of the screen rather than from
/// the window it is delivered to?
/// </summary>
static bool Uses_Screen_Coordinates(UINT message)
{
	return(message == WM_MOUSEWHEEL);
}


/// <summary>
/// Finds the deepest descendant that owns a position, examining children topmost-first
/// the way native hit-testing does. A window answering WM_NCHITTEST with HTTRANSPARENT
/// passes the position to the sibling beneath it, which is what lets a click reach a
/// control that a group box frames.
/// </summary>
/// <param name="parent">The window whose children to search.</param>
/// <param name="parent_point">The position, in the parent's client space.</param>
/// <param name="screen_point">The same position in screen coordinates, for the
/// WM_NCHITTEST probe.</param>
/// <returns>The descendant that owns the position, or NULL when no child does.</returns>
static HWND Child_From_Logical_Point(HWND parent, POINT parent_point, POINT screen_point)
{
	/*
	**  Snapshot the children so a re-entrant WM_NCHITTEST cannot invalidate the
	**  iterator, then walk them topmost-first. The port paints children in list
	**  order, so the *last* child is drawn on top; the original walk started at
	**  the *first* child, which meant the control the player could see on top was
	**  never the one that received the click -- its (lower-painted) sibling did.
	**  Walking back-to-front makes the hit test agree with the paint order: the
	**  topmost child, then its topmost descendant, owns the point.
	*/
	std::vector<HWND> children;
	for (HWND child = GetTopWindow(parent); child != NULL; child = GetWindow(child, GW_HWNDNEXT)) {
		children.push_back(child);
	}

	/* One-shot dump: touching /tmp/opents_cflp_once dumps the next walk, then the
	** trigger is consumed, so a targeted click can be captured without flooding
	** the log with mouse-move walks. */
	{
		FILE * f = fopen("/tmp/opents_cflp_once", "rb");
		if (f != NULL) {
			fclose(f);
			remove("/tmp/opents_cflp_once");
			if (!children.empty()) {
				DebugString("CFLP parent=%p pt=(%d,%d) children=%d\n",
					(void *)parent, parent_point.x, parent_point.y, (int)children.size());
				for (size_t i = 0; i < children.size(); i++) {
					HWND ch = children[i];
					RECT r;
					GetWindowRect(ch, &r);
					RECT mr = r;
					MapWindowPoints(HWND_DESKTOP, parent, (POINT *)&mr, 2);
					DebugString("  child[%zu] id=%d rect=(%d,%d)-(%d,%d) mr=(%d,%d)-(%d,%d) in=%d\n",
						i, (int)GetWindowLong(ch, GWL_ID),
						r.left, r.top, r.right, r.bottom,
						mr.left, mr.top, mr.right, mr.bottom,
						(int)PtInRect(&mr, parent_point));
				}
			}
		}
	}

	for (size_t i = children.size(); i-- > 0; ) {
		HWND child = children[i];

		if (!IsWindowVisible(child) || !IsWindowEnabled(child)) {
			continue;
		}

		RECT rect;
		GetWindowRect(child, &rect);
		MapWindowPoints(HWND_DESKTOP, parent, (POINT *)&rect, 2);

		if (!PtInRect(&rect, parent_point)) {
			continue;
		}

		if (SendMessage(child, WM_NCHITTEST, 0, MAKELPARAM((short)screen_point.x, (short)screen_point.y)) == HTTRANSPARENT) {
			continue;
		}

		POINT child_point = parent_point;
		MapWindowPoints(parent, child, &child_point, 1);

		HWND descendant = Child_From_Logical_Point(child, child_point, screen_point);
		return(descendant != NULL ? descendant : child);
	}

	return(NULL);
}


/// <summary>
/// Finds the window that owns a position in the frame, walking the tree the way Windows
/// would if the controls stood where the player sees them.
/// A window holding the mouse capture keeps the message whatever the position, which is
/// what lets the controls track a drag.
/// </summary>
/// <param name="logical_point">The position, in the frame.</param>
/// <returns>The window that owns it, or the main window when no control does.</returns>
static HWND Window_From_Logical_Point(POINT logical_point)
{
	/*
	**  A captured window receives every mouse message, wherever the cursor
	**  is, until ReleaseCapture -- that is what keeps a combo-box drop-down
	**  receiving its clicks while it is open (Win32 SetCapture semantics).
	**  The previous guard restricted this to MainWindow and its *direct*
	**  children, so a drop-down (whose direct parent is the dialog frame,
	**  not MainWindow) never saw its capture and its item clicks fell
	**  through to the control painted behind it. Honour the capture for any
	**  valid window -- logical or the native main window.
	*/
	HWND capture = GetCapture();
	if (capture != NULL && IsWindow(capture)) {
		return(capture);
	}

	POINT screen_point = logical_point;
	ClientToScreen(MainWindow, &screen_point);

	HWND child = Child_From_Logical_Point(MainWindow, logical_point, screen_point);
	return(child != NULL ? child : MainWindow);
}



/// <summary>
/// Packs a position into an lParam in the space the receiving window expects.
/// </summary>
/// <param name="frame_point">The position, in the game frame.</param>
static LPARAM Logical_LParam_For_Target(HWND target, UINT message, POINT frame_point)
{
	/*
	**	frame_point arrives in the frame -- the space Route_Mouse_Message hit-tests
	**	in, and the space a control's own client coordinates are measured in. A
	**	real control wants the position relative to itself; the main window (the
	**	game's own rendered UI) wants the frame coordinate as it stands, which is
	**	what it used to be handed here after a conversion; a wheel message keeps
	**	its screen coordinates.
	*/
	POINT target_point = frame_point;

	if (Uses_Screen_Coordinates(message)) {
		Game_Point_To_Screen(target_point);
	} else if (target != MainWindow) {
		MapWindowPoints(MainWindow, target, &target_point, 1);
	}

	return(MAKELPARAM((short)target_point.x, (short)target_point.y));
}


/// <summary>
/// Converts a mouse message into frame coordinates and sends it to the window the player
/// sees under the cursor.
/// </summary>
/// <param name="window">The window whose procedure received the message.</param>
/// <param name="message">The message.</param>
/// <param name="wparam">Passed along unchanged.</param>
/// <param name="lparam">The position as Windows delivered it.</param>
/// <param name="translated_lparam">Receives the position to carry on with when this
/// returns false.</param>
/// <returns>bool; Was the message delivered elsewhere? The caller returns without
/// handling it when so.</returns>
bool Route_Mouse_Message(HWND window, UINT message, WPARAM wparam, LPARAM lparam, LPARAM * translated_lparam)
{
	if (translated_lparam != NULL) {
		*translated_lparam = lparam;
	}

	if (!Is_Mouse_Coordinate_Message(message) || MainWindow == NULL || _RoutingMouseMessage) {
		return(false);
	}

	/*
	**	The incoming lParam is a position in the main window's *client pixels* --
	**	the space the presenter draws the frame into. A control's own client
	**	coordinates, though, are *frame* coordinates: the controls are laid out on
	**	the game frame (Resize_Dialog stores the template's 1.5px-per-unit layout,
	**	and Center_Window_Within_Window measures MainWindow by the video mode
	**	rather than by its client area), and Get_Logical_Cursor_Pos says so
	**	outright. The hit test therefore has to run in the frame.
	**
	**	Hit-testing the client-pixel point directly is what broke the dialogs once
	**	the window stopped being exactly the frame size. The frame is letterboxed
	**	inside the drawable -- Update_Scale_Info centres it and holds its shape --
	**	so from that moment a client-pixel point sits at a different frame
	**	position, and a click landed on whichever control happened to be under the
	**	*unscaled* coordinates: the campaign chooser's buttons resolved to its
	**	listbox and its difficulty slider, so the buttons looked dead. At 1:1 the
	**	two spaces coincide, which is why a default-sized window never showed it.
	**
	**	Window_Point_To_Game is the same conversion the polled cursor path uses
	**	(WWMouseClass::Convert_Coordinate), so the two routes now agree.
	*/
	POINT point;
	point.x = GET_X_LPARAM(lparam);
	point.y = GET_Y_LPARAM(lparam);

	if (Uses_Screen_Coordinates(message)) {
		/* Wheel messages carry screen coordinates; fold them into client space first.
		** The wheel's own lParam is re-derived in screen space by
		** Logical_LParam_For_Target. */
		ScreenToClient(MainWindow, &point);
	} else if (window != MainWindow) {
		MapWindowPoints(window, MainWindow, &point, 1);
	}

	POINT frame_point = point;
	Window_Point_To_Game(frame_point);

	HWND target = Window_From_Logical_Point(frame_point);
	LPARAM target_lparam = Logical_LParam_For_Target(target, message, frame_point);

	if (message == WM_LBUTTONDOWN) {
		/* File-gated so a plain run pays nothing: touching
		** /tmp/opents_trace_route makes every click report the window it was
		** routed to, which is what a control that ignores the mouse looks like. */
		static int gate = -1;
		if (gate < 0) {
			FILE * f = fopen("/tmp/opents_trace_route", "rb");
			gate = (f != NULL) ? 1 : 0;
			if (f != NULL) {
				fclose(f);
			}
		}
		if (gate == 1) {
			DebugString("ROUTETEST client=(%d,%d) frame=(%d,%d) target=%p main=%p id=%ld lparam=(%d,%d) same=%d\n",
				point.x, point.y, frame_point.x, frame_point.y,
				(void *)target, (void *)MainWindow,
				(long)GetWindowLong(target, GWL_ID),
				(int)(short)LOWORD(target_lparam), (int)(short)HIWORD(target_lparam),
				(int)(target == window));
		}
	}

	if (target == window) {
		if (translated_lparam != NULL) {
			*translated_lparam = target_lparam;
		}
		return(false);
	}

	if (IsWindow(target)) {
		_RoutingMouseMessage = true;
		SendMessage(target, message, wparam, target_lparam);
		_RoutingMouseMessage = false;
	}

	return(true);
}


/// <summary>
/// Does this message carry keyboard input that belongs to the focused window?
/// </summary>
static bool Is_Keyboard_Message(UINT message)
{
	switch (message) {
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_CHAR:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_SYSCHAR:
		case WM_SYSDEADCHAR:
			return(true);

		default:
			return(false);
	}
}


/*
**	The port's event source pushes every key to the main window (see
**	port_window.mm's Opents_Push_Message), whereas native Win32 delivers keyboard
**	input to the window that currently holds focus. Without this, a key press never
**	reached a focused dialog control and nothing could be typed into the skirmish
**	Name field (or any edit box): the control captured focus on click
**	(opents_win_set_focus, in port_windows.cpp's edit handling) but the WM_CHAR /
**	WM_KEYDOWN that would fill it in lived only on the main window's procedure.
**	Route the message to the focused window, which is exactly what the OS would do.
**	When focus is the main window (or none) the message is left for the game's own
**	handler, so gameplay hotkeys are unchanged.
*/
static bool _RoutingKeyboardMessage = false;

bool Route_Keyboard_Message(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	if (!Is_Keyboard_Message(message) || MainWindow == NULL || _RoutingKeyboardMessage) {
		return(false);
	}

	HWND focus = GetFocus();
	if (focus == NULL || focus == window) {
		return(false);
	}

	_RoutingKeyboardMessage = true;
	SendMessage(focus, message, wparam, lparam);
	_RoutingKeyboardMessage = false;
	return(true);
}
