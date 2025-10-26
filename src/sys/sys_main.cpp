#include <csignal>
#include <cstdlib>
#include <cstdarg>
#include <cstdio>
#include <sys/stat.h>
#define __STDC_FORMAT_MACROS
#if (defined(_MSC_VER) && _MSC_VER < 1800)
#include <stdint.h>
#else
#include <inttypes.h>
#endif
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#ifndef DEDICATED
#include "SDL.h"
#endif
#include "../qcommon/qcommon.h"
#include "../sys/sys_local.h"
#include "../sys/sys_loadlib.h"
#include "../sys/sys_public.h"
#include "con_local.h"

static char binaryPath[ MAX_OSPATH ] = { 0 };
static char installPath[ MAX_OSPATH ] = { 0 };

cvar_t *com_minimized;
cvar_t *com_unfocused;
cvar_t *com_maxfps;
cvar_t *com_maxfpsMinimized;
cvar_t *com_maxfpsUnfocused;

static volatile sig_atomic_t sys_signal = 0;

/*
=================
Sys_BinaryPath
=================
*/
char *Sys_BinaryPath(void)
{
	return binaryPath;
}

/*
=================
Sys_SetDefaultInstallPath
=================
*/
void Sys_SetDefaultInstallPath(const char *path)
{
	Q_strncpyz(installPath, path, sizeof(installPath));
}

/*
=================
Sys_DefaultInstallPath
=================
*/
#if defined(MACOS_X)
static char *last_strstr(const char *haystack, const char *needle)
{
    if (*needle == '\0')
        return (char *) haystack;

    char *result = NULL;
    for (;;) {
        char *p = (char *)strstr(haystack, needle);
        if (p == NULL)
            break;
        result = p;
        haystack = p + 1;
    }

    return result;
}
#endif // MACOS_X

#if !defined(MACOS_X) && defined(INSTALLED)
#include <unistd.h>

char *Sys_LinuxGetInstallPrefix() {
    static char path[MAX_OSPATH];
    int i;

    readlink("/proc/self/exe", path, sizeof(path));

    // from: /usr/local/bin/jk2mvmp
    // to: /usr/local
    for (i = 0; i < 2; i++) {
        char *l = strrchr(path, '/');
        if (!l) {
            break;
        }

        *l = 0;
    }

    return path;
}
#endif

char *Sys_DefaultInstallPath(void)
{
    if (*installPath) {
        return installPath;
    } else {
#if defined(MACOS_X) && defined(INSTALLED)
        // Inside the app...
        static char path[MAX_OSPATH];
        char *override;

        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size)) {
            return Sys_Cwd();
        }

        override = last_strstr(path, "MacOS");
        if (!override) {
            return Sys_Cwd();
        }

        override[5] = 0;
        return path;
#elif !defined(MACOS_X) && defined(INSTALLED)
        static char path[MAX_OSPATH];

        Com_sprintf(path, sizeof(path), "%s/share/jk2mv", Sys_LinuxGetInstallPrefix());
        return path;
#else
		return Sys_Cwd();
#endif
    }
}

/*
=================
Sys_DefaultAppPath
=================
*/
char *Sys_DefaultAppPath(void)
{
	return Sys_BinaryPath();

}

/*
==================
Sys_GetClipboardData
==================
*/
char *Sys_GetClipboardData(void) {
#ifdef DEDICATED
	return NULL;
#else
	if (!SDL_HasClipboardText())
		return NULL;

	char *cbText = SDL_GetClipboardText();
	size_t len = strlen(cbText) + 1;

	char *buf = (char *)Z_Malloc(len, TAG_CLIPBOARD);
	Q_strncpyz(buf, cbText, len);

	SDL_free(cbText);
	return buf;
#endif
}

/*
=================
Sys_ConsoleInput

Handle new console input
=================
*/
char *Sys_ConsoleInput(void) {
	return CON_Input();
}

