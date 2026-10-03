/*******************************************************************************
 *                                O P E N T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

/* Win32 resource modules. See port_resource.h for why this exists.
 *
 * The file the game asks for is Language.dll: a PE32 image, built by the
 * original resource compiler, with one .rsrc section holding 74 dialog
 * templates and 49 string tables. Reading it is a matter of following the
 * documented PE structures -- DOS header, NT headers, the resource data
 * directory, and the three-level tree underneath it:
 *
 *     type (RT_DIALOG, RT_STRING, ...)
 *       name (the dialog or string-block id)
 *         language -> an IMAGE_RESOURCE_DATA_ENTRY giving an RVA and a length
 *
 * Offsets inside the tree are relative to the start of the resource directory
 * itself, not to the image, which is the one detail that is easy to get wrong.
 *
 * All field reads are bounds-checked and little-endian explicit. Nothing here
 * trusts a length out of the file: a truncated or malformed image reports
 * failure rather than reading out of bounds, which matters because the real
 * resource loader has no such scruples and the strings come from a file the
 * player can replace.
 */

#include "port_resource.h"

#include "gamedirs.h"   /* Data_Directory -- so -DATADIR finds Language.dll too */
#include "utf8.h"       /* UTF8::To_Windows_1252, the engine's own code page table */

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#include <strings.h>   /* strncasecmp */


