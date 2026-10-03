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

/* $Header: /CounterStrike/MIXFILE.CPP 2     3/13/97 2:06p Steve_tall $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : MIXFILE.CPP                                                  *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : August 8, 1994                                               *
 *                                                                                             *
 *                  Last Update : July 12, 1996 [JLB]                                          *
 *                                                                                             *
 *                                                                                             *
 *                  Modified by Vic Grippi for WwXlat Tool 10/14/96                            *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   MixFileClass::Cache -- Caches the named mixfile into RAM.                                 *
 *   MixFileClass::Cache -- Loads this particular mixfile's data into RAM.                     *
 *   MixFileClass::Finder -- Finds the mixfile object that matches the name specified.         *
 *   MixFileClass::Free -- Uncaches a cached mixfile.                                          *
 *   MixFileClass::MixFileClass -- Constructor for mixfile object.                             *
 *   MixFileClass::Offset -- Searches in mixfile for matching file and returns offset if found.*
 *   MixFileClass::Retrieve -- Retrieves a pointer to the specified data file.                 *
 *   MixFileClass::~MixFileClass -- Destructor for the mixfile object.                         *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "always.h"

#include <cstdio>

#include "mixfile.h"

#include "bsearch.h"
#include "buff.h"
#include "ccfile.h"
#include "conquer.h"
#include "crc.h"
#include "globals.h"
#include "pk.h"
#include "pkstraw.h"
#include "shastraw.h"
#include "xstraw.h"


//template<class T> int Compare(T const *obj1, T const *obj2) {
//	if (*obj1 < *obj2) return(-1);
//	if (*obj1 > *obj2) return(1);
//	return(0);
//};


/*
**	This is the pointer to the first mixfile in the list of mixfiles registered
**	with the mixfile system.
*/
//template<class T>
List<MixFileClass *> MixFileClass::List;

/// template class MixFileClass<CCFileClass>;

/// <summary>
/// Compares two mixfile index entries by their filename CRC.
/// This is the support routine that the standard search and sort routines use when they
/// need to order or scan the index block of a mixfile.
/// </summary>
/// <param name="ptr1">Pointer to the first index entry to compare.</param>
/// <param name="ptr2">Pointer to the second index entry to compare.</param>
/// <returns>Returns with -1, 0, or 1 according to whether the first entry sorts before,
/// with, or after the second.</returns>
int __cdecl compfunc(void const * ptr1, void const * ptr2)
{
	if (*(int const *)ptr1 < *(int const *)ptr2) return(-1);
	if (*(int const *)ptr1 > *(int const *)ptr2) return(1);
	return(0);
}


/***********************************************************************************************
 * MixFileClass::MixFileClass -- Constructor for mixfile object.                               *
 *                                                                                             *
 *    This is the constructor for the mixfile object. It takes a filename and a memory         *
 *    handler object and registers the mixfile object with the system. The index block is      *
 *    allocated and loaded from disk by this routine.                                          *
 *                                                                                             *
 * INPUT:   filename -- Pointer to the filename of the mixfile object.                         *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/08/1994 JLB : Created.                                                                 *
 *   07/12/1996 JLB : Handles compressed file header.                                          *
 *=============================================================================================*/
