/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Cocoa implementation of the port window backend. This is the only translation
// unit that speaks Objective-C; everything else in the shim stays plain C++.
//
// Besides owning the NSWindow, this file is where the platform's input events
// become Win32 messages. The engine's pump is
//
//   Windows_Message_Handler -> PeekMessage -> GetMessage -> TranslateMessage
//                           -> DispatchMessage -> Windows_Procedure
//
// and Windows_Procedure hands anything the keyboard handler recognises to
// Keyboard->Message_Handler, which buffers the key or the click and its
// position. PeekMessage is the point at which a Win32 application receives
// events from the O/S, so that is where the platform queue is drained and
// translated here. Nothing else in the port has to know about NSEvent.

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <Metal/Metal.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ApplicationServices/ApplicationServices.h>

#include "port_window.h"
#include "port_bridge.h"
#include "port_input.h"
#include "port_trace.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>


/*
 * The window messages this file raises, spelled out rather than included.
 *
 * Their real home is code/port/shim/winuser.h, but that header reaches
 * windows_stub.h, which is written for the engine's translation units and
 * relies on flags this Objective-C++ file is deliberately not compiled with.
 * The values are the Win32 ones and are asserted against the shim's at the
 * bottom of this block by re-listing them; if the two ever disagree the
 * message numbering is wrong and every case in Windows_Procedure is off.
 */
enum {
	OpentsWM_DESTROY       = 0x0002,
	OpentsWM_MOVE          = 0x0003,
	OpentsWM_SIZE          = 0x0005,
	OpentsWM_MOUSEMOVE     = 0x0200,
	OpentsWM_LBUTTONDOWN   = 0x0201,
	OpentsWM_LBUTTONUP     = 0x0202,
	OpentsWM_LBUTTONDBLCLK = 0x0203,
	OpentsWM_RBUTTONDOWN   = 0x0204,
	OpentsWM_RBUTTONUP     = 0x0205,
	OpentsWM_RBUTTONDBLCLK = 0x0206,
	OpentsWM_MBUTTONDOWN   = 0x0207,
	OpentsWM_MBUTTONUP     = 0x0208,
	OpentsWM_MBUTTONDBLCLK = 0x0209,
	OpentsWM_MOUSEWHEEL    = 0x020A,
	OpentsWM_KEYDOWN       = 0x0100,
	OpentsWM_KEYUP         = 0x0101,
	OpentsWM_CHAR          = 0x0102,
	OpentsWM_SYSKEYDOWN    = 0x0104,
	OpentsWM_SYSKEYUP      = 0x0105,
	OpentsWM_ACTIVATEAPP   = 0x001C
};

/* Virtual-key codes the translation below names directly (Win32 values, and
   the same ones code/keyboard.h defines for the engine). */
enum {
	OpentsVK_LBUTTON  = 0x01,
	OpentsVK_RBUTTON  = 0x02,
	OpentsVK_MBUTTON  = 0x04,
	OpentsVK_SHIFT    = 0x10,
	OpentsVK_CONTROL  = 0x11,
	OpentsVK_MENU     = 0x12,
	OpentsVK_CAPITAL  = 0x14
};

#define OPENTS_WHEEL_DELTA 120


// The single window this backend serves. The shim's HWND is this NSWindow*.
static NSWindow * g_Window = nil;

// The pointer's position at the end of the previous pump, so the O/S cursor
// only has to be warped when it has actually escaped the clip rectangle.
static bool g_ClipActive = false;
static int  g_Clip[4] = { 0, 0, 0, 0 };

// ShowCursor's display counter. The pointer is visible while it is >= 0.
static int  g_CursorCount = 0;

// The engine's own pointer. Built once per shape frame from the pixels the
// cursor loader packs into a memory DIB, then re-asserted over the window
// whenever the engine asks (and again on every mouse move, since AppKit
// resets the cursor when tracking areas change under the pointer).
static NSCursor * g_CustomCursor = nil;
static BOOL g_CustomCursorActive = NO;

// Application activation, so Focus_Loss/Focus_Restore can be driven from here.
static bool g_AppActive = false;

// Whether each virtual key is currently held, which is what the key messages'
// "previous key state" bit has to report and what the mouse-move button bits
// are read from.
static bool g_KeyHeld[256] = {};

// Self-test hook: when set, the next pump shifts the window origin once so the
// geometry-change path below synthesises a WM_MOVE exactly as a real drag does.
static bool g_TestMovePending = false;

// Self-test hook: when set, the next pump grows the window once so the
// geometry-change path synthesises a WM_SIZE (+WM_MOVE) as a real resize does.
static bool g_TestResizePending = false;

extern "C" void opents_test_trigger_move(void)
{
	g_TestMovePending = true;
}

extern "C" void opents_test_trigger_resize(void)
{
	g_TestResizePending = true;
}


/*
 * macOS virtual keycode (the kVK_* values HIToolbox assigns to physical keys)
 * to Win32 virtual-key code.
 *
 * The engine buffers these codes verbatim -- keyboard.h names KN_ESC as
 * VK_ESCAPE, KN_LMOUSE as VK_LBUTTON, and so on for every entry -- so a key
 * that maps incorrectly is a key the game never sees, not a key it
 * misinterprets. Unmapped positions (the ones macOS leaves unused, plus Command
 * and Fn, which have no Win32 equivalent) read as 0 and are dropped.
 *
 * Note the positions that do not follow the key legends: 0x18/0x1B and
 * 0x21/0x1E/0x27/0x2A/0x2C/0x32/0x33 sit where the US layout puts them, and
 * 0x33 is Backspace while 0x75 is forward delete.
 */
