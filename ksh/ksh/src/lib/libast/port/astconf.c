/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1985-2004 AT&T Corp.                *
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
*                 Phong Vo <kpv@research.att.com>                  *
*                                                                  *
*******************************************************************/
#pragma prototyped

/*
 * string interface to confstr(),pathconf(),sysconf(),sysinfo()
 * extended to allow some features to be set
 */

static const char id[] = "\n@(#)$Id: getconf (AT&T Labs Research) 2003-06-21 $\0\n";

#include "univlib.h"

#include <ast.h>
#include <error.h>
#include <sfstr.h>
#include <fs3d.h>
#include <ctype.h>
#include <regex.h>

#include "confmap.h"
#include "conftab.h"
#include "FEATURE/libpath"

#if _sys_systeminfo
# if !_lib_sysinfo
#   if _lib_systeminfo
#     define _lib_sysinfo	1
#     define sysinfo(a,b,c)	systeminfo(a,b,c)
#   else
#     if _lib_syscall && _sys_syscall
#       include <sys/syscall.h>
#       if defined(SYS_systeminfo)
#         define _lib_sysinfo	1
#         define sysinfo(a,b,c)	syscall(SYS_systeminfo,a,b,c)
#       endif
#     endif
#   endif
# endif
#else
# undef	_lib_sysinfo
#endif
#if !_lib_sysinfo
# define sysinfo(a,b,c)	((errno=EINVAL),(-1))
#endif

#define OP_conformance		1
#define OP_fs_3d		2
#define OP_hosttype		3
#define OP_libpath		4
#define OP_libprefix		5
#define OP_libsuffix		6
#define OP_path_attributes	7
#define OP_path_resolve		8
#define OP_universe		9

#define CONF_ERROR	(CONF_USER<<0)
#define CONF_READONLY	(CONF_USER<<1)
#define CONF_ALLOC	(CONF_USER<<2)

#define INITIALIZE()	do{if(!state.data)synthesize(NiL,NiL,NiL);}while(0)

#define MAXVAL		256

#if MAXVAL <= UNIV_SIZE
#undef	MAXVAL
#define	MAXVAL		(UNIV_SIZE+1)
#endif

#ifndef _UNIV_DEFAULT
#define _UNIV_DEFAULT	"att"
#endif

static char	null[1];

typedef struct Feature_s
{
	struct Feature_s*next;
	const char*	name;
	char*		value;
	char*		strict;
	short		length;
	short		standard;
	short		flags;
	short		op;
} Feature_t;

typedef struct
{
	Conf_t*		conf;
	const char*	name;
	short		flags;
	short		call;
	short		standard;
	short		section;
} Lookup_t;

static Feature_t	dynamic[] =
{
	{
		&dynamic[1],
		"CONFORMANCE",
		"ast",
		"standard",
		11,
		CONF_AST,
		0,
		OP_conformance
	},
	{
		&dynamic[2],
		"FS_3D",
		&null[0],
		"0",
		5,
		CONF_AST,
		0,
		OP_fs_3d
	},
	{
		&dynamic[3],
		"HOSTTYPE",
		HOSTTYPE,
		&null[0],
		8,
		CONF_AST,
		CONF_READONLY,
		OP_hosttype
	},
	{
		&dynamic[4],
		"LIBPATH",
#ifdef CONF_LIBPATH
		CONF_LIBPATH,
#else
		&null[0],
#endif
		&null[0],
		7,
		CONF_AST,
		0,
		OP_libpath
	},
	{
		&dynamic[5],
		"LIBPREFIX",
#ifdef CONF_LIBPREFIX
		CONF_LIBPREFIX,
#else
		"lib",
#endif
		&null[0],
		7,
		CONF_AST,
		0,
		OP_libprefix
	},
	{
		&dynamic[6],
		"LIBSUFFIX",
#ifdef CONF_LIBSUFFIX
		CONF_LIBSUFFIX,
#else
		".so",
#endif
		&null[0],
		7,
		CONF_AST,
		0,
		OP_libsuffix
	},
	{
		&dynamic[7],
		"PATH_ATTRIBUTES",
#if _WINIX
		"c",
#else
		&null[0],
#endif
		&null[0],
		15,
		CONF_AST,
		CONF_READONLY,
		OP_path_attributes
	},
	{
		&dynamic[8],
		"PATH_RESOLVE",
		&null[0],
		"metaphysical",
		12,
		CONF_AST,
		0,
		OP_path_resolve
	},
	{
		0,
		"UNIVERSE",
		&null[0],
		"att",
		8,
		CONF_AST,
		0,
		OP_universe
	},
	{
		0
	}
};