MixFileClass::MixFileClass(char const * filename, PKey const * key) :
	IsDigest(false),
	IsEncrypted(false),
	IsAllocated(false),
	Filename(NULL),
	Count(0),
	DataSize(0),
	DataStart(0),
	HeaderBuffer(NULL),
	Data(NULL)
{
	CCFileClass file(filename);		// Working file object.
	Filename = strdup(file.File_Name());
	OPENTS_IF_IO_TRACE fprintf(stderr, "[PORTMIX] ENTER %s\n", filename);
	FileStraw fstraw(file);
	PKStraw pstraw(PKStraw::DECRYPT, CryptRandom);
	Straw * straw = &fstraw;

	if (!file.Is_Available()) return;

	/*
	**	Stuctures used to hold the various file headers.
	*/
	FileHeader fileheader;
	struct {
		short First;		// Always zero for extended mixfile format.
		short Second;		// Bitfield of extensions to this mixfile.
	} alternate;

	/*
	**	Fetch the first bit of the file. From this bit, it is possible to detect
	**	whether this is an extended mixfile format or the plain format. An
	**	extended format may have extra options or data layout.
	*/
	int got = straw->Get(&alternate, sizeof(alternate));

	/*
	**	Detect if this is an extended mixfile. If so, then see if it is encrypted
	**	and/or has a message digest attached. Otherwise, just retrieve the
	**	plain mixfile header.
	*/
	if (alternate.First == 0) {
		IsDigest = ((alternate.Second & 0x01) != 0);
		IsEncrypted = ((alternate.Second & 0x02) != 0);

		if (IsEncrypted) {
			pstraw.Key(key);
			pstraw.Get_From(&fstraw);
			straw = &pstraw;
		}
		straw->Get(&fileheader, sizeof(fileheader));

	} else {
		memmove(&fileheader, &alternate, sizeof(alternate));
		straw->Get(((char*)&fileheader)+sizeof(alternate), sizeof(fileheader)-sizeof(alternate));
	}

	Count = fileheader.count;
	DataSize = fileheader.size;
	OPENTS_IF_IO_TRACE fprintf(stderr, "[PORTMIX] HDR %s avail=%d ext=%d enc=%d dig=%d count=%d size=%d\n",
		filename, (int)file.Is_Available(), (int)(alternate.First == 0),
		(int)IsEncrypted, (int)IsDigest, (int)Count, (int)DataSize);
//BGMono_Printf("Mixfileclass %s DataSize: %08x   \n",filename,DataSize);Get_Key();
	/*
	**	Load up the offset control array. If RAM is exhausted, then the mixfile is invalid.
	*/
	HeaderBuffer = new SubBlock [Count];
	if (HeaderBuffer == NULL) return;
	int indexgot = straw->Get(HeaderBuffer, Count * sizeof(SubBlock));
	OPENTS_IF_IO_TRACE fprintf(stderr, "[PORTMIX] IDXOK %s got=%d want=%d firstcrc=%08X\n",
		filename, indexgot, (int)(Count * sizeof(SubBlock)),
		Count > 0 ? (unsigned)HeaderBuffer[0].CRC : 0u);

	/*
	**	The start of the embedded mixfile data will be at the current file offset.
	**	This should be true even if the file header has been encrypted because the file
	**	header was cleverly written with just the sufficient number of padding bytes so
	**	that this condition would be true.
	*/
	int seekpos = file.Seek(0, SEEK_CUR);
	OPENTS_IF_IO_TRACE fprintf(stderr, "[PORTMIX] SEEK %s pos=%d biasstart=%d\n",
		filename, seekpos, (int)file.BiasStart);
	DataStart = seekpos + file.BiasStart;
	OPENTS_IF_IO_TRACE fprintf(stderr, "[PORTMIX] %s count=%d size=%d enc=%d dig=%d datastart=%d bias=%d\n",
		Filename, (int)Count, (int)DataSize, (int)IsEncrypted, (int)IsDigest,
		(int)DataStart, (int)file.BiasStart);

	/*
	**	Attach to list of mixfiles.
	*/
	List.Add_Tail(this);
}


/***********************************************************************************************
 * MixFileClass::Free -- Uncaches a cached mixfile.                                            *
 *                                                                                             *
 *    Use this routine to uncache a mixfile that has been cached.                              *
 *                                                                                             *
 * INPUT:   filename -- Pointer to the filename of the mixfile that is to be uncached.         *
 *                                                                                             *
 * OUTPUT:  bool; Was the mixfile found and freed?                                             *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   01/23/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
bool MixFileClass::Free(char const * filename)
{
	MixFileClass * ptr = Finder(filename);

	if (ptr) {
		ptr->Free();
		return(true);
	}
	return(false);
}


/***********************************************************************************************
 * MixFileClass::~MixFileClass -- Destructor for the mixfile object.                           *
 *                                                                                             *
 *    This destructor will free all memory allocated by this mixfile and will remove it from   *
 *    the system. A mixfile removed in this fashion must be created anew in order to be        *
 *    subsequent used.                                                                         *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/08/1994 JLB : Created.                                                                 *
 *   01/06/1995 JLB : Puts mixfile header table into EMS.                                      *
 *=============================================================================================*/
