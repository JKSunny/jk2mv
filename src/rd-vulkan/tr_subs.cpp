/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

// tr_subs.cpp - common function replacements for modular renderer
#include "tr_local.h"

void QDECL Com_Printf( const char *msg, ... )
{
	va_list         argptr;
	char            text[1024];

	va_start(argptr, msg);
	Q_vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);

	ri.Printf(PRINT_ALL, "%s", text);
}

void QDECL Com_OPrintf( const char *msg, ... )
{
	va_list         argptr;
	char            text[1024];

	va_start(argptr, msg);
	Q_vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);

	ri.OPrintf("%s", text);
}

#ifdef USE_JK2
void QDECL Com_Error( errorParm_t level, const char *error, ... )
#else
void QDECL Com_Error( int level, const char *error, ... )
#endif
{
	va_list         argptr;
	char            text[1024];

	va_start(argptr, error);
	Q_vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	ri.Error(level, "%s", text);
}

// HUNK
void *Hunk_AllocateTempMemory( int size ) {
	return ri.Hunk_AllocateTempMemory( size );
}

void Hunk_FreeTempMemory( void *buf ) {
	ri.Hunk_FreeTempMemory( buf );
}

#if defined(USE_JK2) && defined(HUNK_DEBUG)
void *Hunk_AllocDebug( int size, ha_pref preference, char *label, char *file, int line ) {
	return ri.Hunk_AllocDebug( size, preference, label, file, line );
}
#else
void *Hunk_Alloc( int size, ha_pref preference ) {
	return ri.Hunk_Alloc( size, preference );
}
#endif

int Hunk_MemoryRemaining( void ) {
	return ri.Hunk_MemoryRemaining();
}

// ZONE
#ifdef USE_JK2
void *Z_Malloc( int iSize, memtag_t eTag, qboolean bZeroit ) {
	return ri.Z_Malloc( iSize, eTag, bZeroit );
}
#else
void *Z_Malloc( int iSize, memtag_t eTag, qboolean bZeroit, int iAlign ) {
	return ri.Z_Malloc( iSize, eTag, bZeroit, iAlign );
}
#endif

void Z_Free( void *ptr ) {
	ri.Z_Free( ptr );
}

int Z_MemSize( memtag_t eTag ) {
	return ri.Z_MemSize( eTag );
}

void Z_MorphMallocTag( void *pvBuffer, memtag_t eDesiredTag ) {
	ri.Z_MorphMallocTag( pvBuffer, eDesiredTag );
}