static const unsigned short kOpentsVirtualKey[128] = {
	[0x00] = 0x41,  // A
	[0x01] = 0x53,  // S
	[0x02] = 0x44,  // D
	[0x03] = 0x46,  // F
	[0x04] = 0x48,  // H
	[0x05] = 0x47,  // G
	[0x06] = 0x5A,  // Z
	[0x07] = 0x58,  // X
	[0x08] = 0x43,  // C
	[0x09] = 0x56,  // V
	[0x0B] = 0x42,  // B
	[0x0C] = 0x51,  // Q
	[0x0D] = 0x57,  // W
	[0x0E] = 0x45,  // E
	[0x0F] = 0x52,  // R
	[0x10] = 0x59,  // Y
	[0x11] = 0x54,  // T
	[0x12] = 0x31,  // 1
	[0x13] = 0x32,  // 2
	[0x14] = 0x33,  // 3
	[0x15] = 0x34,  // 4
	[0x16] = 0x36,  // 6
	[0x17] = 0x35,  // 5
	[0x18] = 0xBB,  // =
	[0x19] = 0x39,  // 9
	[0x1A] = 0x37,  // 7
	[0x1B] = 0xBD,  // -
	[0x1C] = 0x38,  // 8
	[0x1D] = 0x30,  // 0
	[0x1E] = 0xDD,  // ]
	[0x1F] = 0x4F,  // O
	[0x20] = 0x55,  // U
	[0x21] = 0xDB,  // [
	[0x22] = 0x49,  // I
	[0x23] = 0x50,  // P
	[0x24] = 0x0D,  // Return
	[0x25] = 0x4C,  // L
	[0x26] = 0x4A,  // J
	[0x27] = 0xDE,  // '
	[0x28] = 0x4B,  // K
	[0x29] = 0xBA,  // ;
	[0x2A] = 0xDC,  // backslash
	[0x2B] = 0xBC,  // ,
	[0x2C] = 0xBF,  // /
	[0x2D] = 0x4E,  // N
	[0x2E] = 0x4D,  // M
	[0x2F] = 0xBE,  // .
	[0x30] = 0x09,  // Tab
	[0x31] = 0x20,  // Space
	[0x32] = 0xC0,  // `
	[0x33] = 0x08,  // Delete (backspace)
	[0x35] = 0x1B,  // Escape
	[0x38] = 0x10,  // Shift
	[0x39] = 0x14,  // Caps Lock
	[0x3A] = 0x12,  // Option
	[0x3B] = 0x11,  // Control
	[0x3C] = 0x10,  // Right Shift
	[0x3D] = 0x12,  // Right Option
	[0x3E] = 0x11,  // Right Control
	[0x40] = 0x80,  // F17
	[0x41] = 0x6E,  // Keypad .
	[0x43] = 0x6A,  // Keypad *
	[0x45] = 0x6B,  // Keypad +
	[0x47] = 0x0C,  // Keypad Clear
	[0x4B] = 0x6F,  // Keypad /
	[0x4C] = 0x0D,  // Keypad Enter
	[0x4E] = 0x6D,  // Keypad -
	[0x4F] = 0x81,  // F18
	[0x50] = 0x82,  // F19
	[0x51] = 0xBB,  // Keypad =
	[0x52] = 0x60,  // Keypad 0
	[0x53] = 0x61,  // Keypad 1
	[0x54] = 0x62,  // Keypad 2
	[0x55] = 0x63,  // Keypad 3
	[0x56] = 0x64,  // Keypad 4
	[0x57] = 0x65,  // Keypad 5
	[0x58] = 0x66,  // Keypad 6
	[0x59] = 0x67,  // Keypad 7
	[0x5A] = 0x83,  // F20
	[0x5B] = 0x68,  // Keypad 8
	[0x5C] = 0x69,  // Keypad 9
	[0x60] = 0x74,  // F5
	[0x61] = 0x75,  // F6
	[0x62] = 0x76,  // F7
	[0x63] = 0x72,  // F3
	[0x64] = 0x77,  // F8
	[0x65] = 0x78,  // F9
	[0x67] = 0x7A,  // F11
	[0x69] = 0x7C,  // F13
	[0x6A] = 0x7F,  // F16
	[0x6B] = 0x7D,  // F14
	[0x6D] = 0x79,  // F10
	[0x6F] = 0x7B,  // F12
	[0x71] = 0x7E,  // F15
	[0x72] = 0x2F,  // Help
	[0x73] = 0x24,  // Home
	[0x74] = 0x21,  // Page Up
	[0x75] = 0x2E,  // Forward Delete
	[0x76] = 0x73,  // F4
	[0x77] = 0x23,  // End
	[0x78] = 0x71,  // F2
	[0x79] = 0x22,  // Page Down
	[0x7A] = 0x70,  // F1
	[0x7B] = 0x25,  // Left
	[0x7C] = 0x27,  // Right
	[0x7D] = 0x28,  // Down
	[0x7E] = 0x26   // Up
};


// Ensures an NSApplication exists and is a normal foreground app. A command-line
// launched binary has no UI presence until this is called, and bgfx's Metal
// backend needs a live GUI session to create a device and a layer.
static NSApplication * Ensure_App(void)
{
	// A binary launched from a terminal (./run.sh) begins life as a *background*
	// process. macOS will happily let it open a window, but it refuses to route
	// keyboard or mouse input to a background process, so every menu and dialog
	// freezes on first input -- the "stuck on the chooser" symptom. TransformProcessType
	// promotes this process to a foreground GUI application, which is what lets
	// activateIgnoringOtherApps and makeKeyAndOrderFront actually pull input focus
	// to the game (without it the window shows but no mouse/key events ever reach
	// the pump).
	static bool s_Transformed = false;
	if (!s_Transformed) {
		ProcessSerialNumber psn = { 0, kCurrentProcess };
		(void)TransformProcessType(&psn, kProcessTransformToForegroundApplication);
		s_Transformed = true;
	}

	NSApplication * app = [NSApplication sharedApplication];
	if ([app activationPolicy] == NSApplicationActivationPolicyProhibited) {
		[app setActivationPolicy:NSApplicationActivationPolicyRegular];
	}
	return app;
}


/*
 * ---------------------------------------------------------------- geometry --
 */

static NSScreen * Opents_Screen_For_Cocoa_Point(NSPoint point)
{
	NSArray<NSScreen *> * screens = [NSScreen screens];
	for (NSScreen * candidate in screens) {
		if (NSPointInRect(point, [candidate frame])) {
			return candidate;
		}
	}
	return [screens count] > 0 ? [screens objectAtIndex:0] : nil;
}

static CGFloat Opents_Scale_For_Screen(NSScreen * screen)
{
	if (screen == nil) {
		return 1.0;
	}
	CGFloat scale = [screen backingScaleFactor];
	return (scale > 0.0) ? scale : 1.0;
}

/*
 * The top edge of the primary display in Cocoa's global space, which is the
 * constant the vertical flip is measured from. screens[0] is the display
 * carrying the menu bar, and its frame is the one Cocoa puts at y = 0, so its
 * top edge is also the top of CoreGraphics' display space.
 */
static CGFloat Opents_Primary_Top(void)
{
	NSArray<NSScreen *> * screens = [NSScreen screens];
	if ([screens count] == 0) {
		return 0.0;
	}
	return NSMaxY([[screens objectAtIndex:0] frame]);
}