MixFileClass::~MixFileClass(void)
{
	/*
	**	Deallocate any allocated memory.
	*/
	if (Filename) {
		free((char *)Filename);
	}
	if (Data != NULL && IsAllocated) {
		delete [] Data;
		IsAllocated = false;
	}
	Data = NULL;

	if (HeaderBuffer != NULL) {
		delete [] HeaderBuffer;
		HeaderBuffer = NULL;
	}

	/*
	**	Unlink this mixfile object from the chain.
	*/
	Unlink();
}


/***********************************************************************************************
 * MixFileClass::Retrieve -- Retrieves a pointer to the specified data file.                   *
 *                                                                                             *
 *    This routine will return with a pointer to the specified data file if the file resides   *
 *    in memory. Otherwise, this routine returns NULL. Use this routine to access a resident   *
 *    file directly rather than going through the process of pseudo disk access. This will     *
 *    save both time and RAM.                                                                  *
 *                                                                                             *
 * INPUT:   filename -- Pointer to the filename of the data file to retrieve a pointer to.     *
 *                                                                                             *
 * OUTPUT:  Returns with a pointer to the data file's data. If the file is not in RAM, then    *
 *          NULL is returned.                                                                  *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/23/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
void const * MixFileClass::Retrieve(char const * filename)
{
	void * ptr = NULL;
	Offset(filename, &ptr);
	return(ptr);
};


/***********************************************************************************************
 * MixFileClass::Finder -- Finds the mixfile object that matches the name specified.           *
 *                                                                                             *
 *    This routine will scan through all registered mixfiles and return with a pointer to      *
 *    the matching mixfile. If no mixfile could be found that matches the name specified,      *
 *    then NULL is returned.                                                                   *
 *                                                                                             *
 * INPUT:   filename -- Pointer to the filename to search for.                                 *
 *                                                                                             *
 * OUTPUT:  Returns with a pointer to the matching mixfile -- if found.                        *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/08/1994 JLB : Created.                                                                 *
 *   06/08/1996 JLB : Only compares filename and extension.                                    *
 *=============================================================================================*/
MixFileClass * MixFileClass::Finder(char const * filename)
{
	MixFileClass * ptr = List.First();
	while (ptr->Is_Valid()) {
		char path[_MAX_PATH];
		char name[_MAX_FNAME];
		char ext[_MAX_EXT];

		/*
		**	Strip the drive and path (if present) off of the filename
		**	in the mixfile list. This enables a simple comparison to the
		**	filename specified. The filename specified won't have a path attached and
		**	the full pathname in the mixfile list WILL have a path attached. Hence, this
		**	stripping of the path is necessary.
		*/
		_splitpath(ptr->Filename, NULL, NULL, name, ext);
		_makepath(path, NULL, NULL, name, ext);

		if (stricmp(path, filename) == 0) {
			return(ptr);
		}
		ptr = ptr->Next();
	}
	return(NULL);
}


/***********************************************************************************************
 * MixFileClass::Cache -- Caches the named mixfile into RAM.                                   *
 *                                                                                             *
 *    This routine will cache the mixfile, specified by name, into RAM.                        *
 *                                                                                             *
 * INPUT:   filename -- The name of the mixfile that should be cached.                         *
 *                                                                                             *
 * OUTPUT:  bool; Was the cache successful?                                                    *
 *                                                                                             *
 * WARNINGS:   This routine could go to disk for a very long time.                             *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/08/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
bool MixFileClass::Cache(char const * filename, Buffer const * buffer)
{
	MixFileClass * mixer = Finder(filename);

	if (mixer != NULL) {
		return(mixer->Cache(buffer));
	}
	return(false);
}


/***********************************************************************************************
 * MixFileClass::Cache -- Loads this particular mixfile's data into RAM.                       *
 *                                                                                             *
 *    This load the mixfile data into ram for this mixfile object. This is the counterpart     *
 *    to the Free() function.                                                                  *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  bool; Was the file load successful?  It could fail if there wasn't enough room     *
 *                to allocate the raw data block.                                              *
 *                                                                                             *
 * WARNINGS:   This routine goes to disk for a potentially very long time.                     *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/08/1994 JLB : Created.                                                                 *
 *   07/12/1996 JLB : Handles attached message digest.                                         *
 *=============================================================================================*/