void Sys_Print(const char *msg, qboolean extendedColors) {
	// TTimo - prefix for text that shows up in console but not in notify
	// backported from RTCW
	if (!Q_strncmp(msg, "[skipnotify]", 12)) {
		msg += 12;
	}
	if (msg[0] == '*') {
		msg += 1;
	}
	ConsoleLogAppend(msg);
	CON_Print(msg, (extendedColors ? true : false));
}

/*
================
Sys_Init

Called after the common systems (cvars, files, etc)
are initialized
================
*/
void Sys_Init(void) {
	Cmd_AddCommand("in_restart", IN_Restart);
	Cvar_Get("arch", ARCH_STRING, CVAR_ROM);
	Cvar_Get("username", Sys_GetCurrentUser(), CVAR_ROM);

	com_unfocused = Cvar_Get("com_unfocused", "0", CVAR_ROM);
	com_minimized = Cvar_Get("com_minimized", "0", CVAR_ROM);
	com_maxfps = Cvar_Get("com_maxfps", "125", CVAR_ARCHIVE | CVAR_GLOBAL);
	com_maxfpsUnfocused = Cvar_Get("com_maxfpsUnfocused", "0", CVAR_ARCHIVE | CVAR_GLOBAL);
	com_maxfpsMinimized = Cvar_Get("com_maxfpsMinimized", "50", CVAR_ARCHIVE | CVAR_GLOBAL);
}

static void Q_NORETURN Sys_Exit(int ex) {
	IN_Shutdown();
#ifndef DEDICATED
	SDL_Quit();
#endif

	NET_Shutdown();

	Sys_PlatformExit();

	Com_ShutdownZoneMemory();

	CON_Shutdown();

	exit(ex);
}

#if !defined(DEDICATED)
static void Sys_ErrorDialog(const char *error) {
	time_t rawtime;
	char timeStr[32] = {}; // should really only reach ~19 chars
	char crashLogPath[MAX_OSPATH];

	time(&rawtime);
	strftime(timeStr, sizeof(timeStr), "%Y-%m-%d_%H-%M-%S", localtime(&rawtime)); // or gmtime
	Com_sprintf(crashLogPath, sizeof(crashLogPath),
		"%s%cerrorlog-%s.txt",
		Sys_DefaultHomePath(), PATH_SEP, timeStr);

	Sys_Mkdir(Sys_DefaultHomePath());

	FILE *fp = fopen(crashLogPath, "w");
	if (fp) {
		ConsoleLogWriteOut(fp);
		fclose(fp);

		const char *errorMessage = va("%s\n\nThe error log was written to %s", error, crashLogPath);
		if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", errorMessage, NULL) < 0) {
			fprintf(stderr, "%s", errorMessage);
		}
	} else {
		// Getting pretty desperate now
		ConsoleLogWriteOut(stderr);
		fflush(stderr);

		const char *errorMessage = va("%s\nCould not write the error log file, but we printed it to stderr.\n"
			"Try running the game using a command line interface.", error);
		if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", errorMessage, NULL) < 0) {
			// We really have hit rock bottom here :(
			fprintf(stderr, "%s", errorMessage);
		}
	}
}
#endif

void Q_NORETURN QDECL Sys_Error(const char *error, ...) {
	va_list argptr;
	char    string[1024];

	va_start(argptr, error);
	Q_vsnprintf(string, sizeof(string), error, argptr);
	va_end(argptr);

	Sys_Print(string,qfalse);
	Sys_PrintBacktrace();

	// Only print Sys_ErrorDialog for client binary. The dedicated
	// server binary is meant to be a command line program so you would
	// expect to see the error printed.
#if !defined(DEDICATED)
	Sys_ErrorDialog(string);
#endif

	Sys_Exit(3);
}

void Q_NORETURN Sys_Quit(void) {
	Sys_Exit(0);
}

/*
============
Sys_FileTime

returns -1 if not present
============
*/
time_t Sys_FileTime(const char *path) {
	struct stat buf;

	if (stat(path, &buf) == -1)
		return -1;

	return buf.st_mtime;
}

