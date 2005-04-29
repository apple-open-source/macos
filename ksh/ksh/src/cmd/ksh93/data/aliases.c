/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1982-2004 AT&T Corp.                *
*        and it may only be used by you under license from         *
*                       AT&T Corp. ("AT&T")                        *
*         A copy of the Source Code Agreement is available         *
*                at the AT&T Internet web site URL                 *
*                                                                  *
*       http://www.research.att.com/sw/license/ast-open.html       *
*                                                                  *
*    If you have copied or used this software without agreeing     *
*        to the terms of the license you are infringing on         *
*           the license and copyright and are violating            *
*               AT&T's intellectual property rights.               *
*                                                                  *
*            Information and Software Systems Research             *
*                        AT&T Labs Research                        *
*                         Florham Park NJ                          *
*                                                                  *
*                David Korn <dgk@research.att.com>                 *
*                                                                  *
*******************************************************************/
#pragma prototyped
#include	<ast.h>
#include	<signal.h>
#include	"FEATURE/options"
#include	"FEATURE/dynamic"
#include	"shtable.h"
#include	"name.h"

/*
 * This is the table of built-in aliases.  These should be exported.
 */

const struct shtable2 shtab_aliases[] =
{
#if SHOPT_FS_3D
	"2d",		NV_NOFREE,	"set -f;_2d",
#endif /* SHOPT_FS_3D */
	"autoload",	NV_NOFREE,	"typeset -fu",
	"command",	NV_NOFREE,	"command ",
	"fc",		NV_NOFREE,	"hist",
	"float",	NV_NOFREE,	"typeset -E",
	"functions",	NV_NOFREE,	"typeset -f",
	"hash",		NV_NOFREE,	"alias -t --",
	"history",	NV_NOFREE,	"hist -l",
	"integer",	NV_NOFREE,	"typeset -i",
	"nameref",	NV_NOFREE,	"typeset -n",
	"nohup",	NV_NOFREE,	"nohup ",
	"r",		NV_NOFREE,	"hist -s",
	"redirect",	NV_NOFREE,	"command exec",
	"times",	NV_NOFREE,	"{ { time;} 2>&1;}",
	"type",		NV_NOFREE,	"whence -v",
#ifdef SIGTSTP
	"stop",		NV_NOFREE,	"kill -s STOP",
	"suspend", 	NV_NOFREE,	"kill -s STOP $$",
#endif /*SIGTSTP */
	"",		0,		(char*)0
};

