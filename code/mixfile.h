/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2025 Electronic Arts Inc.
 * Copyright 2026 OpenTS contributors
 *
 * Contains material derived from Electronic Arts source code.
 * Modified by OpenTS contributors, 2026.
 * EA's GPLv3 Section 7 additional terms and supplemental warranty
 * disclaimers apply; see LICENSE.md.
 ******************************************************************************/

/* $Header: /CounterStrike/MIXFILE.H 1     3/03/97 10:25a Joe_bostic $ */

#pragma once


#include "buff.h"
#include "listnode.h"

#include <cstdlib>

class PKey;

class MixFileClass : public Node<MixFileClass *>
{
	public:
		char const * Filename;			// Filename of mixfile.


		MixFileClass(char const *filename, PKey const * key);
		virtual ~MixFileClass(void);

		static bool Free(char const *filename);
		void Free(void);
		bool Cache(Buffer const * buffer = NULL);
		static bool Cache(char const *filename, Buffer const * buffer=NULL);
		static bool Offset(char const *filename, void ** realptr = 0, MixFileClass ** mixfile = 0, int * offset = 0, int * size = 0);
		static void const * Retrieve(char const *filename);

		/*
		**	Scans every mixfile currently loaded (cached in RAM or only on disk) for a member
		**	whose bytes contain the supplied magic string. Used as a fallback when a scenario
		**	index that the engine expects by name (e.g. "MISSIONS.PKT") is packed inside a
		**	mixfile under a different member name, as Tiberian Sun 2.03 does. Returns a heap
		**	copy of the first matching member (caller delete[]s it), or NULL.
		*/
		static char * Find_Member_Containing(char const * magic, int magic_len, int * out_size);

		/*
		**	Reads up to `length` bytes of member `index` (0-based) into `buffer`. Returns the
		**	number of bytes read (0 on failure). Works whether the mixfile is cached in RAM or
		**	only present on disk, so callers need not care which.
		*/
		int Read_Member(int index, char * buffer, int length) const;

		struct SubBlock {
			int CRC;				// CRC code for embedded file.
			int Offset;			// Offset from start of data section.
			int Size;				// Size of data subfile.

			int operator < (const SubBlock & two) const {return(CRC < two.CRC);};
			int operator > (const SubBlock & two) const {return(CRC > two.CRC);};
			int operator == (const SubBlock & two) const {return(CRC == two.CRC);};
		};

	private:
		static MixFileClass * Finder(char const * filename);
		int Offset(int crc, int * size = 0) const;

		/*
		**	If this mixfile has an attached message digest, then this flag
		**	will be true. The digest is checked only when the mixfile is
		**	cached.
		*/
		bool IsDigest;

		/*
		**	If the header to this mixfile has been encrypted, then this flag
		**	will be true. Although the header of the mixfile may be encrypted,
		**	the attached data files are not.
		*/
		bool IsEncrypted;

		/*
		**	If the cached memory block was allocated by this routine, then this
		**	flag will be true.
		*/
		bool IsAllocated;

		/*
		**	This is the initial file header. It tells how many files are embedded
		**	within this mixfile and the total size of all embedded files.
		*/
		#pragma pack(1)
		struct FileHeader {
			short	count;
			int	size;
		};
		#pragma pack()

		/*
		**	The number of files within the mixfile.
		*/
		int Count;

		/*
		**	This is the total size of all the data file embedded within the mixfile.
		**	It does not include the header or digest bytes.
		*/
		int DataSize;

		/*
		**	Start of raw data in within the mixfile.
		*/
		int DataStart;

		/*
		**	Points to the file header control block array. Each file in the mixfile will
		**	have an entry in this table. The entries are sorted by their (signed) CRC value.
		*/
		SubBlock * HeaderBuffer;

		/*
		**	If the mixfile has been cached, then this points to the cached data.
		*/
		void * Data;						// Pointer to raw data.

		static List<MixFileClass *> List;
};