namespace {

/* ---- PE constants ------------------------------------------------------- */

constexpr uint16_t kMzSignature   = 0x5A4D;      /* 'MZ' */
constexpr uint32_t kPeSignature   = 0x00004550;  /* 'PE\0\0' */
constexpr uint16_t kOptionalPe32  = 0x010B;
constexpr uint16_t kOptionalPe32P = 0x020B;

/* The resource directory is the third entry of the data directory array. */
constexpr uint32_t kResourceDirectoryIndex = 2;

/* Data directories begin at these offsets inside the optional header. */
constexpr uint32_t kPe32DataDirectoryOffset  = 96;
constexpr uint32_t kPe32PDataDirectoryOffset = 112;
constexpr uint32_t kPe32DirectoryCountOffset  = 92;
constexpr uint32_t kPe32PDirectoryCountOffset = 108;

/* A field's top bit distinguishes an offset from an identifier. */
constexpr uint32_t kNameIsString = 0x80000000u;
constexpr uint32_t kSubIsDirectory = 0x80000000u;

/* Win32 stores an identifier in the pointer itself, so anything at or below
 * this is an id and anything above it is a real string address. */
constexpr uintptr_t kLargestIdentifier = 0xFFFF;

/* Resource type ordinals, as Win32's RT_* macros evaluate to. */
constexpr unsigned kTypeDialog = 5;
constexpr unsigned kTypeString = 6;

/* A string table holds sixteen strings, numbered from one. */
constexpr unsigned kStringsPerBlock = 16;

/* No resource name in this game is long; this bounds the name reader. */
constexpr unsigned kLongestResourceName = 512;


/* ---- Little-endian reads ------------------------------------------------ */

uint16_t Read_U16(uint8_t const * p)
{
	return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

uint32_t Read_U32(uint8_t const * p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}


/* ---- Structures --------------------------------------------------------- */

struct Section {
	uint32_t rva;
	uint32_t virtual_size;
	uint32_t raw;
	uint32_t raw_size;
};

struct Module {
	std::string          path;
	std::vector<uint8_t> bytes;
	std::vector<Section> sections;
	uint32_t             resource_rva  = 0;
	uint32_t             resource_off  = 0;   /* File offset of the resource directory. */
	uint32_t             resource_size = 0;
};

/* One directory entry, with its raw fields already decoded. */
struct DirectoryEntry {
	uint32_t id        = 0;      /* Set when the entry is identified by number. */
	uint32_t string_at = 0;      /* Offset of the name, relative to the directory. */
	uint32_t sub       = 0;      /* Offset of the child, relative to the directory. */
	bool     named     = false;  /* The entry is identified by name, not number. */
	bool     is_directory = false; /* The child is another directory, not a leaf. */
};

struct Found {
	uint8_t const * data = nullptr;
	unsigned long   size = 0;
};

/* Modules and handles are kept alive for the process, matching Win32's rule that
 * resource memory stays mapped while the module is loaded. */
std::vector<Module *> g_Modules;
std::vector<Found *>  g_Found;


/* ---- Bounds-checked field access ---------------------------------------- */

bool Get_U16(Module const & m, uint32_t at, uint16_t * out)
{
	if ((size_t)at + 2 > m.bytes.size()) {
		return(false);
	}
	*out = Read_U16(m.bytes.data() + at);
	return(true);
}

bool Get_U32(Module const & m, uint32_t at, uint32_t * out)
{
	if ((size_t)at + 4 > m.bytes.size()) {
		return(false);
	}
	*out = Read_U32(m.bytes.data() + at);
	return(true);
}

/* Translates an image RVA into a file offset. A resource never lives in a
 * section's zero-filled tail, so a hit there reports failure. */
bool Rva_To_Offset(Module const & m, uint32_t rva, uint32_t * out)
{
	for (Section const & s : m.sections) {
		if (rva < s.rva || rva >= s.rva + s.virtual_size) {
			continue;
		}
		uint32_t const delta = rva - s.rva;
		if (delta >= s.raw_size) {
			return(false);
		}
		uint32_t const at = s.raw + delta;
		if ((size_t)at >= m.bytes.size()) {
			return(false);
		}
		*out = at;
		return(true);
	}
	return(false);
}


/* ---- Header parsing ----------------------------------------------------- */

/* Validates the PE headers and records where the sections and the resource
 * directory are. Reports failure for anything that is not a readable image with
 * resources -- including a Mach-O library that happens to share the name. */
bool Parse_Image(Module & m)
{
	uint8_t const * d = m.bytes.data();
	size_t const     n = m.bytes.size();

	if (n < 0x40 || Read_U16(d) != kMzSignature) {
		return(false);
	}

	uint32_t const pe_at = Read_U32(d + 0x3C);
	if ((size_t)pe_at + 24 > n || Read_U32(d + pe_at) != kPeSignature) {
		return(false);
	}

	uint16_t const section_count = Read_U16(d + pe_at + 6);
	uint16_t const optional_size = Read_U16(d + pe_at + 20);
	uint32_t const optional_at   = pe_at + 24;
	if ((size_t)optional_at + 2 > n) {
		return(false);
	}

	uint32_t directory_at;
	uint32_t directory_count_at;
	switch (Read_U16(d + optional_at)) {
		case kOptionalPe32:
			directory_at       = optional_at + kPe32DataDirectoryOffset;
			directory_count_at = optional_at + kPe32DirectoryCountOffset;
			break;

		case kOptionalPe32P:
			directory_at       = optional_at + kPe32PDataDirectoryOffset;
			directory_count_at = optional_at + kPe32PDirectoryCountOffset;
			break;

		default:
			return(false);
	}

	/* The image has to claim enough directories to carry a resource one. */
	uint32_t directory_count;
	if (!Get_U32(m, directory_count_at, &directory_count) || directory_count <= kResourceDirectoryIndex) {
		return(false);
	}

	uint32_t const resource_entry = directory_at + kResourceDirectoryIndex * 8;
	if (!Get_U32(m, resource_entry, &m.resource_rva) || !Get_U32(m, resource_entry + 4, &m.resource_size)) {
		return(false);
	}
	if (m.resource_rva == 0 || m.resource_size == 0) {
		return(false);
	}

	/* Section table, which is what turns the resource RVA into a file offset. */
	uint32_t const sections_at = optional_at + optional_size;
	for (uint16_t i = 0; i < section_count; i++) {
		uint32_t const at = sections_at + (uint32_t)i * 40;

		Section section;
		if (!Get_U32(m, at + 8, &section.virtual_size) ||
		    !Get_U32(m, at + 12, &section.rva) ||
		    !Get_U32(m, at + 16, &section.raw_size) ||
		    !Get_U32(m, at + 20, &section.raw)) {
			return(false);
		}
		m.sections.push_back(section);
	}

	if (!Rva_To_Offset(m, m.resource_rva, &m.resource_off)) {
		return(false);
	}
	return((size_t)m.resource_off + m.resource_size <= n);
}


/* ---- Walking the resource tree ------------------------------------------ */

/* Reads the entry count of the directory whose offset (relative to the resource
 * directory) is `dir_at`. */
bool Read_Directory(Module const & m, uint32_t dir_at, uint16_t * named, uint16_t * numbered)
{
	uint32_t const at = m.resource_off + dir_at;
	if ((size_t)at + 16 > m.bytes.size()) {
		return(false);
	}
	uint8_t const * p = m.bytes.data() + at;
	*named    = Read_U16(p + 12);
	*numbered = Read_U16(p + 14);
	return(true);
}

bool Read_Directory_Entry(Module const & m, uint32_t dir_at, unsigned index, DirectoryEntry * out)
{
	uint32_t const at = m.resource_off + dir_at + 16 + index * 8;
	if ((size_t)at + 8 > m.bytes.size()) {
		return(false);
	}
	uint8_t const * p = m.bytes.data() + at;

	uint32_t const name = Read_U32(p);
	uint32_t const sub  = Read_U32(p + 4);

	out->named        = (name & kNameIsString) != 0;
	out->string_at    = name & ~kNameIsString;
	out->id           = out->named ? 0 : name;
	out->is_directory = (sub & kSubIsDirectory) != 0;
	out->sub          = sub & ~kSubIsDirectory;
	return(true);
}

/* A resource name is stored as a length-prefixed run of UTF-16 units. Every
 * resource name here is ASCII, so the code units are narrowed directly. */
bool Read_Directory_Name(Module const & m, uint32_t string_at, std::string * out)
{
	uint16_t length;
	if (!Get_U16(m, m.resource_off + string_at, &length) || length > kLongestResourceName) {
		return(false);
	}

	uint32_t const at = m.resource_off + string_at + 2;
	if ((size_t)at + (size_t)length * 2 > m.bytes.size()) {
		return(false);
	}

	out->clear();
	for (uint16_t i = 0; i < length; i++) {
		out->push_back((char)Read_U16(m.bytes.data() + at + (uint32_t)i * 2));
	}
	return(true);
}

/* Win32 resource names are compared without regard to case. */
bool Same_Name(std::string const & stored, char const * wanted)
{
	size_t i = 0;
	for (; i < stored.size() && wanted[i] != '\0'; i++) {
		char a = stored[i];
		char b = wanted[i];
		if (a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
		if (b >= 'a' && b <= 'z') b = (char)(b - 'a' + 'A');
		if (a != b) {
			return(false);
		}
	}
	return(i == stored.size() && wanted[i] == '\0');
}

/* Finds one child of a directory. Pass `wanted_name` as NULL to look up an
 * identifier instead; exactly one of the two is used. */
bool Find_Child(Module const & m, uint32_t dir_at, uint32_t wanted_id, char const * wanted_name, DirectoryEntry * out)
{
	uint16_t named = 0;
	uint16_t numbered = 0;
	if (!Read_Directory(m, dir_at, &named, &numbered)) {
		return(false);
	}

	unsigned const total = (unsigned)named + (unsigned)numbered;
	for (unsigned i = 0; i < total; i++) {
		DirectoryEntry entry;
		if (!Read_Directory_Entry(m, dir_at, i, &entry)) {
			return(false);
		}

		if (wanted_name != nullptr) {
			if (!entry.named) {
				continue;
			}
			std::string stored;
			if (Read_Directory_Name(m, entry.string_at, &stored) && Same_Name(stored, wanted_name)) {
				*out = entry;
				return(true);
			}
		} else {
			if (entry.named || entry.id != wanted_id) {
				continue;
			}
			*out = entry;
			return(true);
		}
	}
	return(false);
}

/* type -> name -> language, ending at the IMAGE_RESOURCE_DATA_ENTRY that names
 * the bytes. The game ships one language per file, so the first language leaf is
 * the answer; Win32 would choose by thread locale. */
bool Find_Resource(Module const & m,
                   uint32_t type_id, char const * type_name,
                   uint32_t name_id, char const * name_name,
                   uint32_t * out_rva, uint32_t * out_size)
{
	DirectoryEntry type_entry;
	if (!Find_Child(m, 0, type_id, type_name, &type_entry) || !type_entry.is_directory) {
		return(false);
	}

	DirectoryEntry name_entry;
	if (!Find_Child(m, type_entry.sub, name_id, name_name, &name_entry) || !name_entry.is_directory) {
		return(false);
	}

	uint16_t named = 0;
	uint16_t numbered = 0;
	if (!Read_Directory(m, name_entry.sub, &named, &numbered)) {
		return(false);
	}

	unsigned const total = (unsigned)named + (unsigned)numbered;
	for (unsigned i = 0; i < total; i++) {
		DirectoryEntry language;
		if (!Read_Directory_Entry(m, name_entry.sub, i, &language) || language.is_directory) {
			continue;
		}

		uint32_t const at = m.resource_off + language.sub;
		if ((size_t)at + 16 > m.bytes.size()) {
			continue;
		}
		uint8_t const * p = m.bytes.data() + at;
		*out_rva  = Read_U32(p);      /* IMAGE_RESOURCE_DATA_ENTRY.OffsetToData */
		*out_size = Read_U32(p + 4);  /* .Size */
		return(true);
	}
	return(false);
}

/* Points a handle at a resource's bytes, or reports failure. */
void * Make_Handle(Module & m, uint32_t rva, uint32_t size)
{
	uint32_t offset;
	if (!Rva_To_Offset(m, rva, &offset) || (size_t)offset + size > m.bytes.size()) {
		return(nullptr);
	}

	Found * found = new Found;
	found->data = m.bytes.data() + offset;
	found->size = size;
	g_Found.push_back(found);
	return(found);
}


/* ---- Tokens ------------------------------------------------------------- */

Module * As_Module(void * token)
{
	if (token == nullptr) {
		return(nullptr);
	}
	auto const at = std::find(g_Modules.begin(), g_Modules.end(), (Module *)token);
	return((at != g_Modules.end()) ? (Module *)token : nullptr);
}

/* Decodes a Win32 resource name argument. Rejects NULL outright rather than
 * letting it collapse to identifier zero and match something. */
bool Resolve_Argument(void const * value, uint32_t * id, char const ** name)
{
	if (value == nullptr) {
		return(false);
	}
	uintptr_t const raw = (uintptr_t)value;
	if (raw <= kLargestIdentifier) {
		*id   = (uint32_t)raw;
		*name = nullptr;
	} else {
		*id   = 0;
		*name = (char const *)value;
	}
	return(true);
}


/* ---- Locating the file -------------------------------------------------- */

bool Read_Whole_File(std::string const & path, std::vector<uint8_t> * out)
{
	std::FILE * file = std::fopen(path.c_str(), "rb");
	if (file == nullptr) {
		return(false);
	}

	std::fseek(file, 0, SEEK_END);
	long const length = std::ftell(file);
	if (length <= 0) {
		std::fclose(file);
		return(false);
	}
	std::rewind(file);

	out->resize((size_t)length);
	size_t const got = std::fread(out->data(), 1, (size_t)length, file);
	std::fclose(file);

	if (got != (size_t)length) {
		out->clear();
		return(false);
	}
	return(true);
}

/* The directory the running executable sits in, with a trailing separator, or
 * empty when it cannot be determined. Resolved through the loader rather than
 * argv[0], which is empty when WinMain is reached through port_main. */
std::string Executable_Directory(void)
{
	char path[PATH_MAX];

#if defined(__APPLE__)
	uint32_t size = (uint32_t)sizeof(path);
	if (_NSGetExecutablePath(path, &size) != 0) {
		return(std::string());
	}
	char resolved[PATH_MAX];
	char const * final_path = (realpath(path, resolved) != nullptr) ? resolved : path;
#elif defined(__linux__)
	ssize_t const got = readlink("/proc/self/exe", path, sizeof(path) - 1);
	if (got <= 0) {
		return(std::string());
	}
	path[got] = '\0';
	char const * final_path = path;
#else
	return(std::string());
#endif

	std::string text(final_path);
	size_t const cut = text.find_last_of('/');
	return((cut == std::string::npos) ? std::string() : text.substr(0, cut + 1));
}

/*
 * The deployment's data directory, resolved without depending on initialisation
 * order.
 *
 * Data_Directory() answers this once the engine has parsed its options, but the
 * engine asks for Language.dll *before* that happens: a global initialiser in
 * the map generator fetches a string, which reaches LoadLibrary. On Win32 the
 * order never mattered, because Language.dll sits in the application directory
 * and the loader finds it without being told where to look. A deployment that
 * keeps its game data somewhere else -- which is what the port's -DATADIR
 * switch describes -- has to have that switch honoured before main runs, and
 * the only place that can happen is the point of use.
 *
 * So the switch is read from the process arguments when the engine's own value
 * is not yet set. Once it is set that value wins, so an early load and a later
 * one look in the same place.
 */
std::string Deployment_Data_Directory(void)
{
	std::string const applied = Data_Directory();   /* Empty, or ends in a separator. */
	if (!applied.empty()) {
		return(applied);
	}

	std::vector<std::string> arguments;

#if defined(__APPLE__)
	int const count = *_NSGetArgc();
	char ** const list = *_NSGetArgv();
	for (int index = 0; index < count; index++) {
		if (list[index] != nullptr) {
			arguments.push_back(list[index]);
		}
	}
#elif defined(__linux__)
	std::FILE * file = std::fopen("/proc/self/cmdline", "rb");
	if (file != nullptr) {
		std::string whole;
		char chunk[512];
		size_t got = 0;
		while ((got = std::fread(chunk, 1, sizeof(chunk), file)) > 0) {
			whole.append(chunk, got);
		}
		std::fclose(file);
		size_t at = 0;
		while (at < whole.size()) {
			size_t const end = whole.find('\0', at);
			if (end == std::string::npos) break;
			arguments.push_back(whole.substr(at, end - at));
			at = end + 1;
		}
	}
#endif

	static char const kSwitch[] = "-DATADIR=";
	for (std::string const & argument : arguments) {
		if (argument.size() >= sizeof(kSwitch) - 1 &&
		    strncasecmp(argument.c_str(), kSwitch, sizeof(kSwitch) - 1) == 0) {
			std::string path(argument.c_str() + sizeof(kSwitch) - 1);
			while (!path.empty() && path.back() == '/') {
				path.pop_back();
			}
			return(path.empty() ? std::string() : path + "/");
		}
	}

	return(std::string());
}

}  // namespace


/* ---- Public interface --------------------------------------------------- */

void * opents_resource_open_module(char const * name)
{
	if (name == nullptr || *name == '\0') {
		return(nullptr);
	}

	/* An explicit path is tried first so a caller can point at a specific file;
	 * absolute names are not re-based. */
	std::vector<std::string> candidates;
	candidates.push_back(name);

	bool const relative = (name[0] != '/');
	if (relative) {
		std::string const data = Deployment_Data_Directory();   /* Empty, or ends in a separator. */
		if (!data.empty()) {
			candidates.push_back(data + name);
		}

		std::string const exe = Executable_Directory();
		if (!exe.empty()) {
			candidates.push_back(exe + name);
		}
	}

	for (std::string const & candidate : candidates) {
		Module * module = new Module;
		module->path = candidate;

		if (Read_Whole_File(candidate, &module->bytes) && Parse_Image(*module)) {
			OPENTS_IF_IO_TRACE std::fprintf(stderr, "[PORTRES] module %s -> %s (%zu bytes, resource dir at 0x%X)\n",
			             name, candidate.c_str(), module->bytes.size(), module->resource_off);
			std::fflush(stderr);
			g_Modules.push_back(module);
			return(module);
		}
		delete module;
	}

	OPENTS_IF_IO_TRACE std::fprintf(stderr, "[PORTRES] module %s not found as a PE image\n", name);
	std::fflush(stderr);
	return(nullptr);
}


void opents_resource_close_module(void * token)
{
	Module * module = As_Module(token);
	if (module == nullptr) {
		return;
	}

	/* Drop the handles pointing into these bytes before the bytes go away, so
	 * that a caller holding one gets a closed handle rather than a stale
	 * pointer. */
	uint8_t const * const base = module->bytes.data();
	uint8_t const * const end  = base + module->bytes.size();

	for (size_t i = 0; i < g_Found.size(); ) {
		if (g_Found[i]->data >= base && g_Found[i]->data < end) {
			delete g_Found[i];
			g_Found.erase(g_Found.begin() + (long)i);
		} else {
			i++;
		}
	}

	g_Modules.erase(std::find(g_Modules.begin(), g_Modules.end(), module));
	delete module;
}


int opents_resource_is_module(void * token)
{
	return(As_Module(token) != nullptr ? 1 : 0);
}


char const * opents_resource_module_path(void * token)
{
	Module * module = As_Module(token);
	return(module != nullptr ? module->path.c_str() : nullptr);
}


void * opents_resource_find(void * module_token, void const * name, void const * type)
{
	Module * module = As_Module(module_token);
	if (module == nullptr) {
		return(nullptr);
	}

	uint32_t type_id = 0;
	char const * type_name = nullptr;
	if (!Resolve_Argument(type, &type_id, &type_name)) {
		return(nullptr);
	}

	uint32_t name_id = 0;
	char const * name_name = nullptr;
	if (!Resolve_Argument(name, &name_id, &name_name)) {
		return(nullptr);
	}

	uint32_t rva = 0;
	uint32_t size = 0;
	if (!Find_Resource(*module, type_id, type_name, name_id, name_name, &rva, &size)) {
		return(nullptr);
	}
	return(Make_Handle(*module, rva, size));
}


/*
**  Bytes handed out are remembered by address.
**
**  Win32 keeps the size with the handle, and callers that need it pass the
**  handle. The engine's Fetch_Resource keeps only the pointer, so
**  CreateDialogIndirectParam later receives a template with no length
**  attached -- and a DLGTEMPLATE cannot be parsed safely without one. This
**  table supplies the missing length from the address, which is enough
**  because the bytes live as long as the module does.
*/
namespace {

struct ByteRange {
	uint8_t const * data = nullptr;
	unsigned long   size = 0;
};

std::vector<ByteRange> g_Ranges;

void Remember_Bytes(void const * data, unsigned long size)
{
	if (data == nullptr) {
		return;
	}
	uint8_t const * bytes = (uint8_t const *)data;
	for (ByteRange const & range : g_Ranges) {
		if (range.data == bytes) {
			return;   /* already known */
		}
	}
	g_Ranges.push_back(ByteRange{bytes, size});
}

}  // namespace


extern "C" unsigned long opents_resource_size_of(void const * bytes)
{
	if (bytes == nullptr) {
		return(0);
	}
	for (ByteRange const & range : g_Ranges) {
		if (range.data == bytes) {
			return(range.size);
		}
	}
	return(0);
}


void const * opents_resource_lock(void * handle, unsigned long * size)
{
	Found * found = (Found *)handle;
	if (found == nullptr) {
		return(nullptr);
	}
	if (size != nullptr) {
		*size = found->size;
	}
	Remember_Bytes(found->data, found->size);
	return(found->data);
}


unsigned long opents_resource_size(void * handle)
{
	Found * found = (Found *)handle;
	return(found != nullptr ? found->size : 0);
}


int opents_resource_load_string(void * module_token, unsigned int id, char * buffer, int size)
{
	if (buffer == nullptr || size <= 0) {
		return(0);
	}
	buffer[0] = '\0';

	Module * module = As_Module(module_token);
	if (module == nullptr) {
		return(0);
	}

	/* The tables are numbered from one and hold sixteen strings each. */
	uint32_t const block = (id / kStringsPerBlock) + 1;
	unsigned const index = id % kStringsPerBlock;

	uint32_t rva = 0;
	uint32_t length = 0;
	if (!Find_Resource(*module, kTypeString, nullptr, block, nullptr, &rva, &length)) {
		return(0);
	}

	uint32_t offset;
	if (!Rva_To_Offset(*module, rva, &offset) || (size_t)offset + length > module->bytes.size()) {
		return(0);
	}

	/*
	**	The block interleaves each length with the text that follows it --
	**	length, text, length, text -- rather than grouping the sixteen lengths
	**	first as the older documentation describes. That layout is not a guess:
	**	it is the only one that consumes all 196 string blocks across the four
	**	shipped language files exactly, and the text it yields matches the
	**	resource script the port carries character for character.
	*/
	uint8_t const * blob = module->bytes.data() + offset;
	size_t at = 0;

	for (unsigned i = 0; i <= index; i++) {
		if (at + 2 > length) {
			return(0);
		}
		unsigned const units = Read_U16(blob + at);
		at += 2;

		if (i == index) {
			if (units == 0 || at + (size_t)units * 2 > length) {
				return(0);
			}

			/* UTF-16 to UTF-8 to the code page the engine expects back from a
			 * narrow API, using the engine's own table so the two agree. */
			std::string utf8;
			utf8.reserve(units);
			for (unsigned unit = 0; unit < units; unit++) {
				char32_t code = Read_U16(blob + at + (size_t)unit * 2);

				/* Join a surrogate pair when one follows. */
				if (code >= 0xD800 && code <= 0xDBFF && unit + 1 < units) {
					char32_t const low = Read_U16(blob + at + (size_t)(unit + 1) * 2);
					if (low >= 0xDC00 && low <= 0xDFFF) {
						code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
						unit++;
					}
				}

				char encoded[UTF8::MAX_SEQUENCE];
				utf8.append(encoded, (size_t)UTF8::Encode(code, encoded));
			}

			std::string const ansi = UTF8::To_Windows_1252(utf8);

			int count = (int)ansi.size();
			if (count > size - 1) {
				count = size - 1;
			}
			std::memcpy(buffer, ansi.data(), (size_t)count);
			buffer[count] = '\0';
			return(count);
		}

		at += (size_t)units * 2;
	}
	return(0);
}