static void Opents_Cocoa_To_Screen(NSPoint point, CGFloat scale, int * x, int * y)
{
	if (x != nullptr) *x = (int)lround(point.x * scale);
	if (y != nullptr) *y = (int)lround((Opents_Primary_Top() - point.y) * scale);
}

// The content area in Cocoa screen coordinates (bottom-left origin).
static NSRect Opents_Content_Rect(NSWindow * window)
{
	return [window convertRectToScreen:[[window contentView] frame]];
}

// Client (0,0) -- the top-left of the content area -- in screen pixels. A
// client position is just this origin plus the position, because the vertical
// flip that turns Cocoa's space into Win32's cancels in the round trip.
static void Opents_Content_Origin(NSWindow * window, int * x, int * y)
{
	NSRect content = Opents_Content_Rect(window);
	NSPoint top_left;
	top_left.x = content.origin.x;
	top_left.y = NSMaxY(content);
	Opents_Cocoa_To_Screen(top_left, Opents_Scale_For_Screen([window screen]), x, y);
}

// The engine reads the pointer by polling GetCursorPos every frame. On Win32 that
// answers the live O/S cursor. On macOS the reliable answer is queried straight from
// the window server via CGEventGetLocation(CGEventCreate(NULL)): it is current on
// every call regardless of whether this app is active or receives mouse events, which
// is exactly what a terminal-launched background process needs. [NSEvent mouseLocation]
// is NOT used -- it only advances when the app happens to receive a mouse event, so for
// a process that gets no input it answers a stale launch-time spot and the menu freezes.
static int  g_CursorScreenX = 0;
static int  g_CursorScreenY = 0;
static bool g_CursorKnown   = false;

static void Opents_Update_Cursor_From_Event(NSWindow * window, NSEvent * event)
{
	if (window == nil || event == nil) {
		return;
	}
	NSPoint on_screen = [window convertPointToScreen:[event locationInWindow]];
	CGFloat scale = Opents_Scale_For_Screen([window screen]);
	Opents_Cocoa_To_Screen(on_screen, scale, &g_CursorScreenX, &g_CursorScreenY);
	g_CursorKnown = true;
}

// Where the pointer is, in screen pixels, queried live from the window server.
static void Opents_Pointer_Position(int * x, int * y)
{
	@autoreleasepool {
		CGPoint q = CGPointZero;
		CGEventRef event = CGEventCreate(NULL);
		if (event != NULL) {
			q = CGEventGetLocation(event);
			CFRelease(event);
		}
		// q is in global Quartz coordinates: origin at the top-left of the primary
		// display, y increasing downward, in points. Flip to the Cocoa global space
		// (bottom-left origin) so the shared Opents_Cocoa_To_Screen flip is reused.
		NSPoint cocoa;
		cocoa.x = q.x;
		cocoa.y = Opents_Primary_Top() - q.y;
		Opents_Cocoa_To_Screen(cocoa, Opents_Scale_For_Screen(Opents_Screen_For_Cocoa_Point(cocoa)), x, y);
	}
}

// A position inside the content area, from an event, in client pixels.
static void Opents_Event_Client_Point(NSWindow * window, NSEvent * event, int * x, int * y)
{
	NSPoint on_screen = [window convertPointToScreen:[event locationInWindow]];
	NSRect content = Opents_Content_Rect(window);

	/*
	**	A position has to arrive in the engine's own client units, which are the
	**	units its control rectangles are measured in and the units msgroute.cpp
	**	hit-tests against. The content view is not always the same size on screen
	**	as it is to the engine: the presenter stretches the frame to fill it, so
	**	at anything but 1:1 a physical offset means a different client offset --
	**	and multiplying by the display's backing scale instead answers in device
	**	pixels, which is a different space again once the two disagree.
	**
	**	So the offset is taken as a fraction of the content area and re-expressed
	**	in client units. When the view is not stretched the fraction's denominator
	**	is the client size itself and this is the identity, DPI included.
	*/
	int cw = 0;
	int ch = 0;
	opents_window_get_client_size((__bridge void *)window, &cw, &ch);

	const CGFloat dx = on_screen.x - content.origin.x;
	const CGFloat dy = NSMaxY(content) - on_screen.y;

	if (x != nullptr) {
		*x = (content.size.width > 0.0 && cw > 0)
			? (int)lround(dx * (CGFloat)cw / content.size.width)
			: (int)lround(dx);
	}
	if (y != nullptr) {
		*y = (content.size.height > 0.0 && ch > 0)
			? (int)lround(dy * (CGFloat)ch / content.size.height)
			: (int)lround(dy);
	}
}


/*
 * ------------------------------------------------------------ input events --
 */

static void Opents_Update_Modifiers(NSEventModifierFlags flags)
{
	opents_input_set_key_state(OpentsVK_SHIFT,   (flags & NSEventModifierFlagShift) != 0);
	opents_input_set_key_state(OpentsVK_CONTROL, (flags & NSEventModifierFlagControl) != 0);
	opents_input_set_key_state(OpentsVK_MENU,    (flags & NSEventModifierFlagOption) != 0);
	// Caps Lock is fed as a latch rather than a hold, which is what its
	// GetKeyState bit means (see port_input.cpp).
	opents_input_set_key_state(OpentsVK_CAPITAL, (flags & NSEventModifierFlagCapsLock) != 0);
}

static void Opents_Push_Message(NSWindow * window, unsigned message, uintptr_t wparam, intptr_t lparam)
{
	OpentsMsg msg;
	msg.hwnd = (__bridge void *)window;
	msg.message = message;
	msg.wParam = wparam;
	msg.lParam = lparam;
	opents_msg_push(msg);

	// Temporary port diagnostic: proves the platform events actually reach the
	// engine's queue, and shows the translated coordinates. Keys and buttons are
	// always logged; mouse moves are sampled, since a moving pointer would
	// otherwise bury them.
	static int moves = 0;
	const bool is_move = (message == OpentsWM_MOUSEMOVE);
	if (!is_move || (++moves % 20) == 0) {
		int cx = (int)(short)(lparam & 0xFFFF);
		int cy = (int)(short)((lparam >> 16) & 0xFFFF);
		OPENTS_IF_IO_TRACE fprintf(stderr, "[PORTIN] msg=0x%04X wp=0x%llX at=%d,%d\n",
		        message, (unsigned long long)wparam, cx, cy);
		fflush(stderr);
	}
}

