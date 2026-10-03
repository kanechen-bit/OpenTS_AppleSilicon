/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#define INCLUDE_COM
#include "always.h"

#include "savestream.h"

#include "saveload.h"


unsigned int LoadedSaveVersion = 0;


/// <summary>
/// Builds a save stream over the stream given.
/// </summary>
/// <param name="stream">The stream the members are to be read from or written to.</param>
/// <param name="mode">Is this stream saving or loading?</param>
SaveStreamClass::SaveStreamClass(IStream * stream, ModeType mode) :
	Stream(stream),
	Mode(mode),
	ErrorCode(stream != NULL ? S_OK : E_POINTER),
	FormatVersion(mode == MODE_LOAD ? LoadedSaveVersion : ExpectedGameVersion),
	OwnerType(NULL),
	OwnerID(0)
{
}


/// <summary>
/// Stops the pass, as though the stream itself had failed.
/// This is for a record that reads back as something no save could hold -- a length that
/// is negative, or one that does not fit the object waiting for it. An earlier failure is
/// left in place, since it is the one that explains the rest.
/// </summary>
void SaveStreamClass::Fail(void)
{
	if (SUCCEEDED(ErrorCode)) {
		ErrorCode = E_FAIL;
	}
}


/// <summary>
/// Moves a block of bytes between the object and the stream.
/// Every other Serialize reaches the stream through this one. Once something has gone
/// wrong the block is left alone and the failure is kept, so the rest of the pass runs
/// harmlessly and the caller finds out at the end.
/// </summary>
/// <param name="data">The bytes to write, or the place to read them back into.</param>
/// <param name="length">The number of bytes to move.</param>
void SaveStreamClass::Serialize_Bytes(void * data, int length)
{
	if (FAILED(ErrorCode)) {
		return;
	}

	ULONG moved = 0;
	HRESULT result;

	if (Mode == MODE_SAVE) {
		result = Stream->Write(data, length, &moved);
	} else {
		result = Stream->Read(data, length, &moved);
	}

	/*
	 * A stream that stops early has run out in the middle of an object, which leaves
	 * the rest of the members holding whatever they held before. Treat it as a failure
	 * rather than let a half-read object reach the game.
	 */
	if (SUCCEEDED(result) && moved != (ULONG)length) {
		result = E_FAIL;
	}

	ErrorCode = result;
}
