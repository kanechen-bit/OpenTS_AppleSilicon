/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

#include "iloco.h"

#if defined(OPENTS_EXPERIMENTAL_NONWIN32)
#include "port/com_stub.h"
#else
#include <comdef.h>
#endif

/// Names and comments from TLBs

EXTERN_C const IID IID_IPiggyback;

MIDL_INTERFACE("92FEA800-A184-11D1-B70A-00A024DDAFD1")
IPiggyback : public IUnknown
{
public:
	/*
	 * Piggybacks a locomotor onto this one.
	 */
	virtual HRESULT STDMETHODCALLTYPE Begin_Piggyback(ILocomotion * pointer) = 0;

	/*
	 * End piggyback process and restore locomotor interface pointer.
	 */
	virtual HRESULT STDMETHODCALLTYPE End_Piggyback(ILocomotion ** pointer) = 0;

	/*
	 * Is it ok to end the piggyback process?
	 */
	virtual boolean STDMETHODCALLTYPE Is_Ok_To_End(void) = 0;

	/*
	 * Fetches piggybacked locomotor class ID.
	 */
	virtual HRESULT STDMETHODCALLTYPE Piggyback_CLSID(GUID * classid) = 0;

	/*
	 * Is it currently piggy backing another locomotor?
	 */
	virtual boolean STDMETHODCALLTYPE Is_Piggybacking(void) = 0;
};

/*
 * IPiggyback com smart pointer declaration.
 */
_COM_SMARTPTR_TYPEDEF(IPiggyback, __uuidof(IPiggyback));
