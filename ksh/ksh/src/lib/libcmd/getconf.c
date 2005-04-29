/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1992-2004 AT&T Corp.                *
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
*               Glenn Fowler <gsf@research.att.com>                *
*                David Korn <dgk@research.att.com>                 *
*                                                                  *
*******************************************************************/
#pragma prototyped
/*
 * Glenn Fowler
 * AT&T Labs Research
 *
 * getconf - get configuration values
 */

static const char usage[] =
"[-?\n@(#)$Id: getconf (AT&T Labs Research) 2003-05-31 $\n]"
USAGE_LICENSE
"[+NAME?getconf - get configuration values]"
"[+DESCRIPTION?\bgetconf\b displays the system configuration value for"
"	\aname\a. If \aname\a is a filesystem specific variable then"
"	the value is determined relative to \apath\a or the current"
"	directory if \apath\a is omitted. If \avalue\a is specified then"
"	\bgetconf\b attempts to change the process local value to \avalue\a."
"	\b-\b may be used in place of \apath\a when it is not relevant."
"	Only \bwritable\b variables may be set; \breadonly\b variables"
"	cannot be changed.]"
"[+?The current value for \aname\a is written to the standard output. If"
"	\aname\a is valid but undefined then \bundefined\b is written to"
"	the standard output. If \aname\a is invalid or an error occurs in"
"	determining its value, then a diagnostic written to the standard error"
"	and \bgetconf\b exits with a non-zero exit status.]"
"[+?More than one variable may be set or queried by providing the \aname\a"
"	\apath\a \avalue\a 3-tuple for each variable, specifying \b-\b for"
"	\avalue\a when querying.]"
"[+?If no operands are specified then all known variables are written in"
"	\aname\a=\avalue\a form to the standard output, one per line."
"	Only one of \b--call\b, \b--name\b or \b--standard\b may be specified.]"

"[a:all?All known variables are written in \aname\a=\avalue\a form to the"
"	standard output, one per line. Present for compatibility with other"
"	implementations.]"
"[b:base?List base variable name sans call and standard prefixes.]"
"[c:call?Display variables with call prefix that matches \aRE\a. The call"
"	prefixes are:]:[RE]{"
"		[+CS?\bconfstr\b(2)]"
"		[+PC?\bpathconf\b(2)]"
"		[+SC?\bsysconf\b(2)]"
"		[+SI?\bsysinfo\b(2)]"
"		[+XX?Constant value.]"
"}"
"[d:defined?Only display defined values when no operands are specified.]"
"[l:lowercase?List variable names in lower case.]"
"[n:name?Display variables with name that match \aRE\a.]:[RE]"
"[p:portable?Display the named \bwritable\b variables and values in a form that"
"	can be directly executed by \bsh\b(1) to set the values. If \aname\a"
"	is omitted then all \bwritable\b variables are listed.]"
"[q:quote?\"...\" quote values.]"
"[r:readonly?Display the named \breadonly\b variables in \aname\a=\avalue\a form."
"	If \aname\a is omitted then all \breadonly\b variables are listed.]"
"[s:standard?Display variables with standard prefix that matches \aRE\a."
"	Use the \b--table\b option to view all standard prefixes, including"
"	local additions. The standard prefixes available on all systems"
"	are:]:[RE]{"
"		[+AES]"
"		[+AST]"
"		[+C]"
"		[+POSIX]"
"		[+SVID]"
"		[+XBS5]"
"		[+XOPEN]"
"		[+XPG]"
"}"
"[t:table?Display the internal table that contains the name, standard,"
"	standard section, and system call symbol prefix for each variable.]"
"[w:writable?Display the named \bwritable\b variables in \aname\a=\avalue\a"
"	form. If \aname\a is omitted then all \bwritable\b variables are"
"	listed.]"
"[v:specification?Ignored by this implementation.]:[name]"

"\n"
"\n[ name [ path [ value ] ] ... ]\n"
"\n"

"[+ENVIRONMENT]{"
"	[+_AST_FEATURES?Process local writable values that are different from"
"		the default are stored in the \b_AST_FEATURES\b environment"
"		variable. The \b_AST_FEATURES\b value is a space-separated"
"		list of \aname\a \apath\a \avalue\a 3-tuples, where"
"		\aname\a is the system configuration name, \apath\a is the"
"		corresponding path, \b-\b if no path is applicable, and"
"		\avalue\a is the system configuration value.]"
"}"
"[+SEE ALSO?\bpathchk\b(1), \bconfstr\b(2), \bpathconf\b(2),"
"	\bsysconf\b(2), \bastgetconf\b(3)]"
;

#include <cmdlib.h>

int
b_getconf(int argc, char** argv, void* context)
{
	register char*	name;
	register char*	path;
	register char*	value;
	register char*	s;
	char*		pattern;
	int		all;
	int		flags;

	static char	empty[] = "-";

	NoP(argc);
	cmdinit(argv, context, ERROR_CATALOG, 0);
	all = 0;
	flags = 0;
	pattern = 0;
	for (;;)
	{
		switch (optget(argv, usage))
		{
		case 'a':
			all = opt_info.num;
			continue;
		case 'b':
			flags |= ASTCONF_base;
			continue;
		case 'c':
			flags |= ASTCONF_matchcall;
			pattern = opt_info.arg;
			continue;
		case 'd':
			flags |= ASTCONF_defined;
			continue;
		case 'l':
			flags |= ASTCONF_lower;
			continue;
		case 'n':
			flags |= ASTCONF_matchname;
			pattern = opt_info.arg;
			continue;
		case 'p':
			flags |= ASTCONF_parse;
			continue;
		case 'q':
			flags |= ASTCONF_quote;
			continue;
		case 'r':
			flags |= ASTCONF_read;
			continue;
		case 's':
			flags |= ASTCONF_matchstandard;
			pattern = opt_info.arg;
			continue;
		case 't':
			flags |= ASTCONF_table;
			continue;
		case 'w':
			flags |= ASTCONF_write;
			continue;
		case ':':
			error(2, "%s", opt_info.arg);
			break;
		case '?':
			error(ERROR_usage(2), "%s", opt_info.arg);
			break;
		}
		break;
	}
	argv += opt_info.index;
	if (error_info.errors || (name = *argv) && all)
		error(ERROR_usage(2), "%s", optusage(NiL));
	do
	{
		if (!name)
		{
			path = 0;
			value = 0;
		}
		else
		{
			if (streq(name, empty))
				name = 0;
			if (!(path = *++argv))
				value = 0;
			else
			{
				if (streq(path, empty))
					path = 0;
				if ((value = *++argv) && (streq(value, empty)))
					value = 0;
			}
		}
		if (!name)
			astconflist(sfstdout, path, flags, pattern);
		else if (!(s = astgetconf(name, path, value, errorf)))
			break;
		else if (!value)
		{
			if (flags & X_OK)
			{
				sfputr(sfstdout, name, ' ');
				sfputr(sfstdout, path ? path : empty, ' ');
			}
			sfputr(sfstdout, *s ? s : "undefined", '\n');
		}
	} while (*argv && (name = *++argv));
	error_info.flags &= ~ERROR_LIBRARY;
	return error_info.errors != 0;
}