/*
=================
Sys_UnloadDll
=================
*/
void Sys_UnloadDll( void *dllHandle )
{
	if( !dllHandle )
	{
		Com_Printf("Sys_UnloadDll(NULL)\n");
		return;
	}

	Sys_UnloadLibrary(dllHandle);
}

/*
=================
Sys_LoadDll
First try to load library name from system library path,
from executable path, then fs_basepath.
=================
*/

extern char		*FS_BuildOSPath( const char *base, const char *game, const char *qpath );

void *Sys_LoadDll(const char *name, qboolean useSystemLib)
{
	void *dllhandle = NULL;

	// Don't load any DLLs that end with the pk3 extension
	if ( COM_CompareExtension( name, ".pk3" ) )
	{
		Com_Printf( S_COLOR_YELLOW "WARNING: Rejecting DLL named \"%s\"", name );
		return NULL;
	}

	if ( useSystemLib )
	{
		Com_Printf( "Trying to load \"%s\"...\n", name );

		dllhandle = Sys_LoadLibrary( name );
		if ( dllhandle )
			return dllhandle;

		Com_Printf( "%s(%s) failed: \"%s\"\n", __FUNCTION__, name, Sys_LibraryError() );
	}

	//fn = FS_BuildOSPath( cdpath, gamedir, name );
	const char *binarypath = Sys_BinaryPath();
	const char *basepath = Cvar_VariableString( "fs_basepath" );

	if ( !*binarypath )
		binarypath = ".";

	const char *searchPaths[] = {
		binarypath,
		basepath,
	};
	const size_t numPaths = ARRAY_LEN( searchPaths );

	for ( size_t i = 0; i < numPaths; i++ )
	{
		const char *libDir = searchPaths[i];
		if ( !libDir[0] )
			continue;

		Com_Printf( "Trying to load \"%s\" from \"%s\"...\n", name, libDir );
		char *fn = va( "%s%c%s", libDir, PATH_SEP, name );
		dllhandle = Sys_LoadLibrary( fn );
		if ( dllhandle )
			return dllhandle;

		Com_Printf( "%s(%s) failed: \"%s\"\n", __FUNCTION__, fn, Sys_LibraryError() );
	}
	return NULL;
}

/*
=================
Sys_LoadModuleLibrary

Used to load a module (jk2mpgame, cgame, ui) dll
=================
*/
void *Sys_LoadModuleLibrary(const char *name, qboolean mvOverride, VM_EntryPoint_t *entryPoint, intptr_t(QDECL *systemcalls)(intptr_t, ...)) {
	void	*libHandle = NULL;
	void	(QDECL *dllEntry)(intptr_t(QDECL *syscallptr)(intptr_t, ...));
	const char	*path, *filePath;
	char	filename[MAX_OSPATH];

	Com_sprintf(filename, sizeof(filename), "%s_" ARCH_STRING "." LIBRARY_EXTENSION, name);

	if (!mvOverride) {
		path = Cvar_VariableString("fs_basepath");
		filePath = FS_BuildOSPath(path, NULL, filename);

		Com_DPrintf("Loading module: %s...", filePath);
		libHandle = Sys_LoadLibrary(filePath);
		if (!libHandle) {
			Com_DPrintf(" failed!\n");
			path = Cvar_VariableString("fs_homepath");
			filePath = FS_BuildOSPath(path, NULL, filename);

			Com_DPrintf("Loading module: %s...", filePath);
			libHandle = Sys_LoadLibrary(filePath);
			if (!libHandle) {
				Com_DPrintf(" failed!\n");
				return NULL;
			} else {
				Com_DPrintf(" success!\n");
			}
		} else {
			Com_DPrintf(" success!\n");
		}
	} else {
		char dllPath[MAX_OSPATH];
		path = Cvar_VariableString("fs_basepath");
		Com_sprintf(dllPath, sizeof(dllPath), "%s\\%s", path, filename);

		Com_DPrintf("Loading module: %s...", dllPath);
		libHandle = Sys_LoadLibrary(dllPath);
		if (!libHandle) {
			Com_DPrintf(" failed!\n");
			return NULL;
		} else {
			Com_DPrintf(" success!\n");
		}
	}

	dllEntry = (void (QDECL *)(intptr_t(QDECL *)(intptr_t, ...)))Sys_LoadFunction( libHandle, "dllEntry" );
	*entryPoint = (VM_EntryPoint_t)Sys_LoadFunction( libHandle, "vmMain" );

	if (!*entryPoint) {
		Com_DPrintf("Could not find vmMain in %s\n", filename);
		Sys_UnloadLibrary(libHandle);
		return NULL;
	}

	if (!dllEntry) {
		Com_DPrintf("Could not find dllEntry in %s\n", filename);
		Sys_UnloadLibrary(libHandle);
		return NULL;
	}

	dllEntry(systemcalls);

	return libHandle;
}



