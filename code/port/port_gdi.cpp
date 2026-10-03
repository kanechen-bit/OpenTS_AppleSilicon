/*******************************************************************************
 *                                O P E N T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

/* Minimal in-process GDI. See port_gdi.h for what this covers and why.
 *
 * The shim types (BITMAP, BITMAPINFOHEADER, DIBSECTION) are used directly so
 * that a GetObject reply has exactly the layout the caller's DIBSECTION has.
 * They arrive through the port's force-include: port_early.h pulls in
 * windows_stub.h before this file's own includes are read. */

#include "port_gdi.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

struct BitmapEntry {
	void * handle;
	int    width;
	int    height;      /* Positive; rows are always stored top row first. */
	int    bitcount;
	int    stride;      /* Bytes per row, aligned per the creating API. */
	void * bits;
};

struct DcEntry {
	void * handle;
	void * selected;
};

/* The engine builds a handful of surfaces and one cursor, so fixed tables are
 * enough and keep this file clear of anything the shim's min/max macros could
 * rewrite. */
constexpr int MAX_BITMAPS = 64;
constexpr int MAX_DCS = 16;

BitmapEntry g_Bitmaps[MAX_BITMAPS];
DcEntry g_DCs[MAX_DCS];

std::uintptr_t g_NextHandle = 0x4000;

/* Handles are counters rather than addresses, so a stale or bogus one fails the
 * lookup below instead of being dereferenced. */
void * Alloc_Handle(void)
{
	g_NextHandle += 16;
	return reinterpret_cast<void *>(g_NextHandle);
}

/* A DIB section is a device independent bitmap, so its rows are padded out to
 * the next four byte boundary. */
int Dib_Row_Stride(int width, int bitcount)
{
	return ((width * bitcount + 31) / 32) * 4;
}

/* A bitmap made by CreateBitmap is device dependent, and GDI pads its rows only
 * to the next word. The engine sizes its 1bpp cursor mask with this rule, so the
 * two have to agree or the copy below reads past the caller's buffer. */
int Bitmap_Row_Stride(int width, int bitcount)
{
	return ((width * bitcount + 15) / 16) * 2;
}

BitmapEntry * Find_Bitmap(void * handle)
{
	if (handle == nullptr) {
		return nullptr;
	}
	for (int i = 0; i < MAX_BITMAPS; ++i) {
		if (g_Bitmaps[i].handle == handle) {
			return &g_Bitmaps[i];
		}
	}
	return nullptr;
}

DcEntry * Find_Dc(void * handle)
{
	if (handle == nullptr) {
		return nullptr;
	}
	for (int i = 0; i < MAX_DCS; ++i) {
		if (g_DCs[i].handle == handle) {
			return &g_DCs[i];
		}
	}
	return nullptr;
}

bool Bit_Count_Is_Supported(int bitcount)
{
	return bitcount == 1 || bitcount == 8 || bitcount == 16 || bitcount == 24 || bitcount == 32;
}

void * Create_Bitmap_Entry(int width, int height, int bitcount, int stride, const void * source)
{
	if (width <= 0 || height <= 0 || !Bit_Count_Is_Supported(bitcount)) {
		return nullptr;
	}

	int slot = -1;
	for (int i = 0; i < MAX_BITMAPS; ++i) {
		if (g_Bitmaps[i].handle == nullptr) {
			slot = i;
			break;
		}
	}
	if (slot < 0) {
		return nullptr;
	}

	std::size_t const bytes = static_cast<std::size_t>(stride) * static_cast<std::size_t>(height);
	void * bits = std::calloc(1, bytes);
	if (bits == nullptr) {
		return nullptr;
	}

	if (source != nullptr) {
		/* The caller packs its rows to the same alignment this API reports, so a
		 * flat copy keeps the padding consistent. */
		std::memcpy(bits, source, bytes);
	}

	BitmapEntry & entry = g_Bitmaps[slot];
	entry.handle   = Alloc_Handle();
	entry.width    = width;
	entry.height   = height;
	entry.bitcount = bitcount;
	entry.stride   = stride;
	entry.bits     = bits;
	return entry.handle;
}

}  // namespace


extern "C" void * opents_gdi_create_dc(void)
{
	for (int i = 0; i < MAX_DCS; ++i) {
		if (g_DCs[i].handle == nullptr) {
			g_DCs[i].handle = Alloc_Handle();
			g_DCs[i].selected = nullptr;
			return g_DCs[i].handle;
		}
	}
	return nullptr;
}


