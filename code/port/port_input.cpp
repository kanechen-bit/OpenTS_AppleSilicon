/*******************************************************************************
 *                                O P E N T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Implementation of the shared Win32 input state declared in port_input.h.
//
// There is exactly one keyboard and one mouse, so this is plain file-scope
// state rather than anything keyed by window. The engine is single-window at
// this stage (see port_msgqueue.cpp for the same reasoning about the WNDPROC
// registry), and the state the engine reads is process-wide on Win32 too.

#include "port_input.h"

#include <cstring>

namespace {

// Indexed by virtual-key code. VK_ codes run 0x00-0xFE, so a flat table covers
// them with room to spare.
bool g_KeyDown[256] = {};

// Set when a key goes down and cleared when GetAsyncKeyState reports it, which
// is the whole of Win32's "has been pressed since the last call" contract.
bool g_KeyPressed[256] = {};

void * g_CaptureWindow = nullptr;

}  // namespace


extern "C" void opents_input_set_key_state(int vk, int down)
{
	if (vk < 0 || vk > 255) {
		return;
	}

	const bool held = (down != 0);
	if (held && !g_KeyDown[vk]) {
		g_KeyPressed[vk] = true;
	}
	g_KeyDown[vk] = held;

	/*
	**	Fold the sided modifier codes onto the generic ones.
	**
	**	Windows distinguishes VK_LSHIFT (0xA0) from VK_RSHIFT (0xA1), but the
	**	engine does not: keyboard.h names KN_LSHIFT and KN_RSHIFT both as
	**	VK_SHIFT, and Put_Key_Message tests VK_SHIFT/VK_CONTROL/VK_MENU alone
	**	when it decides which modifier bits to attach to a buffered key. So a
	**	call naming a side has to move the generic entry, or the engine sees a
	**	key that is held but reports itself as unmodified.
	*/
	switch (vk) {
		case 0xA0: case 0xA1:  // VK_LSHIFT, VK_RSHIFT
			g_KeyDown[0x10] = (g_KeyDown[0xA0] || g_KeyDown[0xA1]);
			break;

		case 0xA2: case 0xA3:  // VK_LCONTROL, VK_RCONTROL
			g_KeyDown[0x11] = (g_KeyDown[0xA2] || g_KeyDown[0xA3]);
			break;

		case 0xA4: case 0xA5:  // VK_LMENU, VK_RMENU
			g_KeyDown[0x12] = (g_KeyDown[0xA4] || g_KeyDown[0xA5]);
			break;

		case 0x10:             // VK_SHIFT -- keep the sides consistent with it
			g_KeyDown[0xA0] = held;
			g_KeyDown[0xA1] = held;
			break;

		case 0x11:             // VK_CONTROL
			g_KeyDown[0xA2] = held;
			g_KeyDown[0xA3] = held;
			break;

		case 0x12:             // VK_MENU
			g_KeyDown[0xA4] = held;
			g_KeyDown[0xA5] = held;
			break;

		default:
			break;
	}
}


extern "C" short opents_input_get_key_state(int vk)
{
	if (vk < 0 || vk > 255) {
		return 0;
	}

	short state = 0;
	if (g_KeyDown[vk]) {
		state |= (short)0x8000;
	}
	/*
	**	Win32 also reports the low bit for the keys that toggle rather than
	**	hold, and only when that key is the one being asked about. The engine
	**	reads it for VK_CAPITAL, but its use is commented out -- it is reported
	**	because a caller that does test it would otherwise always see "off".
	**
	**	The pump feeds the VK_CAPITAL entry from the platform's alpha-shift
	**	flag rather than from a press/release pair, so the entry already means
	**	"caps lock is on" rather than "caps lock is being held", which is what
	**	this bit is asking.
	*/
	if (vk == 0x14 && g_KeyDown[0x14]) {  // VK_CAPITAL
		state |= 0x0001;
	}
	return state;
}


extern "C" short opents_input_get_async_key_state(int vk)
{
	if (vk < 0 || vk > 255) {
		return 0;
	}

	short state = 0;
	if (g_KeyDown[vk]) {
		state |= (short)0x8000;
	}
	if (g_KeyPressed[vk]) {
		state |= 0x0001;
		g_KeyPressed[vk] = false;
	}
	return state;
}


extern "C" void opents_input_set_capture(void * hwnd)
{
	g_CaptureWindow = hwnd;
}


extern "C" void * opents_input_get_capture(void)
{
	return g_CaptureWindow;
}
