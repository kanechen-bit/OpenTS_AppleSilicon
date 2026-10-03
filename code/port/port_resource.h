/*******************************************************************************
 *                                O P E N T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

/* Win32 resource access for the non-Windows shim.
 *
 * Every localized string and every dialog template the game uses lives in
 * Language.dll, and that file is a plain 32-bit PE image: its dialog templates
 * and its string tables are entries in a .rsrc section, exactly as on Windows.
 * There is no module loader here to ask, so this module reads the file itself --
 * it validates the PE headers, walks the resource directory tree, and hands back
 * the raw bytes. That is the whole job.
 *
 * The result is that LoadString and FindResource(..., RT_DIALOG) return what the
 * game shipped rather than nothing at all, which is what unblocks every
 * owner-draw dialog: the campaign picker, the mission list, the in-game menus.
 *
 * Read-only, and deliberately free of any dependency on the engine's own types:
 * the shim calls it from C++ and port_resource.cpp is compiled with the engine's
 * flags, but the surface here is plain C so that neither side has to agree on
 * anything else.
 *
 * Lifetime follows Win32. A module token stays valid until it is closed, and the
 * bytes a FindResource handle points at stay valid for as long as the module
 * does -- the engine relies on that, because Fetch_Resource returns the pointer
 * and never releases it.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Opens a Win32 resource module by file name, e.g. "Language.dll".
 *
 * The name is tried as given first, so an explicit path always wins, and then
 * relative to the game's data directory (so -DATADIR works) and to the
 * executable's own directory. A candidate is only accepted if it parses as a PE
 * image carrying a resource directory, so a name that resolves to something else
 * -- a real dylib, say -- reports failure rather than producing a token that
 * silently yields no resources.
 *
 * Returns an opaque token, or NULL when no candidate could be used. */
void * opents_resource_open_module(char const * name);

/* Releases a token and every handle that pointed into it. Safe on NULL. */
void   opents_resource_close_module(void * module);

/* True when the token came from opents_resource_open_module. The shim asks
 * before treating a handle as a loaded library, since a resource module has no
 * exports to resolve. */
int    opents_resource_is_module(void * module);

/* The file a token was opened from, or NULL. For diagnostics. */
char const * opents_resource_module_path(void * module);

/* FindResource.
 *
 * `name` and `type` follow Win32's MAKEINTRESOURCE convention: a value at or
 * below 0xFFFF is an integer identifier stored in the pointer itself, and
 * anything larger is a NUL-terminated name. Names compare case-insensitively,
 * as they do on Windows.
 *
 * Returns an opaque handle for LoadResource, or NULL when the resource is not
 * present. */
void * opents_resource_find(void * module, void const * name, void const * type);

/* LoadResource and LockResource collapsed into one call, since the port has no
 * module to keep the memory mapped and nothing to unlock. Reports the resource's
 * length through `size` when that is non-NULL. Returns NULL for a NULL handle. */
void const * opents_resource_lock(void * found, unsigned long * size);

/* SizeofResource. Zero for a NULL handle. */
unsigned long opents_resource_size(void * found);

/* The length of the bytes a lock handed out, looked up by address.
 *
 * Needed because the engine keeps only the pointer: Fetch_Resource returns it
 * and never mentions the handle again, so CreateDialogIndirectParam is handed a
 * DLGTEMPLATE with no length attached. Zero when the address is not one this
 * module produced. */
unsigned long opents_resource_size_of(void const * bytes);

/* LoadString.
 *
 * Writes the text held for `id` into `buffer` and returns how many characters
 * were copied, or 0 when the id holds nothing -- the same contract LoadString
 * has, which the engine tests against 0.
 *
 * The text is transcoded from the resource's UTF-16 to Windows-1252, because
 * that is what the engine expects a "narrow" Win32 API to produce: it runs the
 * result through UTF8::From_Windows_1252 itself. Code points the code page lacks
 * become '?'. */
int    opents_resource_load_string(void * module, unsigned int id, char * buffer, int size);

#ifdef __cplusplus
}
#endif
