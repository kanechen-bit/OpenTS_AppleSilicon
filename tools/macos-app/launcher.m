//
//  OpenTS.app launcher
//
//  The bundle's main executable. Tiberian Sun's data files cannot be shipped with
//  the application -- they are the game's own content, still inside the Steam
//  install or GOG archive on the player's disk -- so the first launch asks where
//  they are and remembers the answer.
//
//  What it does, in order:
//
//    1. Reads the remembered data folder. First launch has none, so it shows an
//       NSOpenPanel folder picker.
//    2. Checks the folder actually holds Tiberian Sun. Anything missing is listed
//       by name in an error dialog; the player can pick a different folder instead
//       of quitting.
//    3. Creates the writable user folder (settings, saved games, debug logs) under
//       ~/Library/Application Support/OpenTS, seeded with a default SUN.INI.
//    4. Hands the real engine its directories and steps aside: execv()s the engine
//       binary that sits beside this one inside Contents/MacOS.
//
//  Why execv() and not NSTask: the engine opens its Metal window and owns the run
//  loop, and the process that holds the bundle's Dock tile and activation policy
//  has to be the process that draws it. Replacing this process outright is the only
//  arrangement where the engine is a first-class app rather than a child that has
//  to be waited on and reaped.
//
//  Built by tools/macos-app/make_app.sh; deliberately not part of the CMake build,
//  so the engine stays buildable with nothing but a compiler and CMake.
//

#import <Cocoa/Cocoa.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


#pragma mark - What Tiberian Sun needs to run

/*
 * The files measured as required, by booting the engine against progressively
 * smaller sets of a real install (see docs/game-data-requirements.md).
 */
static NSArray<NSString *> * const kRequiredFiles = @[
	@"TIBSUN.MIX",		// carries the encrypted archives everything else is reached through
	@"MAPS01.MIX",		// the solo campaign map set
	@"SCORES.MIX",		// menu music
];

/*
 * Groups where any one member is enough: the intro movie ships as three discs'
 * worth of alternatives, and the in-game speech as either the GDI or the Nod CD.
 */
static NSArray<NSArray<NSString *> *> * const kRequiredGroups = @[
	@[@"MOVIES01.MIX", @"MOVIES02.MIX", @"MOVIES03.MIX"],
	@[@"SIDECD01.MIX", @"SIDECD02.MIX"],
];

static NSString * const kDataDirKey = @"DataDirectoryBookmark";
static NSString * const kAppSupportFolder = @"OpenTS";


#pragma mark - Small helpers

static BOOL File_Exists(NSString * directory, NSString * name)
{
	NSString * path = [directory stringByAppendingPathComponent:name];
	return([[NSFileManager defaultManager] fileExistsAtPath:path]);
}


