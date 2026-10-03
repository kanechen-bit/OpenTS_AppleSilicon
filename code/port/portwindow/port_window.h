/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Pure-C interface to the port's Cocoa window backend. Implemented in
// port_window.mm (Objective-C++) so that the rest of the shim, which is compiled
// with -fms-extensions, never has to touch Objective-C. The shim's HWND is a
// real NSWindow* (see windows_stub.h), so this layer just wraps the AppKit calls
// and exposes a C ABI the C++ shim can call.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Creates a real NSWindow and returns it as an opaque void* (the HWND). The
// caller owns the reference; pass it to opents_window_destroy to release it.
// popup != 0 builds a borderless window, otherwise a titled/resizable one.
void * opents_window_create(int x, int y, int w, int h, int popup);

// Makes the window visible and brings it forward.
void opents_window_show(void * hwnd);

// Physical-pixel size of the window's drawable (client) area.
void opents_window_get_client_size(void * hwnd, int * width, int * height);

// The window's outer frame, in the screen space described below.
void opents_window_get_window_rect(void * hwnd, int * left, int * top, int * right, int * bottom);

// Display refresh rate in Hz for the window's screen (0 if unknown).
int opents_window_get_refresh_hz(void * hwnd);

/*
 * Screen space.
 *
 * Win32 measures the desktop from the top left corner of the primary display
 * with y increasing downward; Cocoa measures it from the bottom left with y
 * increasing upward. The engine never reads screen metrics (GetSystemMetrics
 * answers 0 on this target), so the absolute origin is the port's to pick --
 * but every screen-space call has to agree, because the engine derives a
 * position by subtracting one from another:
 *
 *   WWMouseClass::Calc_Confining_Rect   GetClientRect, then ClientToScreen on
 *                                       both corners, gives an origin;
 *   WWMouseClass::Convert_Coordinate    GetCursorPos, minus that origin, is then
 *                                       scaled by the drawable size.
 *
 * For that scaling to be right the difference has to be in the same units as
 * GetClientRect, so this space is PHYSICAL PIXELS with a top-left origin --
 * Win32's convention, so the engine's arithmetic is unmodified.
 */
void opents_window_client_to_screen(void * hwnd, int * x, int * y);
void opents_window_screen_to_client(void * hwnd, int * x, int * y);

// The O/S pointer's position, in that same space.
void opents_window_get_cursor_pos(int * x, int * y);

// Moves the O/S pointer to a position in that space.
void opents_window_set_cursor_pos(int x, int y);

// Confines the pointer to a rectangle given as left/top/right/bottom in that
// space, or releases it when rect4 is NULL. Win32's ClipCursor.
void opents_window_clip_cursor(const int * rect4);

// Win32's ShowCursor: advances the cursor's display counter by +1 (show) or -1
// (hide) and returns the new value. The pointer is only visible while the
// counter is non-negative.
int opents_window_show_cursor(int show);

// Shows the engine's own pointer image over the window, replacing the system
// arrow. argb holds width * height pixels as A, R, G, B bytes, top row first;
// hotx/hoty locate the pointing pixel, measured from the top-left corner.
// Pass a null argb (sizes are then ignored) to stop showing a custom pointer;
// overall visibility stays governed by opents_window_show_cursor's counter.
// Safe to call repeatedly with the same image; a changed image rebuilds.
void opents_window_set_custom_cursor(const unsigned char * argb, int width, int height,
                                     int hotx, int hoty);

// Services any pending Cocoa events without blocking. Call this from the host
// run loop so the OS can deliver events and the window stays live.
void opents_window_pump(void);

// Releases the window created by opents_window_create.
void opents_window_destroy(void * hwnd);

#ifdef __cplusplus
}
#endif