typedef struct
{

	const char*	id;
	const char*	name;
	Feature_t*	features;

	/* default initialization from here down */

	int		prefix;
	int		synthesizing;

	char*		data;
	char*		last;

	Ast_confdisc_f	notify;

} State_t;

static State_t	state = { "getconf", "_AST_FEATURES", dynamic };

static char*	feature(const char*, const char*, const char*, Error_f);

/*
 * return fmtbuf() copy of s
 */

static char*
buffer(char* s)
{
	return strcpy(fmtbuf(strlen(s) + 1), s);
}

/*
 * synthesize state for fp
 * fp==0 initializes from getenv(state.name)
 * value==0 just does lookup
 * otherwise state is set to value
 */

static char*
synthesize(register Feature_t* fp, const char* path, const char* value)
{
	register char*		s;
	register char*		d;
	register char*		v;
	register int		n;

	if (state.synthesizing)
		return null;
	if (!state.data)
	{
		char*		se;
		char*		de;
		char*		ve;

		state.prefix = strlen(state.name) + 1;
		n = state.prefix + 3 * MAXVAL;
		if (s = getenv(state.name))
			n += strlen(s) + 1;
		n = roundof(n, 32);
		if (!(state.data = newof(0, char, n, 0)))
			return 0;
		state.last = state.data + n - 1;
		strcpy(state.data, state.name);
		state.data += state.prefix - 1;
		*state.data++ = '=';
		if (s)
			strcpy(state.data, s);
		ve = state.data;
		state.synthesizing = 1;
		for (;;)
		{
			for (s = ve; isspace(*s); s++);
			for (d = s; *d && !isspace(*d); d++);
			for (se = d; isspace(*d); d++);
			for (v = d; *v && !isspace(*v); v++);
			for (de = v; isspace(*v); v++);
			if (!*v)
				break;
			for (ve = v; *ve && !isspace(*ve); ve++);
			if (*ve)
				*ve = 0;
			else
				ve = 0;
			*de = 0;
			*se = 0;
			feature(s, d, v, 0);
			*se = ' ';
			*de = ' ';
			if (!ve)
				break;
			*ve++ = ' ';
		}
		state.synthesizing = 0;
	}
	if (!fp)
		return state.data;
	if (!state.last)
	{
		if (!value)
			return 0;
		n = strlen(value);
		goto ok;
	}
	s = (char*)fp->name;
	n = fp->length;
	d = state.data;
	for (;;)
	{
		while (isspace(*d))
			d++;
		if (!*d)
			break;
		if (strneq(d, s, n) && isspace(d[n]))
		{
			if (!value)
			{
				for (d += n + 1; *d && !isspace(*d); d++);
				for (; isspace(*d); d++);
				for (s = d; *s && !isspace(*s); s++);
				n = s - d;
				value = (const char*)d;
				goto ok;
			}
			for (s = d + n + 1; *s && !isspace(*s); s++);
			for (; isspace(*s); s++);
			for (v = s; *s && !isspace(*s); s++);
			n = s - v;
			if (strneq(v, value, n))
				goto ok;
			for (; isspace(*s); s++);
			if (*s)
				for (; *d = *s++; d++);
			else if (d != state.data)
				d--;
			break;
		}
		for (; *d && !isspace(*d); d++);
		for (; isspace(*d); d++);
		for (; *d && !isspace(*d); d++);
		for (; isspace(*d); d++);
		for (; *d && !isspace(*d); d++);
	}
	if (!value)
	{
		if (!fp->op)
		{
			if (fp->flags & CONF_ALLOC)
				fp->value[0] = 0;
			else
				fp->value = null;
		}
		return 0;
	}
	if (!value[0])
		value = "0";
	if (!path || !path[0] || path[0] == '/' && !path[1])
		path = "-";
	n += strlen(path) + strlen(value) + 3;
	if (d + n >= state.last)
	{
		int	c;
		int	i;

		i = d - state.data;
		state.data -= state.prefix;
		c = n + state.last - state.data + 3 * MAXVAL;
		c = roundof(c, 32);
		if (!(state.data = newof(state.data, char, c, 0)))
			return 0;
		state.last = state.data + c - 1;
		state.data += state.prefix;
		d = state.data + i;
	}
	if (d != state.data)
		*d++ = ' ';
	for (s = (char*)fp->name; *d = *s++; d++);
	*d++ = ' ';
	for (s = (char*)path; *d = *s++; d++);
	*d++ = ' ';
	for (s = (char*)value; *d = *s++; d++);
	setenviron(state.data - state.prefix);
	if (state.notify)
		(*state.notify)(NiL, NiL, state.data - state.prefix);
	n = s - (char*)value - 1;
 ok:
	if (!(fp->flags & CONF_ALLOC))
		fp->value = 0;
	if (n == 1 && (*value == '0' || *value == '-'))
		n = 0;
	if (!(fp->value = newof(fp->value, char, n, 1)))
		fp->value = null;
	else
	{
		fp->flags |= CONF_ALLOC;
		memcpy(fp->value, value, n);
		fp->value[n] = 0;
	}
	return fp->value;
}