static BOOL Is_Directory(NSString * path)
{
	BOOL is_directory = NO;

	if (![[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&is_directory]) {
		return(NO);
	}

	return(is_directory);
}


/*
 * What the engine can be told, in its own terms.
 *
 * Parse_Command_Line() matches "-DATADIR=" and then takes the whole remainder of
 * the token verbatim, so an ordinary path needs no quoting, escaping or splitting
 * here -- and a path containing '=' is fine as it stands. What it does do is
 * strip every '"' from the token, which is the one character that cannot survive
 * the trip; Folders_With_Unquotable_Path() rejects those rather than quietly
 * handing the engine a different folder than the player chose.
 */
static BOOL Folders_With_Unquotable_Path(NSString * path)
{
	return([path rangeOfString:@"\""].location != NSNotFound);
}


static NSString * Engine_Path(void)
{
	// This executable is Contents/MacOS/OpenTS; the engine is beside it.
	NSString * const macos = [[[NSBundle mainBundle] bundlePath]
							  stringByAppendingPathComponent:@"Contents/MacOS"];

	return([macos stringByAppendingPathComponent:@"OpenTS-engine"]);
}


#pragma mark - The writable folder

/*
 * Where everything the game writes goes. Not the bundle: a sealed bundle rejects
 * files that were not there when it was signed, so a SUN.INI written beside the
 * executable would break the signature and, on a notarized build, the launch
 * that follows.
 */
static NSString * User_Directory(void)
{
	NSArray<NSString *> * paths = NSSearchPathForDirectoriesInDomains(
			NSApplicationSupportDirectory, NSUserDomainMask, YES);

	NSString * base = paths.firstObject ?: NSTemporaryDirectory();
	NSString * folder = [base stringByAppendingPathComponent:kAppSupportFolder];

	[[NSFileManager defaultManager] createDirectoryAtPath:folder
							  withIntermediateDirectories:YES
											   attributes:nil
													error:NULL];

	return(folder);
}


/*
 * SUN.INI is read through the file layer, so it is found in the user directory
 * rather than beside the engine. A first run has none, and the engine would then
 * come up on its compiled-in defaults rather than the settings this port was
 * tuned with, so the shipped template is copied in once.
 */
static void Seed_Settings(NSString * user_directory)
{
	NSFileManager * manager = [NSFileManager defaultManager];
	NSString * const settings = [user_directory stringByAppendingPathComponent:@"SUN.INI"];

	if ([manager fileExistsAtPath:settings]) {
		return;
	}

	NSString * const source = [[[NSBundle mainBundle] resourcePath]
							   stringByAppendingPathComponent:@"SUN.INI"];

	if (![manager fileExistsAtPath:source]) {
		NSLog(@"[OpenTS] no shipped SUN.INI to seed settings from; using engine defaults.");
		return;
	}

	if (![manager copyItemAtPath:source toPath:settings error:NULL]) {
		NSLog(@"[OpenTS] could not write %@: %s", settings, strerror(errno));
	}
}


#pragma mark - Validating a folder

/*
 * What a folder is missing, as one alert-ready line, or nil when it holds
 * everything needed. Each group is named whole when none of its members is
 * present, because "MOVIES01.MIX or MOVIES02.MIX or MOVIES03.MIX" tells the
 * player more than any single one of them would.
 */
static NSString * Missing_Files_Report(NSString * directory)
{
	NSMutableArray<NSString *> * missing = [NSMutableArray array];

	for (NSString * name in kRequiredFiles) {
		if (!File_Exists(directory, name)) {
			[missing addObject:name];
		}
	}

	for (NSArray<NSString *> * group in kRequiredGroups) {
		BOOL satisfied = NO;

		for (NSString * name in group) {
			if (File_Exists(directory, name)) {
				satisfied = YES;
				break;
			}
		}

		if (!satisfied) {
			[missing addObject:[group componentsJoinedByString:@" or "]];
		}
	}

	if (missing.count == 0) {
		return(nil);
	}

	return([missing componentsJoinedByString:@", "]);
}


#pragma mark - Remembering the folder

/*
 * The remembered folder, or nil when there is none, it cannot be resolved, or it
 * has gone away.
 *
 * A bookmark rather than a path: a Steam library gets moved and renamed, and a
 * stored path would go stale and ask again. The bookmark follows the folder.
 */
static NSString * Remembered_Data_Directory(void)
{
	NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
	NSData * const bookmark = [defaults dataForKey:kDataDirKey];

	if (bookmark == nil) {
		return(nil);
	}

	BOOL stale = NO;
	NSURL * const url = [NSURL URLByResolvingBookmarkData:bookmark
										   options:NSURLBookmarkResolutionWithoutUI
								 relativeToURL:nil
							 bookmarkDataIsStale:&stale
											 error:NULL];

	if (url == nil) {
		return(nil);
	}

	if (stale) {
		// Re-write it, so the next launch resolves without another prompt.
		NSData * const refreshed = [url bookmarkDataWithOptions:0
						 includingResourceValuesForKeys:nil
										  relativeToURL:nil
												  error:NULL];

		if (refreshed != nil) {
			[defaults setObject:refreshed forKey:kDataDirKey];
		}
	}

	NSString * const path = url.path;

	if (path == nil || !Is_Directory(path)) {
		return(nil);
	}

	return(path);
}


static void Remember_Data_Directory(NSString * path)
{
	NSURL * const url = [NSURL fileURLWithPath:path isDirectory:YES];
	NSData * const bookmark = [url bookmarkDataWithOptions:0
					 includingResourceValuesForKeys:nil
									  relativeToURL:nil
											  error:NULL];

	if (bookmark == nil) {
		// Not fatal: the folder is simply chosen again next launch.
		NSLog(@"[OpenTS] could not bookmark %@; it will be asked for again.", path);
		return;
	}

	[[NSUserDefaults standardUserDefaults] setObject:bookmark forKey:kDataDirKey];
}


#pragma mark - Asking for the folder

/*
 * Runs the folder picker and returns the chosen path, or nil when cancelled.
 * This panel is the one piece of the launch that cannot be automated away, so it
 * is kept to exactly one call and everything after it stays testable.
 */
static NSString * Ask_For_Data_Directory(void)
{
	NSOpenPanel * const panel = [NSOpenPanel openPanel];

	[panel setCanChooseFiles:NO];
	[panel setCanChooseDirectories:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setCanCreateDirectories:NO];
	[panel setPrompt:@"Select"];
	[panel setMessage:@"Where are your Tiberian Sun game files?\n\n"
					  "Select the folder that contains TIBSUN.MIX."];

	NSString * const desktop = NSSearchPathForDirectoriesInDomains(
			NSDesktopDirectory, NSUserDomainMask, YES).firstObject;

	[panel setDirectoryURL:[NSURL fileURLWithPath:desktop ?: NSHomeDirectory()]];

	if ([panel runModal] != NSModalResponseOK) {
		return(nil);
	}

	// -canChooseDirectories guarantees the one URL is a directory, so it is the
	// folder that was chosen; the accessors below are for the panel's own state.
	return(panel.URLs.firstObject.path);
}


#pragma mark - Handing over to the engine

/*
 * Whether the game opens in a window rather than filling the screen.
 *
 * The engine takes -W for a window; there is no switch for the other direction,
 * because full screen is a setting (Fullscreen in SUN.INI) rather than a mode the
 * command line sets. So the two are kept in agreement here: the switch asks for a
 * window explicitly, and asking for full screen instead means writing the setting
 * and leaving the switch off, rather than passing a switch that would be ignored.
 *
 * A bundle is a windowed application by nature -- launched from the Finder, with a
 * Dock tile -- and on macOS the borderless full screen the game asks for drops the
 * player into a separate Space they have to swipe back out of, so the window is
 * the better default.
 *
 * OPENTS_FULLSCREEN=1 is how the other one is asked for without editing the
 * bundle. It rewrites the player's own Fullscreen setting, so it is deliberately
 * not the default: a first run is a window unless asked otherwise.
 */
static BOOL Prefers_Windowed(void)
{
	NSString * const requested = [[[NSProcessInfo processInfo] environment]
								  objectForKey:@"OPENTS_FULLSCREEN"];

	return(![requested isEqualToString:@"1"]);
}


/*
 * Turns full screen on in the settings the engine is about to read, which is the
 * only place it takes that setting from. Best effort: if the line cannot be
 * written the engine simply comes up windowed, which is the safer of the two.
 */
static void Request_Fullscreen(NSString * user_directory)
{
	NSString * const settings = [user_directory stringByAppendingPathComponent:@"SUN.INI"];
	NSString * contents = [NSString stringWithContentsOfFile:settings
													encoding:NSUTF8StringEncoding
													   error:NULL];

	if (contents == nil) {
		return;
	}

	NSString * const rewritten = [contents stringByReplacingOccurrencesOfString:@"Fullscreen=yes"
																  withString:@"Fullscreen=no"];

	if ([rewritten isEqualToString:contents]) {
		// Already windowed, or the key is written some other way. Say so plainly
		// rather than appending a second Fullscreen= the parser would resolve
		// unpredictably.
		NSLog(@"[OpenTS] OPENTS_FULLSCREEN=1 was asked for, but %@ does not say Fullscreen=yes. "
			  "Set it in the game's own options and it will persist.", settings.lastPathComponent);
		return;
	}

	[rewritten writeToFile:settings
				atomically:YES
				  encoding:NSUTF8StringEncoding
					 error:NULL];
}


static void Hand_Over(NSString * data_directory)
{
	NSString * const user_directory = User_Directory();

	Seed_Settings(user_directory);

	NSString * const engine = Engine_Path();

	if (![[NSFileManager defaultManager] isExecutableFileAtPath:engine]) {
		NSAlert * alert = [NSAlert new];
		alert.messageText = @"OpenTS is incomplete.";
		alert.informativeText = [NSString stringWithFormat:
				@"The game engine is missing from the application bundle:\n\n%@\n\n"
				 "Reinstall OpenTS.app from the archive it came in.", engine];
		[alert addButtonWithTitle:@"Quit"];
		[alert runModal];
		exit(1);
	}

	/*
	 * Logs go to the user directory. Left to itself the engine puts them beside its
	 * own executable, which inside a bundle is a directory the signature covers.
	 */
	setenv("OPENTS_DEBUG_DIR",
		   [user_directory stringByAppendingPathComponent:@"Debug"].UTF8String, 1);

	// The engine's parser wants each switch as one "-SWITCH=value" token, and
	// takes the value as the rest of that token, so a plain path is passed as is.
	NSString * const datadir_switch = [NSString stringWithFormat:@"-DATADIR=%@", data_directory];
	NSString * const userdir_switch = [NSString stringWithFormat:@"-USERDIR=%@", user_directory];

	if (!Prefers_Windowed()) {
		Request_Fullscreen(user_directory);
	}

	// -W last, so it is the switch that decides. It is only there when a window
	// was actually asked for; full screen is the engine's own default.
	char * const child_argv[] = {
		strdup(engine.UTF8String),
		strdup(datadir_switch.UTF8String),
		strdup(userdir_switch.UTF8String),
		(Prefers_Windowed() ? strdup("-W") : NULL),
		NULL,
	};

	execv(child_argv[0], child_argv);

	// Only reached when the exec itself failed.
	NSString * const reason = [NSString stringWithUTF8String:strerror(errno)] ?: @"unknown error";
	NSAlert * alert = [NSAlert new];
	alert.messageText = @"OpenTS could not start.";
	alert.informativeText = [NSString stringWithFormat:@"%@ could not be run:\n\n%@",
									 engine.lastPathComponent, reason];
	[alert addButtonWithTitle:@"Quit"];
	[alert runModal];
	exit(1);
}


#pragma mark - Entry point

int main(int argc, const char * argv[])
{
	@autoreleasepool {
		/*
		 * Regular rather than accessory: this is the application the player
		 * double-clicked, so it gets a Dock tile and a menu bar, and the engine
		 * that replaces it inherits both.
		 */
		[NSApplication sharedApplication];
		[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
		[NSApp activateIgnoringOtherApps:YES];

		/*
		 * A folder named in the environment skips the panel. That is the seam the
		 * validation and hand-over are tested through, since the panel itself is
		 * the one part that needs a person. Undocumented in the UI on purpose.
		 */
		NSString * const override = [[[NSProcessInfo processInfo] environment]
										objectForKey:@"OPENTS_DATA_DIR"];

		/*
		 * Validate the folder named in the environment and exit, without the panel,
		 * without an alert and without starting the engine. The two dialogs above are
		 * modal and need a person, so this is how the decision they hang off gets
		 * tested; the report goes to stdout in the same words the dialog would show.
		 */
		NSString * const check_only = [[[NSProcessInfo processInfo] environment]
										objectForKey:@"OPENTS_VALIDATE_ONLY"];

		if (override.length > 0 && check_only.length > 0) {
			NSString * const missing = Missing_Files_Report(override);

			if (missing != nil) {
				printf("INCOMPLETE %s\n", missing.UTF8String);
				return(2);
			}

			if (Folders_With_Unquotable_Path(override)) {
				printf("INCOMPLETE unquotable path\n");
				return(2);
			}

			printf("COMPLETE %s\n", override.UTF8String);
			return(0);
		}

		NSString * data_directory = override.length > 0 ? override : Remembered_Data_Directory();

		while (YES) {
			if (data_directory == nil) {
				data_directory = Ask_For_Data_Directory();

				if (data_directory == nil) {
					// Cancelled: nothing to launch, so nothing to stay running for.
					return(0);
				}
			}

			NSString * missing = Missing_Files_Report(data_directory);

			if (missing == nil && Folders_With_Unquotable_Path(data_directory)) {
				missing = @"a folder name containing a \" character, which the game "
						   @"cannot be told about";
			}

			if (missing == nil) {
				break;
			}

			NSAlert * alert = [NSAlert new];
			alert.alertStyle = NSAlertStyleCritical;
			alert.messageText = @"That folder has no Tiberian Sun game files.";
			alert.informativeText = [NSString stringWithFormat:
					@"The folder \u201c%@\u201d is missing:\n\n%@\n\n"
					 "Point at the folder the game was installed to \u2014 the one holding "
					 "TIBSUN.MIX. If you only have the launcher, install the game first.",
				 data_directory.lastPathComponent, missing];
			[alert addButtonWithTitle:@"Choose Another Folder\u2026"];
			[alert addButtonWithTitle:@"Quit"];

			if ([alert runModal] != NSAlertFirstButtonReturn) {
				return(0);
			}

			// Whatever was remembered has just been shown to be wrong.
			[[NSUserDefaults standardUserDefaults] removeObjectForKey:kDataDirKey];
			data_directory = nil;
		}

		if (override.length == 0) {
			Remember_Data_Directory(data_directory);
		}

		Hand_Over(data_directory);
	}

	return(0);
}
