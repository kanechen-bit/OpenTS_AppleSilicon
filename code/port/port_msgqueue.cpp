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
#include "port_trace.h"

#include <chrono>
#include <deque>
#include <vector>

namespace {

constexpr unsigned OPENTS_WM_TIMER = 0x0113u;
constexpr unsigned OPENTS_MIN_TIMER_MS = 10u;

struct OpentsTimer {
	void * hwnd;
	uintptr_t id;
	unsigned elapse_ms;
	void * proc;
	std::chrono::steady_clock::time_point next_fire;
};

std::vector<OpentsTimer> g_Timers;

void purge_queued_timer_messages(std::deque<OpentsMsg> & queue, void * hwnd, uintptr_t id)
{
	for (auto it = queue.begin(); it != queue.end(); ) {
		if (it->message == OPENTS_WM_TIMER && it->hwnd == hwnd && it->wParam == id) {
			it = queue.erase(it);
		} else {
			++it;
		}
	}
}

bool has_queued_timer_message(std::deque<OpentsMsg> const & queue, void * hwnd, uintptr_t id)
{
	for (OpentsMsg const & msg : queue) {
		if (msg.message == OPENTS_WM_TIMER && msg.hwnd == hwnd && msg.wParam == id) {
			return true;
		}
	}
	return false;
}

}  // namespace


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


uintptr_t opents_timer_set(void * hwnd, uintptr_t id, unsigned elapse_ms, void * proc)
{
	const unsigned clamped = elapse_ms < OPENTS_MIN_TIMER_MS ? OPENTS_MIN_TIMER_MS : elapse_ms;
	const auto next_fire = std::chrono::steady_clock::now() + std::chrono::milliseconds(clamped);

	purge_queued_timer_messages(g_MessageQueue, hwnd, id);

	for (OpentsTimer & timer : g_Timers) {
		if (timer.hwnd == hwnd && timer.id == id) {
			timer.elapse_ms = clamped;
			timer.proc = proc;
			timer.next_fire = next_fire;
			return id != 0 ? id : 1u;
		}
	}

	g_Timers.push_back(OpentsTimer{ hwnd, id, clamped, proc, next_fire });
	return id != 0 ? id : 1u;
}


bool opents_timer_kill(void * hwnd, uintptr_t id)
{
	purge_queued_timer_messages(g_MessageQueue, hwnd, id);

	for (auto it = g_Timers.begin(); it != g_Timers.end(); ++it) {
		if (it->hwnd == hwnd && it->id == id) {
			g_Timers.erase(it);
			return true;
		}
	}
	return false;
}


void opents_timer_pump(void)
{
	if (g_Timers.empty()) {
		return;
	}

	const auto now = std::chrono::steady_clock::now();
	for (OpentsTimer & timer : g_Timers) {
		if (now >= timer.next_fire) {
			timer.next_fire = now + std::chrono::milliseconds(timer.elapse_ms);
			if (!has_queued_timer_message(g_MessageQueue, timer.hwnd, timer.id)) {
				OpentsMsg msg;
				msg.hwnd = timer.hwnd;
				msg.message = OPENTS_WM_TIMER;
				msg.wParam = timer.id;
				msg.lParam = (intptr_t)timer.proc;
				g_MessageQueue.push_back(msg);
				OPENTS_IF_IO_TRACE std::fprintf(stderr, "[PORTTIMER] fire id=0x%zX\n", (size_t)timer.id);
			}
		}
	}
}