extern "C" int opents_gdi_delete_dc(void * dc)
{
	DcEntry * entry = Find_Dc(dc);
	if (entry == nullptr) {
		return 0;
	}

	/* The selected bitmap belongs to whoever created it, so it is not freed here;
	 * the surface destructor deselects and deletes it itself. */
	entry->handle = nullptr;
	entry->selected = nullptr;
	return 1;
}


extern "C" void * opents_gdi_create_dib_section(int width, int height, int bitcount, void ** bits)
{
	void * handle = Create_Bitmap_Entry(width, height, bitcount, Dib_Row_Stride(width, bitcount), nullptr);

	if (handle != nullptr && bits != nullptr) {
		*bits = Find_Bitmap(handle)->bits;
	}
	return handle;
}


extern "C" void * opents_gdi_create_bitmap(int width, int height, int planes, int bitcount, const void * bits)
{
	(void)planes;
	return Create_Bitmap_Entry(width, height, bitcount, Bitmap_Row_Stride(width, bitcount), bits);
}


extern "C" int opents_gdi_delete_object(void * object)
{
	BitmapEntry * entry = Find_Bitmap(object);
	if (entry == nullptr) {
		/* Fonts, brushes and the stock objects are not tracked here, and freeing
		 * one of those is not an error. */
		return 0;
	}

	std::free(entry->bits);
	entry->bits = nullptr;
	entry->handle = nullptr;
	return 1;
}


extern "C" int opents_gdi_get_object(void * object, int size, void * out)
{
	BitmapEntry * entry = Find_Bitmap(object);
	if (entry == nullptr || out == nullptr) {
		return 0;
	}

	/* Callers ask either for a DIBSECTION (the surface code, which wants the row
	 * pitch) or a plain BITMAP. A buffer big enough for a DIBSECTION gets one,
	 * matching GDI's own precedence. */
	if (size >= static_cast<int>(sizeof(DIBSECTION))) {
		DIBSECTION * section = static_cast<DIBSECTION *>(out);
		std::memset(section, 0, sizeof(*section));

		section->dsBm.bmType       = 0;
		section->dsBm.bmWidth      = entry->width;
		section->dsBm.bmHeight     = entry->height;
		section->dsBm.bmWidthBytes = entry->stride;
		section->dsBm.bmPlanes     = 1;
		section->dsBm.bmBitsPixel  = static_cast<WORD>(entry->bitcount);
		section->dsBm.bmBits       = entry->bits;

		section->dsBmih.biSize        = sizeof(BITMAPINFOHEADER);
		section->dsBmih.biWidth       = entry->width;
		/* Negative height is the convention for a top-down bitmap, which is how
		 * these are stored. */
		section->dsBmih.biHeight      = -entry->height;
		section->dsBmih.biPlanes      = 1;
		section->dsBmih.biBitCount    = static_cast<WORD>(entry->bitcount);
		section->dsBmih.biCompression = BI_RGB;
		section->dsBmih.biSizeImage   = static_cast<DWORD>(entry->stride) * static_cast<DWORD>(entry->height);

		return static_cast<int>(sizeof(DIBSECTION));
	}

	if (size >= static_cast<int>(sizeof(BITMAP))) {
		BITMAP * bitmap = static_cast<BITMAP *>(out);
		std::memset(bitmap, 0, sizeof(*bitmap));

		bitmap->bmType       = 0;
		bitmap->bmWidth      = entry->width;
		bitmap->bmHeight     = entry->height;
		bitmap->bmWidthBytes = entry->stride;
		bitmap->bmPlanes     = 1;
		bitmap->bmBitsPixel  = static_cast<WORD>(entry->bitcount);
		bitmap->bmBits       = entry->bits;

		return static_cast<int>(sizeof(BITMAP));
	}

	return 0;
}


