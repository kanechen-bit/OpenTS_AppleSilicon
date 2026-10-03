/*******************************************************************************
 *                                O P E N T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

/* Minimal in-process GDI: enough of the bitmap half of the API for the engine
 * to build its surfaces.
 *
 * The engine's whole renderer writes pixels straight into a 16bpp top-down DIB
 * section and hands the finished frame to the presenter, so what it actually
 * needs from GDI is a device context to own a bitmap, a bitmap that owns real
 * memory, and a way to read that memory's geometry back. Nothing here draws.
 *
 * The surface code (dsurface.cpp) and the cursor loader (wincursor.cpp) are the
 * only callers, which is why the emulation is this small.
 *
 * Handles are opaque non-null pointers; they are counted rather than derived
 * from real memory so that a bogus handle is rejected rather than dereferenced.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Creates a memory device context. `compatible_from` may be null, matching the
 * CreateCompatibleDC(NULL) the engine uses. Returns null only if out of memory. */
void * opents_gdi_create_dc(void);
int    opents_gdi_delete_dc(void * dc);

/* Creates a top-down device independent bitmap section, with GDI's four byte
 * row alignment, and reports the pixel storage through `bits`. `height` is
 * positive; the section is always stored top row first, which is what the
 * engine's negative-height request asks for. */
void * opents_gdi_create_dib_section(int width, int height, int bitcount, void ** bits);

/* Creates a plain bitmap. When `bits` is non-null the pixels are copied in,
 * padded to the four byte row alignment (that is how the cursor's 1bpp mask
 * arrives); when it is null the storage starts zeroed. */
void * opents_gdi_create_bitmap(int width, int height, int planes, int bitcount, const void * bits);

/* Frees a bitmap created above. Returns 1 on success, 0 for an unknown handle. */
int    opents_gdi_delete_object(void * object);

/* Reads a bitmap's geometry back. `out` is filled as a DIBSECTION when `size`
 * admits one, otherwise as a BITMAP -- the two cases the engine asks for.
 * Returns the number of bytes written, or 0 when `object` is not a bitmap. */
int    opents_gdi_get_object(void * object, int size, void * out);

/* Records `object` as the DC's selection and returns the previous one. The
 * engine only ever selects bitmaps, and only to free the previous one later, so
 * a non-bitmap object is accepted and simply not stored. */
void * opents_gdi_select_object(void * dc, void * object);

/* Stretches a rectangle of the source DC's selected bitmap into the destination
 * DC's selected bitmap.
 *
 * This is the one drawing primitive the engine actually leans on: a blit between
 * two surfaces whose rectangles differ in size is handed to GDI rather than to
 * the software blitter (see DSurface::Blit_From), which is how the scenario
 * dialog scales a map preview into its frame. Sampling is nearest neighbour,
 * matching the COLORONCOLOR mode the engine sets before every stretch.
 *
 * Returns 1 when both sides resolved to a bitmap and the copy was attempted,
 * 0 when a handle is unknown, either rectangle is degenerate, or the two
 * bitmaps are not both 16bpp. */
int    opents_gdi_stretch_blt(void * dest_dc, int dx, int dy, int dw, int dh,
                              void * src_dc, int sx, int sy, int sw, int sh);

/* --- cursors -------------------------------------------------------------- */

/* Creates a cursor object from a 32bpp top-down color bitmap made by
 * CreateDIBSection (BGRA bytes; the alpha channel carries transparency) plus
 * a hotspot measured from the bitmap's top-left corner. The engine's cursor
 * loader (wincursor.cpp) hands the color bitmap of an ICONINFO here.
 * Returns an opaque handle, or null if the bitmap is unusable. */
void * opents_cursor_create(void * color_bitmap, int hotx, int hoty);

/* Destroys a cursor made by opents_cursor_create. If the cursor is the one
 * currently shown, the caller is expected to apply another one (or clear)
 * afterwards. */
void   opents_cursor_destroy(void * cursor);

/* Makes `cursor` the pointer image shown over the game window, or, for a
 * null cursor, stops showing a custom one (the pointer then follows the
 * system's visibility counter). Cheap enough to call on every mouse move. */
void   opents_cursor_apply(void * cursor);

#ifdef __cplusplus
}
#endif