/*
=================
Sys_SigHandler
=================
*/
void Sys_SigHandler(int signal) {
	sys_signal = signal;
}

#if defined(_MSC_VER) && !defined(_DEBUG)
LONG WINAPI Sys_NoteException(EXCEPTION_POINTERS* pExp, DWORD dwExpCode);
void Sys_WriteCrashlog();
#endif

int main(int argc, char* argv[]) {
	int		i;
	char	commandLine[MAX_STRING_CHARS] = { 0 };
	int		missingFuncs = Sys_FindFunctions();

#if defined(_MSC_VER) && !defined(_DEBUG)
	__try {
#endif

	Sys_PlatformInit(argc, argv);

#if defined(DEBUG) && !defined(DEDICATED)
	CON_CreateConsoleWindow();
#endif
	CON_Init();

	// get the initial time base
	Sys_Milliseconds();

#ifdef MACOS_X
	// This is passed if we are launched by double-clicking
	if (argc >= 2 && Q_strncmp(argv[1], "-psn", 4) == 0)
		argc = 1;
#endif

#if defined(DEBUG_SDL) && !defined(DEDICATED)
	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
#endif

	// Concatenate the command line for passing to Com_Init
	for (i = 1; i < argc; i++) {
		const bool containsSpaces = (strchr(argv[i], ' ') != NULL);
		if (containsSpaces)
			Q_strcat(commandLine, sizeof(commandLine), "\"");

		Q_strcat(commandLine, sizeof(commandLine), argv[i]);

		if (containsSpaces)
			Q_strcat(commandLine, sizeof(commandLine), "\"");

		Q_strcat(commandLine, sizeof(commandLine), " ");
	}

	Com_Init(commandLine);

	if ( missingFuncs ) {
		static const char *missingFuncsError =
			"Your system is missing functions this application relies on.\n"
			"\n"
			"Some features may be unavailable or their behavior may be incorrect.";

		// Set the error cvar (the main menu should pick this up and display an error box to the user)
		Cvar_Get( "com_errorMessage", missingFuncsError, CVAR_ROM );
		Cvar_Set( "com_errorMessage", missingFuncsError );

		// Print the error into the console, because we can't always display the main menu (dedicated servers, ...)
		Com_Printf( "********************\n" );
		Com_Printf( "ERROR: %s\n", missingFuncsError );
		Com_Printf( "********************\n" );
	}

	// main game loop
	while (!sys_signal) {
		if (com_busyWait->integer) {
			bool shouldSleep = false;

			if (com_dedicated->integer) {
				shouldSleep = true;
			}

			if (com_minimized->integer) {
				shouldSleep = true;
			}

			if (shouldSleep) {
				Sys_Sleep(5);
			}
		}

		// run the game
		Com_Frame();
	}

	Com_Quit(sys_signal);

#if defined(_MSC_VER) && !defined(_DEBUG)
	} __except(Sys_NoteException(GetExceptionInformation(), GetExceptionCode())) {
		Sys_WriteCrashlog();
		return 1;
	}
#endif

	// never gets here
	return 0;
}
