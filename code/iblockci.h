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

EXTERN_C const IID IID_IBlockCipher;

MIDL_INTERFACE("E0113100-6A7C-11D1-B6F9-00A024DDAFD1")
IBlockCipher : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE Set_Key(LONG keylength, const void *key) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_Max_Key_Length(LONG *length) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_Block_Size(LONG *length) = 0;
	virtual HRESULT STDMETHODCALLTYPE Encrypt(LONG length, const void *plaintext, void *cyphertext) = 0;
	virtual HRESULT STDMETHODCALLTYPE Decrypt(LONG length, const void *cyphertext, void *plaintext) = 0;
};