bool MixFileClass::Cache(Buffer const * buffer)
{
	/*
	**	If the mixfile is already cached, then no action needs to be performed.
	*/
	if (Data != NULL) return(true);

	/*
	**	If a buffer was supplied (and it is big enough), then use it as the data block
	**	pointer. Otherwise, the data block must be allocated.
	*/
	if (buffer != NULL && (buffer->Get_Size() == 0 || buffer->Get_Size() >= DataSize)) {
		Data = buffer->Get_Buffer();

	} else {
		Data = new char [DataSize];
		IsAllocated = true;

		/// Purpose unknown; possibly a debugging aid.
		for (char *i = (char *)Data; i < &((char *)Data)[DataSize]; i += 4096) {

			/// Required to stop the compiler from optimizing this loop away.
			volatile char tmp = *i;
		}

		Call_Back();
	}

	/*
	**	If there is a data buffer to fill, then fill it now.
	*/
	if (Data != NULL) {
		//T file(Filename);
		CCFileClass file(Filename);

		FileStraw fstraw(file);
		Straw * straw = &fstraw;

		/*
		**	If a message digest is attached, then link a SHA straw segment to the data
		**	stream so that the actual SHA can be compared with the attached one.
		*/
		SHAStraw sha;
		if (IsDigest) {
			sha.Get_From(fstraw);
			straw = &sha;
		}

		/*
		**	Bias the file to the actual start of the data. This is necessary because the
		**	real data starts some distance (not so easily determined) from the beginning of
		**	the real file.
		*/
		file.Open(FileClass::READ);
		file.Bias(0);
		file.Bias(DataStart);

		/*
		**	Fetch the whole mixfile data in one step. If the number of bytes retrieved
		**	does not equal that requested, then this indicates a serious error.
		*/
		int actual = straw->Get(Data, DataSize);
		OPENTS_IF_IO_TRACE fprintf(stderr, "[PORTMIX] CACHEREAD %s actual=%d want=%d datastart=%d bias=%d\n",
			Filename, actual, (int)DataSize, (int)DataStart, (int)file.BiasStart);
		if (actual != DataSize) {
			delete [] Data;
			Data = NULL;
			file.Error(EIO);
			return(false);
		}

		/*
		**	If there is a digest attached to this mixfile, then read it in and
		**	compare it to the generated digest. If they don't match, then
		**	return with the "failure to cache" error code.
		*/
		if (IsDigest) {
			char digest1[20];
			char digest2[20];
			sha.Result(digest2);
			int dgot = fstraw.Get(digest1, sizeof(digest1));
			int same = (memcmp(digest1, digest2, sizeof(digest1)) == 0);
			OPENTS_IF_IO_TRACE fprintf(stderr, "[PORTMIX] CACHEDIG %s dgot=%d same=%d\n"
				"   computed=%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n"
				"   stored  =%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
				Filename, dgot, same,
				(unsigned char)digest2[0], (unsigned char)digest2[1], (unsigned char)digest2[2], (unsigned char)digest2[3],
				(unsigned char)digest2[4], (unsigned char)digest2[5], (unsigned char)digest2[6], (unsigned char)digest2[7],
				(unsigned char)digest2[8], (unsigned char)digest2[9], (unsigned char)digest2[10], (unsigned char)digest2[11],
				(unsigned char)digest2[12], (unsigned char)digest2[13], (unsigned char)digest2[14], (unsigned char)digest2[15],
				(unsigned char)digest2[16], (unsigned char)digest2[17], (unsigned char)digest2[18], (unsigned char)digest2[19],
				(unsigned char)digest1[0], (unsigned char)digest1[1], (unsigned char)digest1[2], (unsigned char)digest1[3],
				(unsigned char)digest1[4], (unsigned char)digest1[5], (unsigned char)digest1[6], (unsigned char)digest1[7],
				(unsigned char)digest1[8], (unsigned char)digest1[9], (unsigned char)digest1[10], (unsigned char)digest1[11],
				(unsigned char)digest1[12], (unsigned char)digest1[13], (unsigned char)digest1[14], (unsigned char)digest1[15],
				(unsigned char)digest1[16], (unsigned char)digest1[17], (unsigned char)digest1[18], (unsigned char)digest1[19]);
			if (memcmp(digest1, digest2, sizeof(digest1)) != 0) {
				delete [] Data;
				Data = NULL;
				return(false);
			}
		}

		return(true);
	}
	IsAllocated = false;
	return(false);
}


