/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Message queue and window-procedure registry backing the Win32 message pump in
// the non-Windows shim. The engine's Windows_Message_Handler drains this queue
// via PeekMessage/GetMessage and routes each entry through DispatchMessage, which
// calls the registered WNDPROC -- exactly the Win32 dispatch model, minus the OS.

#include "port_bridge.h"

#include <deque>


// Single window procedure for the main window. Dialogs (Phase 2d) will need a
// per-HWND map; for the present milestone only the game's main window exists.
static OpentsWndProc g_WndProc = nullptr;

// FIFO of posted messages. The engine's loop peeks (PM_NOREMOVE) and then
// GetMessage-pops, so we keep strict FIFO order.
static std::deque<OpentsMsg> g_MessageQueue;


void opents_msg_push(OpentsMsg msg)
{
	g_MessageQueue.push_back(msg);
}


bool opents_msg_pop(OpentsMsg * out)
{
	if (g_MessageQueue.empty()) {
		return false;
	}
	if (out != nullptr) {
		*out = g_MessageQueue.front();
	}
	g_MessageQueue.pop_front();
	return true;
}


bool opents_msg_peek(OpentsMsg * out)
{
	if (g_MessageQueue.empty()) {
		return false;
	}
	if (out != nullptr) {
		*out = g_MessageQueue.front();
	}
	return true;
}


void opents_set_wndproc(OpentsWndProc proc)
{
	g_WndProc = proc;
}


OpentsWndProc opents_get_wndproc(void)
{
	return g_WndProc;
}
