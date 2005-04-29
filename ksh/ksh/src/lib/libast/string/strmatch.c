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
 * D. G. Korn
 * G. S. Fowler
 * AT&T Research
 *
 * match shell file patterns
 * this interface is a wrapper on regex
 *
 *	sh pattern	egrep RE	description
 *	----------	--------	-----------
 *	*		.*		0 or more chars
 *	?		.		any single char
 *	[.]		[.]		char class
 *	[!.]		[^.]		negated char class
 *	[[:.:]]		[[:.:]]		ctype class
 *	[[=.=]]		[[=.=]]		equivalence class
 *	[[...]]		[[...]]		collation element
 *	*(.)		(.)*		0 or more of
 *	+(.)		(.)+		1 or more of
 *	?(.)		(.)?		0 or 1 of
 *	(.)		(.)		1 of
 *	@(.)		(.)		1 of
 *	a|b		a|b		a or b
 *	\#				() subgroup back reference [1-9]
 *	a&b				a and b
 *	!(.)				none of
 *
 * \ used to escape metacharacters
 *
 *	*, ?, (, |, &, ), [, \ must be \'d outside of [...]
 *	only ] must be \'d inside [...]
 *
 */

#include <ast.h>
#include <regex.h>

typedef struct Cache_s
{
	regex_t		re;
	regmatch_t	match[32];
	unsigned long	serial;
	int		flags;
	int		n;
	int		keep;
	int		reflags;
	char		pattern[128];
} Cache_t;

static Cache_t*		cache[8];

static unsigned long	serial;

/*
 * subgroup match
 * 0 returned if no match
 * otherwise number of subgroups matched returned
 * match group begin offsets are even elements of sub
 * match group end offsets are odd elements of sub
 * the matched string is from s+sub[0] up to but not
 * including s+sub[1]
 */

int
strgrpmatch(const char* b, const char* p, int* sub, int n, int flags)
{
	register Cache_t*	cp;
	register int*		end;
	register int		i;
	int			m;
	int			empty;
	int			unused;
	int			old;
	int			once;

	/*
	 * 0 and empty patterns are special
	 */

	if (!p || !b)
	{
		if (!p && !b)
		{
			/*
			 * flush the cache
			 */

			for (i = 0; i < elementsof(cache); i++)
				if (cache[i] && cache[i]->keep)
				{
					cache[i]->keep = 0;
					regfree(&cache[i]->re);
				}
		}
		return 0;
	}
	if (!*p)
		return *b == 0;

	/*
	 * check if the pattern is in the cache
	 */

	once = 0;
	empty = unused = -1;
	old = 0;
	for (i = 0; i < elementsof(cache); i++)
		if (!cache[i])
			empty = i;
		else if (!cache[i]->keep)
			unused = i;
		else if (streq(cache[i]->pattern, p) && cache[i]->flags == flags && cache[i]->n == n)
			break;
		else if (!cache[old] || cache[old]->serial > cache[i]->serial)
			old = i;
	if (i >= elementsof(cache))
	{
		if (unused < 0)
		{
			if (empty < 0)
				unused = old;
			else
				unused = empty;
		}
		if (!(cp = cache[unused]) && !(cp = cache[unused] = newof(0, Cache_t, 1, 0)))
			return 0;
		if (cp->keep)
		{
			cp->keep = 0;
			regfree(&cp->re);
		}
		cp->flags = flags;
		cp->n = n;
		if (strlen(p) < sizeof(cp->pattern))
		{
			strcpy(cp->pattern, p);
			p = (const char*)cp->pattern;
		}
		else
			once = 1;
		cp->reflags = REG_SHELL|REG_AUGMENTED;
		if (!(flags & STR_MAXIMAL))
			cp->reflags |= REG_MINIMAL;
		if (flags & STR_GROUP)
			cp->reflags |= REG_SHELL_GROUP;
		if (flags & STR_LEFT)
			cp->reflags |= REG_LEFT;
		if (flags & STR_RIGHT)
			cp->reflags |= REG_RIGHT;
		if (flags & STR_ICASE)
			cp->reflags |= REG_ICASE;
		if (!sub)
			cp->reflags |= REG_NOSUB;
		if (regcomp(&cp->re, p, cp->reflags))
			return 0;
		cp->keep = 1;
	}
	else
		cp = cache[i];
#if 0
error(-1, "AHA strmatch b=`%s' p=`%s' sub=%p n=%d flags=%08x cp=%d\n", b, p, sub, n, flags, cp - &cache[0]);
#endif
	cp->serial = ++serial;
	m = regexec(&cp->re, b, cp->n, cp->match, cp->reflags);
	i = cp->re.re_nsub;
	if (once)
	{
		cp->keep = 0;
		regfree(&cp->re);
	}
	if (m)
		return 0;
	if (!sub)
		return 1;
	end = sub + n * 2;
	for (n = 0; sub < end && n <= i; n++)
	{
		*sub++ = cp->match[n].rm_so;
		*sub++ = cp->match[n].rm_eo;
	}
	return i + 1;
}

/*
 * compare the string s with the shell pattern p
 * returns 1 for match 0 otherwise
 */

int
strmatch(const char* s, const char* p)
{
	return strgrpmatch(s, p, NiL, 0, STR_MAXIMAL|STR_LEFT|STR_RIGHT);
}

/*
 * leading substring match
 * first char after end of substring returned
 * 0 returned if no match
 *
 * OBSOLETE: use strgrpmatch()
 */

char*
strsubmatch(const char* s, const char* p, int flags)
{
	int	match[2];

	return strgrpmatch(s, p, match, 1, (flags ? STR_MAXIMAL : 0)|STR_LEFT) ? (char*)s + match[1] : (char*)0;
}