/*
 * initialize the value for fp
 * if command!=0 then it is checked for on $PATH
 * synthesize(fp,path,succeed) called on success
 * otherwise synthesize(fp,path,fail) called
 */

static void
initialize(register Feature_t* fp, const char* path, const char* command, const char* succeed, const char* fail)
{
	register char*	p;
	register int	ok = 1;

	switch (fp->op)
	{
	case OP_conformance:
		ok = getenv("POSIXLY_CORRECT") != 0;
		break;
	case OP_hosttype:
		ok = 1;
		break;
	case OP_path_attributes:
		ok = 1;
		break;
	case OP_path_resolve:
		ok = fs3d(FS3D_TEST);
		break;
	case OP_universe:
		ok = streq(_UNIV_DEFAULT, "att");
		/*FALLTHROUGH...*/
	default:
		if (p = getenv("PATH"))
		{
			register int	r = 1;
			register char*	d = p;
			Sfio_t*		tmp;

			if (tmp = sfstropen())
			{
				for (;;)
				{
					switch (*p++)
					{
					case 0:
						break;
					case ':':
						if (command && (fp->op != OP_universe || !ok))
						{
							if (r = p - d - 1)
							{
								sfwrite(tmp, d, r);
								sfputc(tmp, '/');
								sfputr(tmp, command, 0);
								if (!access(sfstruse(tmp), X_OK))
								{
									ok = 1;
									if (fp->op != OP_universe)
										break;
								}
							}
							d = p;
						}
						r = 1;
						continue;
					case '/':
						if (r)
						{
							r = 0;
							if (fp->op == OP_universe)
							{
								if (strneq(p, "bin:", 4) || strneq(p, "usr/bin:", 8))
									break;
							}
						}
						if (fp->op == OP_universe)
						{
							if (strneq(p, "5bin", 4))
							{
								ok = 1;
								break;
							}
							if (strneq(p, "bsd", 3) || strneq(p, "ucb", 3))
							{
								ok = 0;
								break;
							}
						}
						continue;
					default:
						r = 0;
						continue;
					}
					break;
				}
				sfclose(tmp);
			}
			else
				ok = 1;
		}
		break;
	}
	synthesize(fp, path, ok ? succeed : fail);
}

/*
 * value==0 get feature name
 * value!=0 set feature name
 * 0 returned if error or not defined; otherwise previous value
 */

