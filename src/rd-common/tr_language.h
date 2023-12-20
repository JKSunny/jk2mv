#ifndef CLIENT

#pragma once

#include "../qcommon/q_shared.h"

extern cvar_t	*sp_language;

inline int Language_GetIntegerValue(void)
{
	if (sp_language)
	{
		return sp_language->integer;
	}

	return 0;
}

inline qboolean Language_IsRussian(void)
{
	return (sp_language && !Q_stricmp(sp_language->string, "russian")) ? qtrue : qfalse;
}

inline qboolean Language_IsPolish(void)
{
	return (sp_language && !Q_stricmp(sp_language->string, "polish")) ? qtrue : qfalse;
}

inline qboolean Language_IsKorean(void)
{
	return (sp_language && !Q_stricmp(sp_language->string, "korean")) ? qtrue : qfalse;
}

inline qboolean Language_IsTaiwanese(void)
{
	return (sp_language && !Q_stricmp(sp_language->string, "taiwanese")) ? qtrue : qfalse;
}

inline qboolean Language_IsJapanese(void)
{
	return (sp_language && !Q_stricmp(sp_language->string, "japanese")) ? qtrue : qfalse;
}

inline qboolean Language_IsChinese(void)
{
	return (sp_language && !Q_stricmp(sp_language->string, "chinese")) ? qtrue : qfalse;
}

inline qboolean Language_IsThai(void)
{
	return (sp_language && !Q_stricmp(sp_language->string, "thai")) ? qtrue : qfalse;
}
#endif
