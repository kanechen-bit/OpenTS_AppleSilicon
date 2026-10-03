/*******************************************************************************
 *                                O P E N  T S
 ******************************************************************************/
/* Port shim: the C entry point an unsupported non-Windows target needs.
 *
 * The engine's entry point is WinMain, declared CALLBACK and reached by the
 * Windows loader through the /SUBSYSTEM:WINDOWS entry stub, which supplies the
 * instance handle, the command line and the show command. None of that exists
 * here: the loader calls main, and there is no WinMain to call.
 *
 * So this file provides main and forwards to WinMain with the arguments a
 * minimal start would have had. It is the only place in the port that adds a
 * new translation unit rather than a header, because an entry point has to be a
 * real external symbol rather than an inline one.
 *
 * The whole file is empty unless the unsupported target is being configured, so
 * the supported Win32 build compiles exactly what it compiled before: an object
 * with no symbols in it. */

#ifdef OPENTS_EXPERIMENTAL_NONWIN32

#include <cstdlib>
#include <cstring>
#include <string>

// OpenTS diag probe (temporary, remove before clean build): crash backtrace.
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static void opents_port_crash_handler(int sig)
{
	// Re-arm nothing; just record where we died and get out.
	FILE *f = fopen("/tmp/opents_crash.txt", "w");
	if (f != NULL) {
		fprintf(f, "OpenTS crash: signal %d\n", sig);
		fflush(f);
		void *bt[64];
		int n = backtrace(bt, 64);
		backtrace_symbols_fd(bt, n, fileno(f));
		fclose(f);
	}
	_exit(99);
}

static void opents_port_install_crash_handler(void)
{
	signal(SIGSEGV, opents_port_crash_handler);
	signal(SIGBUS, opents_port_crash_handler);
	signal(SIGABRT, opents_port_crash_handler);
	signal(SIGILL, opents_port_crash_handler);
	signal(SIGFPE, opents_port_crash_handler);
}

/*
 * Declared here rather than through a header so that the Win32 build does not
 * have to be given one. The signature matches the definition in startup.cpp,
 * where HINSTANCE resolves to void * and CALLBACK is empty on this target.
 */
extern int WinMain(void *instance, void *previous, char *command_line, int command_show);

int main(int argc, char **argv)
{
	// OpenTS diag probe (temporary, remove before clean build):
	opents_port_install_crash_handler();

	/*
	 * Rebuild the command line the loader would have supplied, so that the
	 * engine sees its own arguments. Handing WinMain a null command line is not
	 * the same as starting with none: startup.cpp parses the options from
	 * GetCommandLine() rather than from this parameter, and with nothing there
	 * the "-X" switches are unreachable, including the one that opens the debug
	 * console. Without it a headless run has no way to report anything.
	 *
	 * The join quotes any argument holding whitespace, because this string is a
	 * round trip: the engine reads it back through CommandLineToArgvW, which
	 * splits on whitespace. Joined plainly, "-USERDIR=/tmp/My Games/OpenTS"
	 * arrives as three arguments and the directory is silently cut to
	 * "/tmp/My" -- a path that does not exist, so the launch fails on a folder
	 * the player can plainly see. opents_port_command_line() quotes the same way
	 * for the same reason; this is the assignment that makes it the live value.
	 */
	std::string joined;
	for (int index = 0; index < argc; index++) {
		std::string argument = argv[index] != nullptr ? argv[index] : "";

		if (argument.find_first_of(" \t\"") != std::string::npos
			&& argument.find('"') == std::string::npos) {
			argument = "\"" + argument + "\"";
		}

		if (index != 0) {
			joined += ' ';
		}
		joined += argument;
	}

	/*
	 * Owned by the command line slot for the life of the process rather than
	 * freed here: WinMain keeps the pointer, and GetCommandLine() reads the same
	 * storage from anywhere in the engine, including after startup has returned.
	 */
	char *command_line = strdup(joined.c_str());
	opents_port_command_line() = command_line;

	/*
	 * No instance handle and no previous instance. The show command is
	 * SW_SHOWNORMAL: the engine asks for a normal window, and it is up to the
	 * backend rather than this shim that there is no window to give it.
	 */
	return WinMain(nullptr, nullptr, command_line, 1);
}

#endif /* OPENTS_EXPERIMENTAL_NONWIN32 */
