/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

#if defined(OPENTS_EXPERIMENTAL_NONWIN32)
#include "port/com_stub.h"
#else
#include <comdef.h>
#endif

/// Names and comments from TLBs

EXTERN_C const IID LIBID_BlowfishLibrary;
EXTERN_C const CLSID CLSID_BlowfishObject;