/***********************************************************************************************
 * MixFileClass::Free -- Frees the allocated raw data block (not the index block).             *
 *                                                                                             *
 *    This routine will free the (presumably large) raw data block, but leave the index        *
 *    block intact. By using this in conjunction with the Cache() function, one can maintain   *
 *    tight control of memory usage. If the index block is desired to be freed, then the       *
 *    mixfile object must be deleted.                                                          *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/08/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
void MixFileClass::Free(void)
{
	if (Data != NULL && IsAllocated) {
		delete [] Data;
	}
	Data = NULL;
	IsAllocated = false;
}


/***********************************************************************************************
 * MixFileClass::Offset -- Determines the offset of the requested file from the mixfile system.*
 *                                                                                             *
 *    This routine will take the filename specified and search through the mixfile system      *
 *    looking for it. If the file was found, then the mixfile it was found in, the offset      *
 *    from the start of the mixfile, and the size of the embedded file will be returned.       *
 *    Using this method it is possible for the CCFileClass system to process it as a normal    *
 *    file.                                                                                    *
 *                                                                                             *
 * INPUT:   filename    -- The filename to search for.                                         *
 *                                                                                             *
 *          realptr     -- Stores a pointer to the start of the file in memory here. If the    *
 *                         file is not in memory, then NULL is stored here.                    *
 *                                                                                             *
 *          mixfile     -- The pointer to the corresponding mixfile is placed here. If no      *
 *                         mixfile was found that contains the file, then NULL is stored here. *
 *                                                                                             *
 *          offset      -- The starting offset from the beginning of the parent mixfile is     *
 *                         stored here.                                                        *
 *                                                                                             *
 *          size        -- The size of the embedded file is stored here.                       *
 *                                                                                             *
 * OUTPUT:  bool; Was the file found? The file may or may not be resident, but it does exist   *
 *                 and can be opened.                                                          *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/17/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
bool MixFileClass::Offset(char const * filename, void ** realptr, MixFileClass ** mixfile, int * offset, int * size)
{
	MixFileClass * ptr;

	if (filename == NULL) {
		assert(filename != NULL);//BG
		return(false);
	}

	/*
	**	Create the key block that will be used to binary search for the file.
	*/
	/*
	**	The name CRC is defined over the upper case name. `filename` is a
	**	caller-owned const string and is very often a literal, which lives in
	**	read-only memory, so the folding has to happen in a local buffer rather
	**	than in place. Uppercasing in place also silently rewrote the caller's
	**	string, which is not something a lookup should do.
	*/
	char upper[_MAX_PATH];
	strncpy(upper, filename, sizeof(upper));
	upper[sizeof(upper) - 1] = '\0';
	strupr(upper);

	int crc = (CRCEngine()(upper, (int)strlen(upper)));

	SubBlock key;
	key.CRC = crc;

	/*
	**	Sweep through all registered mixfiles, trying to find the file in question.
	*/
	ptr = List.First();
	while (ptr->Is_Valid()) {
		SubBlock * block;

		/*
		**	Binary search for the file in this mixfile. If it is found, then extract the
		**	appropriate information and store it in the locations provided and then return.
		*/
		//block = (SubBlock *)bsearch(&key, ptr->HeaderBuffer, ptr->Count, sizeof(SubBlock), compfunc);
		block = Binary_Search<SubBlock>(ptr->HeaderBuffer, ptr->Count, key);
		if (block != NULL) {
			if (mixfile != NULL) *mixfile = ptr;
			if (size != NULL) *size = block->Size;
			if (realptr != NULL) *realptr = NULL;
			if (offset != NULL) *offset = block->Offset;
			if (realptr != NULL && ptr->Data != NULL) {
				*realptr = (char *)ptr->Data + block->Offset;
			}
			if (ptr->Data == NULL && offset != NULL) {
				*offset += ptr->DataStart;
			}
			return(true);
		}

		/*
		**	Advance to next mixfile.
		*/
		ptr = ptr->Next();
	}

	/*
	**	All the mixfiles have been examined but no match was found. Return with the non success flag.
	*/
	assert(1);//BG
	return(false);
}


