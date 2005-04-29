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

#include	<defs.h>
#include	"FEATURE/options"
#include	"name.h"
#include	"shtable.h"

#if SHOPT_BASH
#   define bashopt(a,b)	a,	b|SH_BASHOPT,
#   define bashextra(a,b)	a,	b|SH_BASHEXTRA,
#else
#   define bashopt(a,b)
#   define bashextra(a,b)
#endif

/*
 * This is the list of invocation and set options
 * This list must be in in ascii sorted order
 */

const Shtable_t shtab_options[] =
{
	"allexport",			SH_ALLEXPORT,
#if SHOPT_BASH
	"bash",				(SH_BASH|SH_COMMANDLINE),
#endif
	"bgnice",			SH_BGNICE,
	bashextra("braceexpand",	SH_BRACEEXPAND)
	bashopt("cdable_vars",		SH_CDABLE_VARS)
	bashopt("cdspell",		SH_CDSPELL)
	bashopt("checkhash",		SH_CHECKHASH)
	bashopt("checkwinsize",		SH_CHECKWINSIZE)
	bashopt("dotglob",		SH_DOTGLOB)
	"emacs",			SH_EMACS,
	"errexit",			SH_ERREXIT,
	bashopt("execfail",		SH_EXECFAIL)
	bashopt("expand_aliases",	SH_EXPAND_ALIASES)
	bashopt("extglob",		SH_EXTGLOB)
	"globstar",			SH_GLOBSTARS,
	"gmacs",			SH_GMACS,
	bashextra("hashall",		SH_TRACKALL)
	bashopt("histappend",		SH_HISTAPPEND)
#if SHOPT_HISTEXPAND
	"histexpand",			SH_HISTEXPAND,
#else
	bashextra("histexpand",		SH_HISTEXPAND)
#endif
	bashextra("history",		SH_HISTORY2)
	bashopt("histreedit",		SH_HISTREEDIT)
	bashopt("histverify",		SH_HISTVERIFY)
	bashopt("hostcomplete",		SH_HOSTCOMPLETE)
	bashopt("huponexit",		SH_HUPONEXIT)
	"ignoreeof",			SH_IGNOREEOF,
	"interactive",			SH_INTERACTIVE|SH_COMMANDLINE,
	bashextra("interactive_comments",	SH_INTERACTIVE_COMM)
	"keyword",			SH_KEYWORD,
	bashopt("lithist",		SH_LITHIST)
	bashopt("login_shell",		SH_LOGIN_SHELL|SH_COMMANDLINE)
	bashopt("mailwarn",		SH_MAILWARN)
	"markdirs",			SH_MARKDIRS,
	"monitor",			SH_MONITOR,
	bashopt("no_empty_cmd_completion", SH_NOEMPTYCMDCOMPL)
	bashopt("nocaseglob",		SH_NOCASEGLOB)
	"noclobber",			SH_NOCLOBBER,
	"noexec",			SH_NOEXEC,
	"noglob",			SH_NOGLOB,
	"nolog",			SH_NOLOG,
	"notify",			SH_NOTIFY,
	"nounset",			SH_NOUNSET,
	bashopt("nullglob",		SH_NULLGLOB)
	bashextra("onecmd",		SH_TFLAG)
	"pipefail",			SH_PIPEFAIL,
	bashextra("physical",		SH_PHYSICAL)
	bashextra("posix",		SH_POSIX)
	"privileged",			SH_PRIVILEGED,
#if SHOPT_PFSH
	"profile",			SH_PFSH|SH_COMMANDLINE,
#endif
	bashopt("progcomp",		SH_PROGCOMP)
	bashopt("promptvars",		SH_PROMPTVARS)
	"restricted",			SH_RESTRICTED|SH_COMMANDLINE,
	bashopt("restricted_shell",	SH_RESTRICTED2|SH_COMMANDLINE)
	bashopt("shift_verbose",	SH_SHIFT_VERBOSE)
	bashopt("sourcepath",		SH_SOURCEPATH)
	"trackall",			SH_TRACKALL,
	"verbose",			SH_VERBOSE,
	"vi",				SH_VI,
	"viraw",			SH_VIRAW,
	bashopt("xpg_echo",		SH_XPG_ECHO)
	"xtrace",			SH_XTRACE,
	"",				0
};

const Shtable_t shtab_attributes[] =
{
	{"-nnameref",	NV_REF},
	{"-xexport",	NV_EXPORT},
	{"-rreadonly",	NV_RDONLY},
	{"-ttagged",	NV_TAGGED},
	{"-Eexponential",(NV_INTEGER|NV_DOUBLE|NV_EXPNOTE)},
	{"-Ffloat",	(NV_INTEGER|NV_DOUBLE)},
	{"++short",	(NV_INTEGER|NV_SHORT)},
	{"++unsigned",	(NV_INTEGER|NV_UNSIGN)},
	{"-iinteger",	NV_INTEGER},
	{"-Hfilename",	NV_HOST},
	{"-bbinary",    NV_BINARY},
	{"-llowercase",	NV_UTOL},
	{"-Zzerofill",	NV_ZFILL},
	{"-Lleftjust",	NV_LJUST},
	{"-Rrightjust",	NV_RJUST},
	{"-uuppercase",	NV_LTOU},
	{"-Aarray",	NV_ARRAY},
	{"++namespace",	NV_TABLE},
	{"",		0}
};