static void Opents_Push_Key(NSWindow * window, NSEvent * event, bool down)
{
	unsigned short keycode = (unsigned short)[event keyCode];
	if (keycode >= 128) {
		return;
	}
	unsigned short vk = kOpentsVirtualKey[keycode];
	if (vk == 0) {
		return;
	}

	const bool was_held = g_KeyHeld[vk];
	g_KeyHeld[vk] = down;
	opents_input_set_key_state(vk, down);

	/*
	**	lParam, as Win32 builds it for a keyboard message:
	**	  bits  0-15  repeat count (1; the engine does not read it)
	**	  bits 16-23  scancode -- the platform's own keycode stands in, because
	**	              the engine only uses it to look a key up again
	**	  bit      30  previous key state, which is how the engine filters the
	**	              O/S's auto-repeat out of the buffered key stream
	**	  bit      31  transition: 1 on release
	*/
	const uintptr_t scancode = (uintptr_t)(keycode & 0x7F);
	intptr_t lparam = (intptr_t)(1 | (scancode << 16));
	if (was_held) {
		lparam |= (intptr_t)(1u << 30);
	}
	if (!down) {
		lparam |= (intptr_t)(1u << 31);
	}

	/*
	**	A key pressed with Alt down arrives as the system-key variant on
	**	Windows, and the engine's dialog layer distinguishes the two. It is
	**	one or the other, never both: its keyboard Message_Handler treats them
	**	identically, so raising both would deliver the key twice.
	*/
	const bool alt = ([event modifierFlags] & NSEventModifierFlagOption) != 0;
	const unsigned plain = down ? OpentsWM_KEYDOWN : OpentsWM_KEYUP;
	const unsigned sys = down ? OpentsWM_SYSKEYDOWN : OpentsWM_SYSKEYUP;
	Opents_Push_Message(window, alt ? sys : plain, (uintptr_t)vk, lparam);

	/*
	**	On Windows TranslateMessage turns a character key's WM_KEYDOWN into a
	**	WM_CHAR, and the edit control types on that; the port's TranslateMessage
	**	is inert, so without this a key never produced a character and no edit
	**	box in the game could be filled in. The character comes from the event,
	**	which already carries the active keyboard layout's result. Only
	**	printable Latin-1 becomes a WM_CHAR -- Tab, Return, Backspace and the
	**	special keys (whose characters sit in Unicode's 0xF700 function range)
	**	are left to the dialog manager and the key-down handler. Alt combos are
	**	system keys and produce none.
	*/
	if (down && !alt) {
		NSString * typed = [event characters];
		if ([typed length] > 0) {
			unsigned short ch = (unsigned short)[typed characterAtIndex:0];
			if (ch >= 0x20 && ch <= 0xFF && ch != 0x7F) {
				Opents_Push_Message(window, OpentsWM_CHAR, (uintptr_t)ch, lparam);
			}
		}
	}
}

static void Opents_Push_Button(NSWindow * window, NSEvent * event, unsigned button, bool down, bool double_click)
{
	Opents_Update_Cursor_From_Event(window, event);

	int cx = 0;
	int cy = 0;
	Opents_Event_Client_Point(window, event, &cx, &cy);
	const uintptr_t wparam = (uintptr_t)((down ? 0x0001u : 0x0000u) |
	                                     ((button == OpentsVK_RBUTTON) ? 0x0002u : 0u) |
	                                     ((button == OpentsVK_MBUTTON) ? 0x0010u : 0u));
	const intptr_t lparam = (intptr_t)((cx & 0xFFFF) | ((cy & 0xFFFF) << 16));

	unsigned message = 0;
	if (button == OpentsVK_LBUTTON) {
		message = down ? (double_click ? OpentsWM_LBUTTONDBLCLK : OpentsWM_LBUTTONDOWN) : OpentsWM_LBUTTONUP;
	} else if (button == OpentsVK_RBUTTON) {
		message = down ? (double_click ? OpentsWM_RBUTTONDBLCLK : OpentsWM_RBUTTONDOWN) : OpentsWM_RBUTTONUP;
	} else {
		message = down ? (double_click ? OpentsWM_MBUTTONDBLCLK : OpentsWM_MBUTTONDOWN) : OpentsWM_MBUTTONUP;
	}

	/*
	**	The button is state the engine can also read directly, and the click
	**	position is what the game actually acts on: the buffered click carries
	**	the coordinates alongside it, and the tactical map and the menus both
	**	read them from there.
	*/
	opents_input_set_key_state(button, down);
	Opents_Push_Message(window, message, wparam, lparam);
}

static void Opents_Push_Mouse_Move(NSWindow * window, NSEvent * event)
{
	Opents_Update_Cursor_From_Event(window, event);

	int cx = 0;
	int cy = 0;
	Opents_Event_Client_Point(window, event, &cx, &cy);
	const intptr_t lparam = (intptr_t)((cx & 0xFFFF) | ((cy & 0xFFFF) << 16));

	uintptr_t wparam = 0;
	if (g_KeyHeld[OpentsVK_LBUTTON]) wparam |= 0x0001u;
	if (g_KeyHeld[OpentsVK_RBUTTON]) wparam |= 0x0002u;
	if (g_KeyHeld[OpentsVK_MBUTTON]) wparam |= 0x0010u;

	/*
	**	AppKit resets the pointer image as the mouse crosses tracking areas, so
	**	the engine's pointer has to be re-asserted on every move or the system
	**	arrow flickers back in over it.
	*/
	if (g_CustomCursorActive && g_CustomCursor != nil) {
		[g_CustomCursor set];
	}

	Opents_Push_Message(window, OpentsWM_MOUSEMOVE, wparam, lparam);
}

static void Opents_Push_Wheel(NSWindow * window, NSEvent * event)
{
	/*
	**	WM_MOUSEWHEEL is one of the two mouse messages whose position is in
	**	screen coordinates rather than client ones, and msgroute.cpp relies on
	**	that when it re-targets the message.
	*/
	int sx = 0;
	int sy = 0;
	Opents_Pointer_Position(&sx, &sy);

	double scroll = [event scrollingDeltaY];
	if (scroll == 0.0) {
		return;
	}

	/*
	**	A notch is WHEEL_DELTA on Windows. A device that reports precise deltas
	**	sends a stream of small values instead, so those are scaled up to a
	**	notch's worth rather than being passed through as a fraction of one,
	**	which would be reported as no movement at all.
	*/
	int delta;
	if ([event hasPreciseScrollingDeltas]) {
		delta = (int)lround(scroll * (double)OPENTS_WHEEL_DELTA * 0.5);
	} else {
		delta = (int)lround(scroll * (double)OPENTS_WHEEL_DELTA);
	}
	if (delta == 0) {
		// Keep the direction even when a precise device reports less than half
		// a notch at a time, so a slow scroll still arrives as movement.
		delta = (scroll > 0.0) ? 1 : -1;
	}

	const uintptr_t wparam = (uintptr_t)(((unsigned short)delta << 16) & 0xFFFF0000u);
	const intptr_t lparam = (intptr_t)((sx & 0xFFFF) | ((sy & 0xFFFF) << 16));
	Opents_Push_Message(window, OpentsWM_MOUSEWHEEL, wparam, lparam);
}


