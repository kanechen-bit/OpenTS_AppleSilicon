/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Bridge between the Win32 shim and the port's real window/event backend.
//
// - The Cocoa window API (opents_window_*) is C-linkage, implemented in
//   port_window.mm, and declared in portwindow/port_window.h.
// - The message queue and WNDPROC registry (opents_msg_*, opents_set/get_wndproc)
//   are C++-linkage, implemented in port_msgqueue.cpp, and used by the shim's
//   PeekMessage/GetMessage/DispatchMessage/RegisterClass/CreateWindowEx.
//
// windows_stub.h includes this header so the hollow GUI stubs can be given
// real behaviour without knowing about Cocoa.

#pragma once

#include <cstdint>

// The real Cocoa window API (C linkage).
#include "portwindow/port_window.h"

#ifdef __cplusplus

// A single queued window message. Mirrors the shim's MSG layout so copying is a
// straight field assignment (hwnd:void*, message:unsigned, wParam:uintptr_t,
// lParam:intptr_t -- matching HWND/UINT/WPARAM/LPARAM under the shim).
struct OpentsMsg {
	void *    hwnd;
	unsigned  message;
	uintptr_t wParam;
	intptr_t  lParam;
};

// The shape of a Win32 window procedure under the shim (CALLBACK expands to
// nothing, and HWND/UINT/WPARAM/LPARAM are void*/unsigned/uintptr_t/intptr_t).
typedef long long OpentsLResult;
typedef OpentsLResult (*OpentsWndProc)(void * hwnd, unsigned message, uintptr_t wParam, intptr_t lParam);

// Message queue (FIFO). Implemented in port_msgqueue.cpp.
void opents_msg_push(OpentsMsg msg);
bool opents_msg_pop(OpentsMsg * out);
bool opents_msg_peek(OpentsMsg * out);

// Registered window procedure, set by RegisterClass and used by DispatchMessage.
void  opents_set_wndproc(OpentsWndProc proc);
OpentsWndProc opents_get_wndproc(void);

#endif  // __cplusplus
