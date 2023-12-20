// Filename:-	tr_font.h
//
// font support

#ifndef DEDICATED
#ifndef TR_FONT_H
#define TR_FONT_H

#pragma once

#include "../qcommon/q_shared.h"

void R_ShutdownFonts(void);
void R_InitFonts(void);
int RE_RegisterFont(const char *psName);
int RE_Font_StrLenPixels(const char *psText, const int iFontHandle, float fScale, float xadjust, float yadjust);
int RE_Font_StrLenChars(const char *psText);
int RE_Font_HeightPixels(const int iFontHandle, float fScale, float xadjust, float yadjust);
void RE_Font_DrawString(int ox, int oy, const char *psText, const vec4_t rgba,
	int iFontHandle, int iCharLimit, float fScale, float xadjust, float yadjust);
qboolean Language_IsAsian(void);
qboolean Language_UsesSpaces(void);
unsigned int AnyLanguage_ReadCharFromString( const char *psText, int *piAdvanceCount, qboolean *pbIsTrailingPunctuation/* = NULL*/ );



#endif	// #ifndef TR_FONT_H
#endif
// end