/*
 * --------------------------------------------------------- exported surface --
 */

extern "C" void * opents_window_create(int x, int y, int w, int h, int popup)
{
	OPENTS_IF_IO_TRACE fprintf(stderr, "[PORT] opents_window_create ENTER popup=%d (%dx%d)\n", popup, w, h);
	fflush(stderr);
	@autoreleasepool {
		NSApplication * app = Ensure_App();

		int width = (w > 0) ? w : 640;
		int height = (h > 0) ? h : 480;

		NSWindowStyleMask style;
		if (popup) {
			style = NSWindowStyleMaskBorderless;
		} else {
			style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
			        NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
		}

		NSRect rect = NSMakeRect((CGFloat)x, (CGFloat)y, (CGFloat)width, (CGFloat)height);
		NSWindow * window = [[NSWindow alloc] initWithContentRect:rect
		                                               styleMask:style
		                                                 backing:NSBackingStoreBuffered
		                                                   defer:NO];
		if (window == nil) {
			return nullptr;
		}

		[window setTitle:@"Tiberian Sun"];
		[window setBackgroundColor:[NSColor blackColor]];
		[window setOpaque:YES];
		// Key events must reach the game's window procedure rather than being
		// swallowed as an unhandled key equivalent; with no menu bar there is
		// nothing for AppKit to dispatch them to.
		[window setAcceptsMouseMovedEvents:YES];

		// Outside windowed mode the engine asks for a popup covering the desktop
		// at (0,0), but this shim reports no screen metrics, so the request
		// arrives as 0x0 and falls back to the game's native size above. The
		// engine only repositions the window in windowed mode (and its MoveWindow
		// is a no-op on this port anyway), so without this the frame sits in a
		// corner. This is the only place that actually sizes/positions the Cocoa
		// window, so without this the frame sits in a corner.
		//
		// Place it on the screen the pointer is on rather than [NSScreen
		// mainScreen]: mainScreen follows the keyboard focus, so on a multi-display
		// setup the game would appear on whichever display was last clicked. The
		// origin is then clamped into that screen's visible frame, so a window
		// larger than the work area still lands on screen instead of off an edge.
		if (popup && x == 0 && y == 0) {
			NSPoint mouse = [NSEvent mouseLocation];
			NSScreen * screen = nil;
			for (NSScreen * candidate in [NSScreen screens]) {
				if (NSPointInRect(mouse, [candidate frame])) {
					screen = candidate;
					break;
				}
			}
			if (screen == nil) {
				screen = [NSScreen mainScreen];
			}
			if (screen != nil) {
				NSRect visible = [screen visibleFrame];
				CGFloat wx = visible.origin.x + (visible.size.width - (CGFloat)width) / 2.0;
				CGFloat wy = visible.origin.y + (visible.size.height - (CGFloat)height) / 2.0;
				if (wx < NSMinX(visible)) wx = NSMinX(visible);
				if (wy < NSMinY(visible)) wy = NSMinY(visible);
				[window setFrameOrigin:NSMakePoint(wx, wy)];
			}
		}

		[app activateIgnoringOtherApps:YES];
		[window makeKeyAndOrderFront:nil];

		g_Window = window;
		// Plain hover (no button held) only produces NSEventTypeMouseMoved when the
		// content view carries a tracking area; without it the custom event loop never
		// sees moves, so hover highlighting and the pointer cache stay frozen. InVisibleRect
		// keeps the area correct across resizes and window moves.
		{
			NSView * content_view = [window contentView];
			if (content_view != nil) {
				NSTrackingArea * tracking = [[NSTrackingArea alloc]
					initWithRect:[content_view bounds]
					    options:(NSTrackingMouseMoved | NSTrackingActiveAlways | NSTrackingInVisibleRect)
					      owner:content_view
					   userInfo:nil];
				[content_view addTrackingArea:tracking];
			}
		}
		// Whatever the app's activation is now is the baseline; a change from
		// it is what raises WM_ACTIVATEAPP in the pump.
		g_AppActive = [app isActive];
		memset(g_KeyHeld, 0, sizeof(g_KeyHeld));

		// Temporary port diagnostic: proves the window exists and shows what the
		// engine will carry as HWND (bgfx takes this same pointer as platformData.nwh).
		OPENTS_IF_IO_TRACE fprintf(stderr, "[PORT] opents_window_create -> %p %s %dx%d\n",
		        (__bridge void *)window, popup ? "borderless" : "titled", width, height);
		fflush(stderr);

		// The handle the engine keeps as HWND is the NSWindow*. bgfx's Metal
		// backend accepts exactly this (it reads .contentView and attaches a
		// CAMetalLayer). The __bridge_retained transfer means we own the
		// reference until opents_window_destroy calls CFRelease.
		return (__bridge_retained void *)window;
	}
}


extern "C" void opents_window_show(void * hwnd)
{
	if (hwnd == nullptr) {
		return;
	}
	NSWindow * window = (__bridge NSWindow *)hwnd;
	[Ensure_App() activateIgnoringOtherApps:YES];
	[window makeKeyAndOrderFront:nil];
}


extern "C" void opents_window_get_client_size(void * hwnd, int * width, int * height)
{
	int w = 0;
	int h = 0;
	if (hwnd != nullptr) {
		NSWindow * window = (__bridge NSWindow *)hwnd;
		NSView * view = [window contentView];
		if (view != nil) {
			NSSize size = [view bounds].size;
			CGFloat scale = 1.0;
			NSScreen * screen = [window screen];
			if (screen != nil) {
				scale = [screen backingScaleFactor];
			}
			w = (int)(size.width * scale);
			h = (int)(size.height * scale);
		}
	}
	if (width != nullptr) *width = w;
	if (height != nullptr) *height = h;
	// Temporary port diagnostic (once): GetClientRect is called repeatedly, so only
	// the first measurement is reported.
	static bool traced = false;
	if (!traced) {
		traced = true;
		OPENTS_IF_IO_TRACE fprintf(stderr, "[PORT] get_client_size %p -> %dx%d\n", hwnd, w, h);
		fflush(stderr);
	}
}


