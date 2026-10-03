/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

#include <cstdio>
#include <cstdlib>

/*
 * The one switch every port probe is gated on.
 *
 * The probes sit in hot paths -- every file read, every MIX open, every straw
 * Get, every mouse move -- and while they were unconditional a two and a half
 * minute run wrote 12,400 lines to stderr, drowning the engine's own output.
 * They are too useful to delete (they are what makes a wrong-data bug visible),
 * so they are gated instead: a normal run writes nothing.
 *
 * The flag is cached in a function-local static, so the test costs one byte
 * load rather than a getenv() per read. CCFileClass::Read and
 * RawFileClass::Read call it on every read.
 *
 * Prefix a probe with the macro:
 *
 *     OPENTS_IF_IO_TRACE fprintf(stderr, "[PORTREAD] ...\n", ...);
 *
 * Set OPENTS_TRACE_IO=1 in the environment to get the probes back.
 *
 * This lives in its own header rather than in com_stub.h because the
 * Objective-C++ window backend is compiled without the engine's force-included
 * shims (see the comment in port_window.mm), so it needs to include this
 * directly.
 */
inline bool _opents_trace_io(void)
{
	static const bool on = (getenv("OPENTS_TRACE_IO") != NULL);
	return on;
}
#define OPENTS_IF_IO_TRACE  if (_opents_trace_io())
