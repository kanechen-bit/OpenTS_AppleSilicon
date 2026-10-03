/*******************************************************************************
 *                                O P E N T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Win32 input state that has to be shared between the C++ shim and the Cocoa
// window backend:
//
//   - virtual-key down/up state, which the engine reads through GetKeyState()
//     while it is translating a key into an ASCII character and while it is
//     deciding which modifier bits to attach to a buffered key;
//   - the mouse capture window, which decides whether a drag keeps its
//     messages when the pointer leaves the control that started it.
//
// The event pump lives in port_window.mm (Objective-C++) and is the only writer
// of the key state, so this interface has C linkage and no C++ types: the .mm
// must be able to call it without the engine's -fms-extensions flags leaking
// into it. The reader side is the shim, which is plain C++.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Records that a virtual key is held (down != 0) or has been released. The
// sided modifier codes VK_LSHIFT/VK_RSHIFT/VK_LCONTROL/VK_RCONTROL/VK_LMENU/
// VK_RMENU (0xA0-0xA5) are folded onto the generic VK_SHIFT/VK_CONTROL/VK_MENU
// the engine actually tests, so either spelling works.
void opents_input_set_key_state(int vk, int down);

// As GetKeyState reports it: 0x8000 set while the key is held, plus 0x0001
// while it has gone down since the previous query (GetAsyncKeyState's
// "pressed since last call" bit).
short opents_input_get_key_state(int vk);
short opents_input_get_async_key_state(int vk);

// Mouse capture. NULL releases it. Returning NULL means "no window is holding
// the mouse", which is what the engine tests before honouring a drag.
void   opents_input_set_capture(void * hwnd);
void * opents_input_get_capture(void);

#ifdef __cplusplus
}
#endif
