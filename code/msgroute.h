/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Delivery of mouse messages to the right dialog control once the frame is scaled.
//
// The dialogs are ordinary child windows sitting at positions in the frame, but the
// window shows that frame scaled, so Windows hands each click to whichever control
// physically covers the cursor rather than the one the player sees under it. Every
// window procedure that handles the mouse calls this first: it redoes the hit test in
// frame coordinates and sends the message on to the window that really owns the spot.

#pragma once

#include "win.h"


bool Route_Mouse_Message(HWND window, UINT message, WPARAM wparam, LPARAM lparam, LPARAM * translated_lparam);

// Clears the mouse-routing re-entrancy guard. A dialog opened from inside a
// click's own delivery (e.g. the multiplayer map picker raised by the Multiplay
// Map button) runs its message pump nested within that delivery, and every
// message the nested pump dispatches would otherwise be swallowed by the guard.
// Messages reaching Windows_Procedure are fresh ones from the queue -- a
// re-targeted message is sent straight to the control's own procedure -- so
// resetting here is safe and lets clicks reach the nested dialog.
void Route_Reset_Mouse_Reentry(void);

// Delivers keyboard input to the window that holds focus, the way native Win32 does.
// The event source pushes every key to the main window, so this must forward it to the
// focused control (e.g. the skirmish Name edit box) or typing never reaches the field.
bool Route_Keyboard_Message(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

