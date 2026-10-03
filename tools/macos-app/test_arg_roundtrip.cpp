/*
 * The command line round trip, on its own.
 *
 * opents_port_command_line() joins the platform's argv back into one string and
 * CommandLineToArgvW() splits it apart again, which is how the engine's
 * -DATADIR= and -USERDIR= switches reach it. A directory whose name contains a
 * space used to come apart at the space, so this checks that it survives.
 *
 * Built and run by the macos-app bundle test; not part of the CMake build.
 */

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <crt_externs.h>

/* The two functions under test, lifted from windows_stub.h by including it the
 * same way the engine does: force-included into a plain C++ translation unit. */
#include "port_early.h"
#include "windows_stub.h"

static int failures = 0;

static void Expect(int condition, char const * what, std::string const & detail)
{
	printf("%s  %s%s%s\n", condition ? "ok  " : "FAIL", what,
		   detail.empty() ? "" : "  ->  ", detail.c_str());

	if (!condition) {
		failures++;
	}
}


static std::vector<std::string> Round_Trip(char const * line)
{
	int const chars = MultiByteToWideChar(CP_ACP, 0, line, -1, nullptr, 0);
	std::wstring wide(chars > 0 ? chars - 1 : 0, L'\0');

	if (chars > 1) {
		MultiByteToWideChar(CP_ACP, 0, line, -1, wide.data(), chars);
	}

	int count = 0;
	LPWSTR * parsed = CommandLineToArgvW(wide.c_str(), &count);
	std::vector<std::string> out;

	if (parsed == nullptr) {
		return out;
	}

	for (int index = 0; index < count; index++) {
		int const length = WideCharToMultiByte(CP_ACP, 0, parsed[index], -1, nullptr, 0, nullptr, nullptr);
		std::string value(length > 0 ? length - 1 : 0, '\0');

		if (length > 1) {
			WideCharToMultiByte(CP_ACP, 0, parsed[index], -1, value.data(), length, nullptr, nullptr);
		}

		out.push_back(value);
	}

	LocalFree(parsed);
	return out;
}


int main(void)
{
	printf("== CommandLineToArgvW ==\n");

	{
		auto parts = Round_Trip("/bin/Game -XC");
		Expect(parts.size() == 2, "plain options split in two", "");
		Expect(parts.size() == 2 && parts[1] == "-XC", "plain option survives", parts.size() > 1 ? parts[1] : "");
	}

	{
		auto parts = Round_Trip("/bin/Game \"-USERDIR=/Users/me/Library/Application Support/OpenTS\"");
		Expect(parts.size() == 2, "quoted path with a space is one argument",
			   parts.size() > 1 ? parts[1] : "");
		Expect(parts.size() == 2 && parts[1] == "-USERDIR=/Users/me/Library/Application Support/OpenTS",
			   "quoted path arrives intact", parts.size() > 1 ? parts[1] : "");
	}

	{
		auto parts = Round_Trip("/bin/Game -DATADIR=/tmp/ts -USERDIR=/tmp/u");
		Expect(parts.size() == 3, "two switches are two arguments", "");
	}

	{
		// What opents_port_command_line() writes for the real user directory.
		auto parts = Round_Trip("\"/Applications/OpenTS.app/Contents/MacOS/OpenTS-engine\" "
								"\"-USERDIR=/Users/me/Library/Application Support/OpenTS\"");
		Expect(parts.size() == 2 && parts[1] == "-USERDIR=/Users/me/Library/Application Support/OpenTS",
			   "executable and user directory both quoted", parts.size() > 1 ? parts[1] : "");
	}

	{
		auto parts = Round_Trip("/bin/Game \"\"");
		Expect(parts.size() == 2 && parts[1].empty(), "an empty quoted argument is kept", "");
	}

	{
		auto parts = Round_Trip("/bin/Game   -XC    -480  ");
		Expect(parts.size() == 3, "runs of whitespace collapse", "");
	}

	{
		auto parts = Round_Trip("/bin/Game");
		Expect(parts.size() == 1, "a bare executable is one argument", "");
	}

	printf("\n== opents_port_command_line ==\n");
	{
		// The real argv of this test process, joined and split back.
		int const argc = *_NSGetArgc();
		char ** const argv = *_NSGetArgv();

		std::string joined;
		for (int index = 0; index < argc; index++) {
			if (index != 0) joined += ' ';
			joined += argv[index] != nullptr ? argv[index] : "";
		}

		auto parts = Round_Trip(joined.c_str());
		Expect(parts.size() == (size_t)argc, "argv survives the round trip",
			   joined);
	}

	printf("\n%s (%d failure%s)\n", failures == 0 ? "PASS" : "FAIL",
		   failures, failures == 1 ? "" : "s");
	return failures == 0 ? 0 : 1;
}