extern "C" void opents_window_get_window_rect(void * hwnd, int * left, int * top, int * right, int * bottom)
{
	if (hwnd == nullptr) {
		if (left) *left = 0;
		if (top) *top = 0;
		if (right) *right = 0;
		if (bottom) *bottom = 0;
		return;
	}

	NSWindow * window = (__bridge NSWindow *)hwnd;
	NSRect frame = [window frame];
	CGFloat scale = Opents_Scale_For_Screen([window screen]);

	int l = 0;
	int t = 0;
	int r = 0;
	int b = 0;
	// The frame's top-left and bottom-right corners, so the flip is measured
	// from the same constant the client mapping uses.
	Opents_Cocoa_To_Screen(NSMakePoint(NSMinX(frame), NSMaxY(frame)), scale, &l, &t);
	Opents_Cocoa_To_Screen(NSMakePoint(NSMaxX(frame), NSMinY(frame)), scale, &r, &b);

	if (left) *left = l;
	if (top) *top = t;
	if (right) *right = r;
	if (bottom) *bottom = b;
}


extern "C" int opents_window_get_refresh_hz(void * hwnd)
{
	NSWindow * window = (hwnd != nullptr) ? (__bridge NSWindow *)hwnd : nil;
	NSScreen * screen = (window != nil) ? [window screen] : [NSScreen mainScreen];
	if (screen == nil) {
		return 60;
	}
	// NSScreen.maximumPossibleRefreshRate was removed from the SDK on recent
	// macOS, so read the rate through CoreGraphics instead (SDK-stable).
	double rate = 0.0;
	NSNumber * screenNumber = [screen.deviceDescription objectForKey:@"NSScreenNumber"];
	if (screenNumber != nil) {
		CGDirectDisplayID displayID = (CGDirectDisplayID)[screenNumber unsignedIntValue];
		CGDisplayModeRef mode = CGDisplayCopyDisplayMode(displayID);
		if (mode != NULL) {
			rate = CGDisplayModeGetRefreshRate(mode);
			CGDisplayModeRelease(mode);
		}
	}
	if (rate <= 0.0) {
		rate = 60.0;
	}
	return (int)rate;
}


extern "C" void opents_window_client_to_screen(void * hwnd, int * x, int * y)
{
	if (hwnd == nullptr) {
		return;
	}
	NSWindow * window = (__bridge NSWindow *)hwnd;
	int ox = 0;
	int oy = 0;
	Opents_Content_Origin(window, &ox, &oy);
	if (x) *x += ox;
	if (y) *y += oy;
}


extern "C" void opents_window_screen_to_client(void * hwnd, int * x, int * y)
{
	if (hwnd == nullptr) {
		return;
	}
	NSWindow * window = (__bridge NSWindow *)hwnd;
	int ox = 0;
	int oy = 0;
	Opents_Content_Origin(window, &ox, &oy);
	if (x) *x -= ox;
	if (y) *y -= oy;
}


extern "C" void opents_window_get_cursor_pos(int * x, int * y)
{
	if (g_CursorKnown) {
		if (x != nullptr) *x = g_CursorScreenX;
		if (y != nullptr) *y = g_CursorScreenY;
		return;
	}
	Opents_Pointer_Position(x, y);
}


extern "C" void opents_window_set_cursor_pos(int x, int y)
{
	/*
	**	CoreGraphics' display space already has the top-left origin this port
	**	uses, so the conversion is the scale alone: the flip that Cocoa's own
	**	global space needs is measured from the primary display's top edge,
	**	which is exactly where CoreGraphics puts its origin.
	*/
	CGFloat scale = Opents_Scale_For_Screen(Opents_Screen_For_Cocoa_Point([NSEvent mouseLocation]));
	CGWarpMouseCursorPosition(CGPointMake((CGFloat)x / scale, (CGFloat)y / scale));
}


extern "C" void opents_window_clip_cursor(const int * rect4)
{
	if (rect4 == nullptr) {
		g_ClipActive = false;
		return;
	}
	for (int i = 0; i < 4; i++) {
		g_Clip[i] = rect4[i];
	}
	g_ClipActive = (g_Clip[2] > g_Clip[0]) && (g_Clip[3] > g_Clip[1]);
}


extern "C" int opents_window_show_cursor(int show)
{
	/*
	**	Win32's display counter, and the same contract: the count moves by one
	**	either way and the pointer is drawn while it is not negative. The game
	**	parks it negative while it draws a pointer of its own, then walks it
	**	back up with `while (ShowCursor(TRUE) < 0) {}`, so the real cursor has
	**	to follow the sign of the count rather than the call itself.
	*/
	if (show) {
		g_CursorCount++;
		if (g_CursorCount >= 0) {
			[NSCursor unhide];
		}
	} else {
		g_CursorCount--;
		if (g_CursorCount < 0) {
			[NSCursor hide];
		}
	}
	return g_CursorCount;
}


// The engine's own pointer image is applied here; the cursor state lives with
// the other window globals at the top of the file.

extern "C" void opents_window_set_custom_cursor(const unsigned char * argb, int width, int height,
                                                int hotx, int hoty)
{
	NSCursor * cursor = nil;

	if (argb != nullptr && width > 0 && height > 0) {
		@autoreleasepool {
			CFDataRef data = CFDataCreate(kCFAllocatorDefault, argb, (CFIndex)(width * height * 4));
			if (data != nullptr) {
				CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
				CFRelease(data);
				if (provider != nullptr) {
					CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
					CGImageRef image = CGImageCreate((size_t)width, (size_t)height, 8, 32,
						(size_t)width * 4, space, kCGImageAlphaFirst | kCGBitmapByteOrder32Big,
						provider, nullptr, false, kCGRenderingIntentDefault);
					CFRelease(provider);
					CFRelease(space);
					if (image != nullptr) {
						NSImage * nsimage = [[NSImage alloc] initWithCGImage:image
								                                        size:NSMakeSize(width, height)];
						CFRelease(image);
						cursor = [[NSCursor alloc] initWithImage:nsimage
								                         hotSpot:NSMakePoint((CGFloat)hotx, (CGFloat)hoty)];
					}
				}
			}
		}
	}

	g_CustomCursor = cursor;
	g_CustomCursorActive = (cursor != nil);
	if (g_CustomCursorActive) {
		[g_CustomCursor set];
	}
}