/***********************************************************************************************
 * MixFileClass::Read_Member -- copy bytes of a member out of a mixfile.                        *
 *                                                                                             *
 *    Works whether the mixfile is held in RAM (Data != NULL) or only on disk: in the latter    *
 *    case the member is read straight from the mixfile via the exposed filename.                *
 ***********************************************************************************************/
int MixFileClass::Read_Member(int index, char * buffer, int length) const
{
	if (index < 0 || index >= Count || buffer == NULL || length <= 0) {
		return(0);
	}

	SubBlock const & block = HeaderBuffer[index];
	int toread = block.Size < length ? block.Size : length;
	if (toread <= 0) {
		return(0);
	}

	if (Data != NULL) {
		memcpy(buffer, (char const *)Data + block.Offset, toread);
		return(toread);
	}

	/*
	**	Not cached: the member data is plaintext within the mixfile, starting at
	**	DataStart + block.Offset. The (possibly encrypted) header does not reach here.
	*/
	CCFileClass file(Filename);
	if (!file.Open(FileClass::READ)) {
		return(0);
	}
	file.Seek(DataStart + block.Offset);
	int got = file.Read(buffer, toread);
	file.Close();
	return(got);
}


/***********************************************************************************************
 * MixFileClass::Find_Member_Containing -- find a member by content across loaded mixfiles.     *
 *                                                                                             *
 *    Tiberian Sun 2.03 packs the multiplayer scenario index the engine reads as "MISSIONS.PKT"  *
 *    inside EXPAND01.MIX under a different member name (and EXPAND01.MIX is not cached in RAM  *
 *    by the engine), so a name lookup never matches. This scans the bytes of every loaded        *
 *    mixfile member for the supplied magic (e.g. "[MultiMaps]") and returns a copy of the first  *
 *    hit so the scenario list can still be built.                                               *
 *                                                                                             *
 * INPUT:   magic      -- Byte sequence to locate.                                              *
 *          magic_len  -- Length of magic.                                                      *
 *          out_size   -- Receives the matching member's size on success.                        *
 *                                                                                             *
 * OUTPUT:  char*; Heap-allocated copy of the matching member (caller delete[]s it), or NULL.    *
 ***********************************************************************************************/
char * MixFileClass::Find_Member_Containing(char const * magic, int magic_len, int * out_size)
{
	if (magic == NULL || magic_len <= 0 || out_size == NULL) {
		return(NULL);
	}
	*out_size = 0;

	char probe[8192];

	MixFileClass * ptr = List.First();
	while (ptr->Is_Valid()) {
		if (ptr->HeaderBuffer != NULL && ptr->Count > 0) {
			for (int i = 0; i < ptr->Count; i++) {
				/*
				**	The marker lives near the top of an INI index, so only the leading bytes
				**	need scanning. This keeps the movie mixfiles (hundreds of MB) out of play.
				*/
				int scan = ptr->HeaderBuffer[i].Size;
				if (scan > (int)sizeof(probe)) scan = (int)sizeof(probe);
				if (scan < magic_len) continue;

				int got = ptr->Read_Member(i, probe, scan);
				if (got < magic_len) continue;

				bool found = false;
				for (int j = 0; j + magic_len <= got; j++) {
					if (memcmp(probe + j, magic, magic_len) == 0) {
						found = true;
						break;
					}
				}
				if (found) {
					int size = ptr->HeaderBuffer[i].Size;
					char * copy = new char[size];
					int full = ptr->Read_Member(i, copy, size);
					*out_size = full > 0 ? full : size;
					return(copy);
				}
			}
		}
		ptr = ptr->Next();
	}
	return(NULL);
}