static char*
feature(const char* name, const char* path, const char* value, Error_f conferror)
{
	register Feature_t*	fp;
	register int		n;
	register Feature_t*	sp;

	if (value && (streq(value, "-") || streq(value, "0")))
		value = null;
	for (fp = state.features; fp && !streq(fp->name, name); fp = fp->next);
	if (!fp)
	{
		if (!value)
			return 0;
		if (state.notify && !(*state.notify)(name, path, value))
			return 0;
		n = strlen(name);
		if (!(fp = newof(0, Feature_t, 1, n + 1)))
		{
			if (conferror)
				(*conferror)(&state, &state, 2, "%s: out of space", name);
			return 0;
		}
		fp->name = (const char*)fp + sizeof(Feature_t);
		strcpy((char*)fp->name, name);
		fp->length = n;
		fp->next = state.features;
		state.features = fp;
	}
	else if (value)
	{
		if (fp->flags & CONF_READONLY)
		{
			if (conferror)
				(*conferror)(&state, &state, 2, "%s: cannot set readonly symbol", fp->name);
			return 0;
		}
		if (state.notify && !streq(fp->value, value) && !(*state.notify)(name, path, value))
			return 0;
	}
	switch (fp->op)
	{

	case OP_conformance:
		if (value && (streq(value, "strict") || streq(value, "posix") || streq(value, "xopen")))
			value = fp->strict;
		n = streq(fp->value, fp->strict);
		if (!synthesize(fp, path, value))
			initialize(fp, path, NiL, fp->strict, fp->value);
		if (!n && streq(fp->value, fp->strict))
			for (sp = state.features; sp; sp = sp->next)
				if (sp->op && sp->op != OP_conformance)
					astconf(sp->name, path, sp->strict);
		break;

	case OP_fs_3d:
		fp->value = fs3d(value ? value[0] ? FS3D_ON : FS3D_OFF : FS3D_TEST) ? "1" : null;
		break;

	case OP_hosttype:
		break;

	case OP_path_attributes:
#ifdef _PC_PATH_ATTRIBUTES
		{
			register char*	s;
			register char*	e;
			register long	v;

			/*
			 * _PC_PATH_ATTRIBUTES is a bitmap for 'a' to 'z'
			 */

			if ((v = pathconf(path, _PC_PATH_ATTRIBUTES)) == -1L)
				return 0;
			s = fp->value;
			e = s + sizeof(fp->value) - 1;
			for (n = 'a'; n <= 'z'; n++)
				if (v & (1 << (n - 'a')))
				{
					*s++ = n;
					if (s >= e)
						break;
				}
			*s = 0;
		}
#endif
		break;

	case OP_path_resolve:
		if (!synthesize(fp, path, value))
			initialize(fp, path, NiL, "logical", "metaphysical");
		break;

	case OP_universe:
#if _lib_universe
		if (getuniverse(fp->value) < 0)
			strcpy(fp->value, "att");
		if (value)
			setuniverse(value);
#else
#ifdef UNIV_MAX
		n = 0;
		if (value)
		{
			while (n < univ_max && !streq(value, univ_name[n])
				n++;
			if (n >= univ_max)
			{
				if (conferror)
					(*conferror)(&state, &state, 2, "%s: %s: universe value too large", fp->name, value);
				return 0;
			}
		}
#ifdef ATT_UNIV
		n = setuniverse(n + 1);
		if (!value && n > 0)
			setuniverse(n);
#else
		n = universe(value ? n + 1 : U_GET);
#endif
		if (n <= 0 || n >= univ_max)
			n = 1;
		strcpy(fp->value, univ_name[n - 1]);
#else
		if (!synthesize(fp, path, value))
			initialize(fp, path, "echo", "att", "ucb");
#endif
#endif
		break;

	default:
		synthesize(fp, path, value);
		break;

	}
	return fp->value;
}

/*
 * binary search for name in conf[]
 */

static int
lookup(register Lookup_t* look, const char* name)
{
	register Conf_t*	mid = (Conf_t*)conf;
	register Conf_t*	lo = mid;
	register Conf_t*	hi = mid + conf_elements;
	register int		v;
	register int		c;
	const Prefix_t*		p;

	look->flags = 0;
	look->call = -1;
	look->standard = -1;
	look->section = -1;
	while (*name == '_')
		name++;
 again:
	for (p = prefix; p < &prefix[prefix_elements]; p++)
		if (strneq(name, p->name, p->length) && ((c = name[p->length] == '_') || (v = isdigit(name[p->length]) && name[p->length + 1] == '_')))
		{
			if (p->call < 0)
			{
				if (look->standard >= 0)
					break;
				look->standard = p->standard;
			}
			else
			{
				if (look->call >= 0)
					break;
				look->call = p->call;
			}
			name += p->length + c;
			if (look->section < 0 && !c && v)
			{
				look->section = name[0] - '0';
				name += 2;
			}
			goto again;
		}
	if (look->section < 0)
		look->section = 1;
	look->name = name;
	c = *((unsigned char*)name);
	while (lo <= hi)
	{
		mid = lo + (hi - lo) / 2;
		if (!(v = c - *((unsigned char*)mid->name)) && !(v = strcmp(name, mid->name)))
		{
			lo = (Conf_t*)conf;
			hi = mid;
			do
			{
				if ((look->standard < 0 || look->standard == mid->standard) &&
				    (look->section < 0 || look->section == mid->section) &&
				    (look->call < 0 || look->call == mid->call))
					goto found;
			} while (mid-- > lo && streq(mid->name, look->name));
			mid = hi;
			hi = lo + conf_elements - 1;
			while (++mid < hi && streq(mid->name, look->name))
			{
				if ((look->standard < 0 || look->standard == mid->standard) &&
				    (look->section < 0 || look->section == mid->section) &&
				    (look->call < 0 || look->call == mid->call))
					goto found;
			}
			break;
		}
		else if (v > 0)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return 0;
 found:
	if (look->call < 0 && look->standard >= 0)
		look->flags |= CONF_MINMAX;
	look->conf = mid;
	return 1;
}

/*
 * return a tolower'd copy of s
 */

static char*
fmtlower(register const char* s)
{
	register int	c;
	register char*	t;
	char*		b;

	b = t = fmtbuf(strlen(s) + 1);
	while (c = *s++)
	{
		if (isupper(c))
			c = tolower(c);
		*t++ = c;
	}
	*t = 0;
	return b;
}

/*
 * print value line for p
 * if !name then value prefixed by "p->name="
 * if (flags & CONF_MINMAX) then default minmax value used
 */

static char*
print(Sfio_t* sp, register Lookup_t* look, const char* name, const char* path, int listflags, Error_f conferror)
{
	register Conf_t*	p = look->conf;
	register int		flags = look->flags|CONF_DEFINED;
	char*			call;
	char*			f;
	long			v;
	int			olderrno;
	int			drop;
	char			buf[PATH_MAX];
	char			flg[16];

	if (!name && !(p->flags & CONF_STRING) && (p->flags & (CONF_FEATURE|CONF_LIMIT|CONF_MINMAX)) && (p->flags & (CONF_LIMIT|CONF_PREFIXED)) != CONF_LIMIT)
	{
		flags |= CONF_PREFIXED;
		if (p->flags & CONF_DEFINED)
			flags |= CONF_MINMAX;
	}
	olderrno = errno;
	errno = 0;
	switch ((flags & CONF_MINMAX) && (p->flags & CONF_DEFINED) ? 0 : p->call)
	{
	case 0:
		if (p->flags & CONF_DEFINED)
			v = p->value;
		else
		{
			flags &= ~CONF_DEFINED;
			v = -1;
		}
		break;
	case CONF_confstr:
		call = "confstr";
		if (!(v = confstr(p->op, buf, sizeof(buf))))
		{
			v = -1;
			errno = EINVAL;
		}
		break;
	case CONF_pathconf:
		call = "pathconf";
		v = pathconf(path, p->op);
		break;
	case CONF_sysconf:
		call = "sysconf";
		v = sysconf(p->op);
		break;
	case CONF_sysinfo:
		call = "sysinfo";
		v = sysinfo(p->op, buf, sizeof(buf));
		break;
	default:
		call = "synthesis";
		errno = EINVAL;
		v = -1;
		break;
	}
	if (v == -1)
	{
		if (!errno)
		{
			if ((p->flags & CONF_FEATURE) || !(p->flags & (CONF_LIMIT|CONF_MINMAX)))
				flags &= ~CONF_DEFINED;
		}
		else if (!(flags & CONF_PREFIXED))
		{
			if (!sp)
			{
				if (conferror)
				{
					(*conferror)(&state, &state, ERROR_SYSTEM|2, "%s: %s error", p->name, call);
					return 0;
				}
				return null;
			}
			flags &= ~CONF_DEFINED;
			flags |= CONF_ERROR;
		}
		else
			flags &= ~CONF_DEFINED;
	}
	errno = olderrno;
	if ((listflags & ASTCONF_defined) && !(flags & CONF_DEFINED))
		return null;
	if ((drop = !sp) && !(sp = sfstropen()))
		return null;
	if (listflags & ASTCONF_table)
	{
		f = flg;
		if (p->flags & CONF_DEFINED)
			*f++ = 'D';
		if (p->flags & CONF_FEATURE)
			*f++ = 'F';
		if (p->flags & CONF_LIMIT)
			*f++ = 'L';
		if (p->flags & CONF_MINMAX)
			*f++ = 'M';
		if (p->flags & CONF_NOSECTION)
			*f++ = 'N';
		if (p->flags & CONF_PREFIXED)
			*f++ = 'P';
		if (p->flags & CONF_STANDARD)
			*f++ = 'S';
		if (p->flags & CONF_UNDERSCORE)
			*f++ = 'U';
		if (f == flg)
			*f++ = 'X';
		*f = 0;
		sfprintf(sp, "%*s %*s %d %2s %5s ", sizeof(p->name), p->name, sizeof(prefix[p->standard].name), prefix[p->standard].name, p->section, prefix[p->call + CONF_call].name, flg);
		if (flags & CONF_ERROR)
			sfprintf(sp, "error");
		else if (p->flags & CONF_STRING)
			sfprintf(sp, "%s", (listflags & ASTCONF_quote) ? fmtquote(buf, "\"", "\"", strlen(buf), FMT_SHELL) : buf);
		else if (v != -1)
			sfprintf(sp, "%ld", v);
		else if (flags & CONF_DEFINED)
			sfprintf(sp, "%lu", v);
		else
			sfprintf(sp, "undefined");
		sfprintf(sp, "\n");
	}
	else
	{
		if (!(flags & CONF_PREFIXED) || (listflags & ASTCONF_base))
		{
			if (!name)
			{
				if ((p->flags & (CONF_PREFIXED|CONF_STRING)) == (CONF_PREFIXED|CONF_STRING) && (!(listflags & ASTCONF_base) || p->standard != CONF_POSIX))
				{
					if ((p->flags & CONF_UNDERSCORE) && !(listflags & ASTCONF_base))
						sfprintf(sp, "_");
					sfprintf(sp, "%s", (listflags & ASTCONF_lower) ? fmtlower(prefix[p->standard].name) : prefix[p->standard].name);
					if (p->section > 1)
						sfprintf(sp, "%d", p->section);
					sfprintf(sp, "_");
				}
				sfprintf(sp, "%s=", (listflags & ASTCONF_lower) ? fmtlower(p->name) : p->name);
			}
			if (flags & CONF_ERROR)
				sfprintf(sp, "error");
			else if (p->flags & CONF_STRING)
				sfprintf(sp, "%s", (listflags & ASTCONF_quote) ? fmtquote(buf, "\"", "\"", strlen(buf), FMT_SHELL) : buf);
			else if (v != -1)
				sfprintf(sp, "%ld", v);
			else if (flags & CONF_DEFINED)
				sfprintf(sp, "%lu", v);
			else
				sfprintf(sp, "undefined");
			if (!name)
				sfprintf(sp, "\n");
		}
		if (!name && !(listflags & ASTCONF_base) && !(p->flags & CONF_STRING) && (p->flags & (CONF_FEATURE|CONF_MINMAX)))
		{
			if (p->flags & CONF_UNDERSCORE)
				sfprintf(sp, "_");
			sfprintf(sp, "%s", (listflags & ASTCONF_lower) ? fmtlower(prefix[p->standard].name) : prefix[p->standard].name);
			if (p->section > 1)
				sfprintf(sp, "%d", p->section);
			sfprintf(sp, "_%s=", (listflags & ASTCONF_lower) ? fmtlower(p->name) : p->name);
			if (p->flags & CONF_DEFINED)
			{
				if ((v = p->value) == -1 && ((p->flags & CONF_FEATURE) || !(p->flags & (CONF_LIMIT|CONF_MINMAX))))
					flags &= ~CONF_DEFINED;
				else
					flags |= CONF_DEFINED;
			}
			if (v != -1)
				sfprintf(sp, "%ld", v);
			else if (flags & CONF_DEFINED)
				sfprintf(sp, "%lu", v);
			else
				sfprintf(sp, "undefined");
			sfprintf(sp, "\n");
		}
	}
	if (drop)
	{
		call = buffer(sfstruse(sp));
		sfclose(sp);
		return call;
	}
	return null;
}

/*
 * value==0 gets value for name
 * value!=0 sets value for name and returns previous value
 * path==0 implies path=="/"
 *
 * settable return values are in permanent store
 * non-settable return values copied to a tmp fmtbuf() buffer
 *
 *	if (!strcmp(astgetconf("PATH_RESOLVE", NiL, NiL), "logical", 0))
 *		our_way();
 *
 *	universe = astgetconf("UNIVERSE", NiL, "att", 0);
 *	astgetconf("UNIVERSE", NiL, universe, 0);
 */

#define ALT	16

char*
astgetconf(const char* name, const char* path, const char* value, Error_f conferror)
{
	register char*	s;
	char*		e;
	int		n;
	long		v;
	Lookup_t	look;
	Sfio_t*		tmp;

	if (!name)
	{
		if (path)
			return null;
		if (!(name = value))
		{
			if (state.data)
			{
				Ast_confdisc_f	notify;

#if _HUH20000515 /* doesn't work for shell builtins */
				free(state.data - state.prefix);
#endif
				state.data = 0;
				notify = state.notify;
				state.notify = 0;
				INITIALIZE();
				state.notify = notify;
			}
			return null;
		}
		value = 0;
	}
	INITIALIZE();
	if (!path)
		path = "/";
	if (isdigit(*name))
	{
		n = (int)strtol(name, &e, 10);
		if (!*e)
		{
			if (value)
				goto ro;
			v = sysconf(n);
			if (v == -1)
				return "error";
			s = fmtbuf(n = 16);
			sfsprintf(s, n, "%lu", v);
			return s;
		}
	}
	if (lookup(&look, name))
	{
		if (value)
		{
		ro:
			errno = EINVAL;
			if (conferror)
			{
				(*conferror)(&state, &state, 2, "%s: cannot set value", name);
				return 0;
			}
			return null;
		}
		s = print(NiL, &look, name, path, 0, conferror);
		return s;
	}
	if ((n = strlen(name)) > 3 && n < (ALT + 3))
	{
		if (!strcmp(name + n - 3, "DEV"))
		{
			if (tmp = sfstropen())
			{
				sfprintf(tmp, "/dev/");
				for (s = (char*)name; s < (char*)name + n - 3; s++)
					sfputc(tmp, isupper(*s) ? tolower(*s) : *s);
				s = sfstruse(tmp);
				if (!access(s, F_OK))
				{
					if (value)
						goto ro;
					s = buffer(s);
					sfclose(tmp);
					return s;
				}
				sfclose(tmp);
			}
		}
		else if (!strcmp(name + n - 3, "DIR"))
		{
			Lookup_t		altlook;
			char			altname[ALT];

			static const char*	dirs[] = { "/usr/lib", "/usr", null };

			strcpy(altname, name);
			altname[n - 3] = 0;
			if (lookup(&altlook, altname))
			{
				if (value)
				{
					errno = EINVAL;
					if (conferror)
					{
						(*conferror)(&state, &state, 2, "%s: cannot set value", altname);
						return 0;
					}
					return null;
				}
				return print(NiL, &altlook, altname, path, 0, conferror);
			}
			for (s = altname; *s; s++)
				if (isupper(*s))
					*s = tolower(*s);
			if (tmp = sfstropen())
			{
				for (n = 0; n < elementsof(dirs); n++)
				{
					sfprintf(tmp, "%s/%s/.", dirs[n], altname);
					s = sfstruse(tmp);
					if (!access(s, F_OK))
					{
						if (value)
							goto ro;
						s = buffer(s);
						sfclose(tmp);
						return s;
					}
				}
				sfclose(tmp);
			}
		}
	}
	if ((look.standard < 0 || look.standard == CONF_AST) && look.call <= 0 && look.section <= 1 && (s = feature(look.name, path, value, conferror)))
		return s;
	errno = EINVAL;
	if (conferror)
		return 0;
	return null;
}

char*
astconf(const char* name, const char* path, const char* value)
{
	return astgetconf(name, path, value, 0);
}

/*
 * set discipline function to be called when features change
 * old discipline function returned
 */

Ast_confdisc_f
astconfdisc(Ast_confdisc_f new_notify)
{
	Ast_confdisc_f	old_notify;

	INITIALIZE();
	old_notify = state.notify;
	state.notify = new_notify;
	return old_notify;
}

/*
 * list all name=value entries on sp
 * path==0 implies path=="/"
 */

void
astconflist(Sfio_t* sp, const char* path, int flags, const char* pattern)
{
	char*		s;
	char*		f;
	char*		call;
	Feature_t*	fp;
	Lookup_t	look;
	regex_t		re;
	regdisc_t	redisc;
	int		olderrno;
	char		flg[8];

	INITIALIZE();
	if (!path)
		path = "/";
	else if (access(path, F_OK))
	{
		errorf(&state, &state, 2, "%s: not found", path);
		return;
	}
	olderrno = errno;
	look.flags = 0;
	if (!(flags & (ASTCONF_read|ASTCONF_write|ASTCONF_parse)))
		flags |= ASTCONF_read|ASTCONF_write;
	else if (flags & ASTCONF_parse)
		flags |= ASTCONF_write;
	if (!(flags & (ASTCONF_matchcall|ASTCONF_matchname|ASTCONF_matchstandard)))
		pattern = 0;
	if (pattern)
	{
		memset(&redisc, 0, sizeof(redisc));
		redisc.re_version = REG_VERSION;
		redisc.re_errorf = (regerror_t)errorf;
		re.re_disc = &redisc;
		if (regcomp(&re, pattern, REG_DISCIPLINE|REG_EXTENDED|REG_LENIENT|REG_NULL))
			return;
	}
	if (flags & ASTCONF_read)
		for (look.conf = (Conf_t*)conf; look.conf < (Conf_t*)&conf[conf_elements]; look.conf++)
		{
			if (pattern)
			{
				if (flags & ASTCONF_matchcall)
				{
					if (regexec(&re, prefix[look.conf->call + CONF_call].name, 0, NiL, 0))
						continue;
				}
				else if (flags & ASTCONF_matchname)
				{
					if (regexec(&re, look.conf->name, 0, NiL, 0))
						continue;
				}
				else if (flags & ASTCONF_matchstandard)
				{
					if (regexec(&re, prefix[look.conf->standard].name, 0, NiL, 0))
						continue;
				}
			}
			print(sp, &look, NiL, path, flags, errorf);
		}
	if (flags & ASTCONF_write)
	{
		call = "GC";
		for (fp = state.features; fp; fp = fp->next)
		{
			if (pattern)
			{
				if (flags & ASTCONF_matchcall)
				{
					if (regexec(&re, call, 0, NiL, 0))
						continue;
				}
				else if (flags & ASTCONF_matchname)
				{
					if (regexec(&re, fp->name, 0, NiL, 0))
						continue;
				}
				else if (flags & ASTCONF_matchstandard)
				{
					if (regexec(&re, prefix[fp->standard].name, 0, NiL, 0))
						continue;
				}
			}
			if (!*(s = feature(fp->name, path, NiL, 0)))
				s = "0";
			if (flags & ASTCONF_table)
			{
				f = flg;
				if (fp->flags & CONF_ALLOC)
					*f++ = 'A';
				if (fp->flags & CONF_READONLY)
					*f++ = 'R';
				if (f == flg)
					*f++ = 'X';
				*f = 0;
				sfprintf(sp, "%*s %*s %d %2s %5s %s\n", sizeof(conf[0].name), fp->name, sizeof(prefix[fp->standard].name), prefix[fp->standard].name, 1, call, flg, s);
			}
			else if (flags & ASTCONF_parse)
				sfprintf(sp, "%s %s - %s\n", state.id, (flags & ASTCONF_lower) ? fmtlower(fp->name) : fp->name, s); 
			else
				sfprintf(sp, "%s=%s\n", (flags & ASTCONF_lower) ? fmtlower(fp->name) : fp->name, (flags & ASTCONF_quote) ? fmtquote(s, "\"", "\"", strlen(s), FMT_SHELL) : s);
		}
	}
	if (pattern)
		regfree(&re);
	errno = olderrno;
}