// Keeps the pointer inside the clip rectangle, by hand.
//
// Win32 confines it in the driver so it can never leave; there is no such call
// here, so the escape is caught on the next pump and undone. Re-warping only
// when the position is actually outside means the pointer settles instead of
// fighting the user.
static void Opents_Enforce_Clip(void)
{
	if (!g_ClipActive) {
		return;
	}

	int x = 0;
	int y = 0;
	Opents_Pointer_Position(&x, &y);

	int cx = x;
	int cy = y;
	if (cx < g_Clip[0]) cx = g_Clip[0];
	if (cy < g_Clip[1]) cy = g_Clip[1];
	if (cx > g_Clip[2] - 1) cx = g_Clip[2] - 1;
	if (cy > g_Clip[3] - 1) cy = g_Clip[3] - 1;

		if (cx != x || cy != y) {
			opents_window_set_cursor_pos(cx, cy);
		}
	}


/*
**	Turns one Cocoa event into the engine's input stream and forwards it to
**	AppKit. Pulled out of opents_window_pump so the same handling runs whether
**	the event arrived in the default run-loop mode or in a tracking/modal mode
**	left behind by a window drag or menu session (see the secondary drain in
**	opents_window_pump).
*/
static void Opents_Process_Event(NSApplication * app, NSEvent * event)
{
	if (g_Window != nil) {
		Opents_Update_Modifiers([event modifierFlags]);

		switch ([event type]) {
			/*
			**	A key-down with no characters is the start of a composition --
			**	a dead key waiting for the accent to be completed -- and carries
			**	no virtual key the engine could act on.
			*/
			case NSEventTypeKeyDown:
				if ([[event characters] length] > 0) {
					Opents_Push_Key(g_Window, event, true);
				}
				break;

			case NSEventTypeKeyUp:
				Opents_Push_Key(g_Window, event, false);
				break;

			case NSEventTypeFlagsChanged:
				// Modifier state only; Opents_Update_Modifiers holds it.
				break;

			case NSEventTypeLeftMouseDown:
				Opents_Push_Button(g_Window, event, OpentsVK_LBUTTON, true,
				                   [event clickCount] >= 2);
				break;
			case NSEventTypeLeftMouseUp:
				Opents_Push_Button(g_Window, event, OpentsVK_LBUTTON, false, false);
				break;
			case NSEventTypeRightMouseDown:
				Opents_Push_Button(g_Window, event, OpentsVK_RBUTTON, true,
				                   [event clickCount] >= 2);
				break;
			case NSEventTypeRightMouseUp:
				Opents_Push_Button(g_Window, event, OpentsVK_RBUTTON, false, false);
				break;
			case NSEventTypeOtherMouseDown:
				Opents_Push_Button(g_Window, event, OpentsVK_MBUTTON, true,
				                   [event clickCount] >= 2);
				break;
			case NSEventTypeOtherMouseUp:
				Opents_Push_Button(g_Window, event, OpentsVK_MBUTTON, false, false);
				break;

			// A drag is a move with a button held, exactly as Win32's
			// WM_MOUSEMOVE with a button bit in wParam is.
			case NSEventTypeMouseMoved:
			case NSEventTypeLeftMouseDragged:
			case NSEventTypeRightMouseDragged:
			case NSEventTypeOtherMouseDragged:
				Opents_Push_Mouse_Move(g_Window, event);
				break;

			case NSEventTypeScrollWheel:
				Opents_Push_Wheel(g_Window, event);
				break;

			default:
				break;
		}
	}

	// Still hand the event to AppKit: it owns the window's own state (key
	// window, mouse location, cursor rectangles) and the engine's drawing
	// depends on that staying current.
	[app sendEvent:event];
}