extern "C" int opents_gdi_stretch_blt(void * dest_dc, int dx, int dy, int dw, int dh,
                                      void * src_dc, int sx, int sy, int sw, int sh)
{
	DcEntry * dest = Find_Dc(dest_dc);
	DcEntry * src = Find_Dc(src_dc);
	if (dest == nullptr || src == nullptr) {
		return 0;
	}

	BitmapEntry * dbm = Find_Bitmap(dest->selected);
	BitmapEntry * sbm = Find_Bitmap(src->selected);
	if (dbm == nullptr || sbm == nullptr) {
		return 0;
	}

	if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) {
		return 0;
	}

	/* Every surface in the tree is 16bpp 565, so a stretch between two of them
	 * is a straight run of pixels. Another depth would need its own unpacking,
	 * and nothing asks for one. */
	if (dbm->bitcount != 16 || sbm->bitcount != 16) {
		return 0;
	}

	std::uint16_t * dpix = static_cast<std::uint16_t *>(dbm->bits);
	std::uint16_t const * spix = static_cast<std::uint16_t const *>(sbm->bits);
	if (dpix == nullptr || spix == nullptr) {
		return 0;
	}

	/* Rows are padded to a four byte boundary, so the pitch in pixels comes from
	 * the recorded stride rather than from the width. */
	int const dpitch = dbm->stride / 2;
	int const spitch = sbm->stride / 2;

	for (int row = 0; row < dh; ++row) {
		int dy0 = dy + row;
		if (dy0 < 0 || dy0 >= dbm->height) {
			continue;
		}
		int sy0 = sy + (row * sh) / dh;
		if (sy0 < 0 || sy0 >= sbm->height) {
			continue;
		}

		std::uint16_t * drow = dpix + static_cast<std::size_t>(dy0) * static_cast<std::size_t>(dpitch);
		std::uint16_t const * srow = spix + static_cast<std::size_t>(sy0) * static_cast<std::size_t>(spitch);

		for (int col = 0; col < dw; ++col) {
			int dx0 = dx + col;
			if (dx0 < 0 || dx0 >= dbm->width) {
				continue;
			}
			int sx0 = sx + (col * sw) / dw;
			if (sx0 < 0 || sx0 >= sbm->width) {
				continue;
			}
			drow[dx0] = srow[sx0];
		}
	}

	return 1;
}


extern "C" void * opents_gdi_select_object(void * dc, void * object)
{
	DcEntry * entry = Find_Dc(dc);
	if (entry == nullptr) {
		/* A context this layer never handed out. Report a previous object anyway,
		 * so callers that save it and select it back do not read the null as
		 * "there was nothing here". */
		return reinterpret_cast<void *>(1);
	}

	void * previous = entry->selected;
	if (Find_Bitmap(object) != nullptr) {
		entry->selected = object;
	}

	return (previous != nullptr) ? previous : reinterpret_cast<void *>(1);
}

/* --- cursors -------------------------------------------------------------- */

/* Provided by the Cocoa window layer (port_window.mm). */
extern "C" void opents_window_set_custom_cursor(const unsigned char * argb, int width, int height,
                                                int hotx, int hoty);

/* The engine packs a cursor shape's pixels into the color bitmap of an
 * ICONINFO and keeps transparency in the alpha channel (the mask bitmap is
 * all zero), so only the color bits are kept here. Pixels are stored as
 * A, R, G, B bytes, top row first, which is the layout the window layer's
 * CoreGraphics path reads. */
struct CursorObject {
	unsigned char * argb;
	int width;
	int height;
	int hotx;
	int hoty;
};

extern "C" void * opents_cursor_create(void * color_bitmap, int hotx, int hoty)
{
	BitmapEntry * entry = Find_Bitmap(color_bitmap);
	if (entry == nullptr || entry->bitcount != 32 || entry->width <= 0 || entry->height <= 0 ||
	    entry->bits == nullptr) {
		return nullptr;
	}

	CursorObject * cursor = new (std::nothrow) CursorObject();
	if (cursor == nullptr) {
		return nullptr;
	}

	std::size_t const bytes = static_cast<std::size_t>(entry->width) *
	                          static_cast<std::size_t>(entry->height) * 4;
	cursor->argb = static_cast<unsigned char *>(std::malloc(bytes));
	if (cursor->argb == nullptr) {
		delete cursor;
		return nullptr;
	}

	/* The DIB stores little-endian DWORDs: B, G, R, A per pixel. The window
	 * layer wants A, R, G, B, so the channels are reshuffled once here. */
	unsigned char const * source = static_cast<unsigned char const *>(entry->bits);
	for (std::size_t i = 0; i < bytes; i += 4) {
		cursor->argb[i + 0] = source[i + 3];
		cursor->argb[i + 1] = source[i + 2];
		cursor->argb[i + 2] = source[i + 1];
		cursor->argb[i + 3] = source[i + 0];
	}

	cursor->width = entry->width;
	cursor->height = entry->height;
	cursor->hotx = hotx;
	cursor->hoty = hoty;
	return cursor;
}

extern "C" void opents_cursor_destroy(void * cursor)
{
	CursorObject * object = static_cast<CursorObject *>(cursor);
	if (object == nullptr) {
		return;
	}
	std::free(object->argb);
	delete object;
}

extern "C" void opents_cursor_apply(void * cursor)
{
	CursorObject * object = static_cast<CursorObject *>(cursor);
	if (object == nullptr) {
		opents_window_set_custom_cursor(nullptr, 0, 0, 0, 0);
		return;
	}
	opents_window_set_custom_cursor(object->argb, object->width, object->height,
	                                object->hotx, object->hoty);
}