extern "C" void opents_window_pump(void)
{

	@autoreleasepool {
		NSApplication * app = [NSApplication sharedApplication];

		// Self-test: simulate a user dragging the window once. Shifting the
		// origin makes the geometry-change detector below believe the window
		// moved and synthesise the same WM_MOVE the real drag would.
		if (g_TestMovePending && g_Window != nil) {
			NSRect f = [g_Window frame];
			[g_Window setFrame:NSMakeRect(f.origin.x + 30, f.origin.y + 30,
			                             f.size.width, f.size.height) display:NO];
			g_TestMovePending = false;
		}

		// Self-test: simulate a resize so the detector synthesises WM_SIZE + WM_MOVE.
		if (g_TestResizePending && g_Window != nil) {
			// Alternate between two fixed sizes rather than accumulating, so a
			// resize repeated every frame cannot run the window off the screen.
			static bool s_TestBig = false;
			s_TestBig = !s_TestBig;
			NSRect f = [g_Window frame];
			NSSize target = s_TestBig ? NSMakeSize(1000, 760) : NSMakeSize(1242, 881);
			// Neither size keeps the frame's aspect ratio, so the frame lands
			// letterboxed (non-zero DestX/DestY) and the test covers an offset
			// as well as a scale.
			g_TestResizePending = false;
			[g_Window setFrame:NSMakeRect(f.origin.x, f.origin.y, target.width, target.height) display:NO];
			g_TestResizePending = false;
		}

		if (g_Window != nil) {
			// Becoming or ceasing to be the active application raises the Win32
			// WM_ACTIVATEAPP, which drives Focus_Loss/Focus_Restore and the
			// engine's own GameInFocus flag. GameInFocus is what every
			// Wait_For_Focus() / while(!GameInFocus) loop gates on, so if it
			// never becomes true the title screen freezes right after the
			// startup movies (no signal, no crash -- just a spinning wait).
			//
			// When launched from a terminal (./run.sh), macOS leaves the parent
			// app active, so [app isActive] AND [g_Window isKeyWindow] both stay
			// false and no WM_ACTIVATEAPP(1) is ever raised. Force a ONE-TIME
			// activation so the window can become key and focus flows. We only
			// do this until focus is first secured, so Alt-Tab away still works
			// afterwards (we never yank focus back).
			static bool s_FocusSecured = false;
			if (!s_FocusSecured && [g_Window isVisible] && ![app isActive]) {
				[app activateIgnoringOtherApps:YES];
				OPENTS_IF_IO_TRACE fprintf(stderr, "[PORT] forced app activation (terminal launch)\n");
				fflush(stderr);
			}

			const bool active = [app isActive] || (g_Window != nil && [g_Window isKeyWindow]);
			if (active && !g_AppActive) {
				// Just became the active application: make our window the key window
				// so keyboard and mouse input are routed to it rather than lingering
				// on the terminal that launched us.
				[g_Window makeKeyWindow];
			}
			if (active) {
				s_FocusSecured = true;
			}
			if (active != g_AppActive) {
				g_AppActive = active;
				Opents_Push_Message(g_Window, OpentsWM_ACTIVATEAPP, active ? 1u : 0u, 0);
			}
		}

		/*
		**	The engine keeps a "confining rectangle" for the mouse (WWMouseClass::
		**	ConfiningRect) that it recomputes from the live window inside its
		**	WM_MOVE / WM_SIZE handlers. On Win32 those messages arrive the moment
		**	the window moves or resizes. On macOS the game window is moved by the
		**	user dragging its title bar, which never goes through the engine's
		**	MoveWindow, so no WM_MOVE is ever raised and the rectangle stays at its
		**	startup value. Convert_Coordinate then subtracts the stale origin from
		**	every polled pointer position, so after a move the mouse reads as dead
		**	while the keyboard (position-independent) keeps working. Detect the
		**	change here and report it the same way Win32 would.
		*/
		{
			static NSRect s_LastFrame = NSZeroRect;
			static bool   s_FrameInit = false;
			NSRect frame = [g_Window frame];
			if (!s_FrameInit) {
				s_LastFrame = frame;
				s_FrameInit = true;
			} else if (!NSEqualRects(frame, s_LastFrame)) {
				const bool size_changed = (frame.size.width != s_LastFrame.size.width) ||
				                          (frame.size.height != s_LastFrame.size.height);
				if (size_changed) {
					int cw = 0;
					int ch = 0;
					opents_window_get_client_size((__bridge void *)g_Window, &cw, &ch);
					Opents_Push_Message(g_Window, OpentsWM_SIZE, 0 /* SIZE_RESTORED */,
					                   (intptr_t)((cw & 0xFFFF) | ((ch & 0xFFFF) << 16)));
				}
			Opents_Push_Message(g_Window, OpentsWM_MOVE, 0, 0);
			OPENTS_IF_IO_TRACE fprintf(stderr, "[PORT] geometry change -> WM_MOVE%s\n",
			        size_changed ? "+WM_SIZE" : "");
			fflush(stderr);
			s_LastFrame = frame;

			/*
			** The polled pointer path (WWMouseClass::Get_Bounded_Position ->
			** GetCursorPos -> opents_window_get_cursor_pos) answers from a
			** per-event cache (g_CursorKnown / g_CursorScreenX/Y) instead of
			** re-querying the window server. A window move leaves that cache
			** pointing at the OLD screen position while ConfiningRect has been
			** recomputed to the new one, so every polled read (the main menu
			** and any other Get_Mouse_X/Y consumer) resolves to a position
			** offset by the move until the next real mouse event refreshes it.
			** Drop the cache on every geometry change so the next poll re-reads
			** the live cursor -- Opents_Pointer_Position queries the server
			** directly and is correct regardless of where the window went.
			*/
			g_CursorKnown = false;

			/*
			** A window drag can swallow the mouse-up that ends it, leaving the
			** port's button-held state (g_KeyHeld) stuck "down". The next real
			** click would then be treated as a duplicate press and ignored, so
			** the dialog appears frozen. Clear it so every button starts each
			** post-move click from a clean up state.
			*/
			opents_input_set_key_state(OpentsVK_LBUTTON, false);
			opents_input_set_key_state(OpentsVK_RBUTTON, false);
			opents_input_set_key_state(OpentsVK_MBUTTON, false);
		}

		/*
		** Self-test / harness click injector. A line "x y" (client pixels) in
		** /tmp/opents_click is turned into a move + left-button down + up with
		** the engine's own message path (Opents_Push_Message), exercising the
		** real Route_Mouse_Message hit test without needing OS-level event
		** permissions. The consumer (the campaign self-test) writes the file
		** once it has located the control it wants pressed; the pump only acts
		** on it when it appears and then consumes it so a stale file cannot
		** re-fire. Poll the heartbeat log to know when it has been consumed.
		*/
		{
			/* Only the self-test harness ever writes that file, so a normal run
			** must not pay a per-frame open for it. */
			static int inject_gate = -1;
			if (inject_gate < 0) {
				inject_gate = (getenv("OPENTS_SELFTEST") != NULL) ? 1 : 0;
			}
			FILE * cf = (inject_gate == 1) ? fopen("/tmp/opents_click", "r") : NULL;
			if (cf != NULL) {
				int cx = -1, cy = -1;
				if (fscanf(cf, "%d %d", &cx, &cy) == 2 && cx >= 0 && cy >= 0) {
					const intptr_t lp = (intptr_t)(((cy & 0xFFFF) << 16) | (cx & 0xFFFF));
					Opents_Push_Message(g_Window, OpentsWM_MOUSEMOVE, 0, lp);
					Opents_Push_Message(g_Window, OpentsWM_LBUTTONDOWN, 1, lp);
					Opents_Push_Message(g_Window, OpentsWM_LBUTTONUP, 0, lp);
					OPENTS_IF_IO_TRACE fprintf(stderr, "[PORT] injected click at %d,%d\n", cx, cy);
					fflush(stderr);
				}
				fclose(cf);
				unlink("/tmp/opents_click");
			}
		}
	}

		NSEvent * event = nil;

		while ((event = [app nextEventMatchingMask:NSEventMaskAny
		                                untilDate:[NSDate distantPast]
		                                   inMode:NSDefaultRunLoopMode
		                                  dequeue:YES]) != nil) {
			Opents_Process_Event(app, event);
		}

		/*
		**	A window drag, menu-tracking pop-up, or modal panel runs AppKit's own
		**	event loop in NSEventTrackingRunLoopMode / NSModalPanelRunLoopMode. The
		**	drag itself completes synchronously inside the [app sendEvent:] in
		**	Opents_Process_Event (for the title-bar mouse-down that starts it), so
		**	by the time we get here the session is finished -- but input that
		**	arrives afterwards (the click that dismisses the campaign dialog, a key
		**	press) can still be enqueued in one of those non-default modes. The
		**	default-mode drain above would never see it, so the game looks frozen
		**	after the window is moved. Drain those modes too (non-blocking) so no
		**	post-drag input is stranded.
		*/
		for (NSString * mode in @[NSEventTrackingRunLoopMode, NSModalPanelRunLoopMode]) {
			while ((event = [app nextEventMatchingMask:NSEventMaskAny
			                                untilDate:[NSDate distantPast]
			                                   inMode:mode
			                                  dequeue:YES]) != nil) {
				Opents_Process_Event(app, event);
			}
		}

		Opents_Enforce_Clip();
	}
}


extern "C" void opents_window_destroy(void * hwnd)
{
	if (hwnd == nullptr) {
		return;
	}
	NSWindow * window = (__bridge_transfer NSWindow *)hwnd;
	if (window == g_Window) {
		g_Window = nil;
		g_ClipActive = false;
	}
	[window close];
	// __bridge_transfer consumed the retain; nothing else to free.
	(void)window;
}
