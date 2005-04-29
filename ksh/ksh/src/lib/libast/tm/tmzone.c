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
 * Glenn Fowler
 * AT&T Research
 *
 * time conversion support
 */

#include <ast.h>
#include <tm.h>

/*
 * return timezone pointer given name and type
 *
 * if type==0 then all time zone types match
 * otherwise type must be one of tm_info.zone[].type
 *
 * if end is non-null then it will point to the next
 * unmatched char in name
 *
 * if dst!=0 then it will point to 0 for standard zones
 * and the offset for daylight zones
 *
 * 0 returned for no match
 */

Tm_zone_t*
tmzone(register const char* name, char** end, const char* type, int* dst)
{
	register Tm_zone_t*	zp;
	register char*		prev;
	char*			e;

	static Tm_zone_t	fixed;
	static char		off[16];

	tmset(tm_info.zone);
	if ((*name == '+' || *name == '-') && (fixed.west = tmgoff(name, &e, 0)) && !*e)
	{
		fixed.standard = fixed.daylight = strncpy(off, name, sizeof(off) - 1);
		if (end)
			*end = e;
		if (dst)
			*dst = 0;
		return &fixed;
	}
	zp = tm_info.local;
	prev = 0;
	do
	{
		if (zp->type)
			prev = zp->type;
		if (!type || type == prev || !prev)
		{
			if (tmword(name, end, zp->standard, NiL, 0))
			{
				if (dst)
					*dst = 0;
				return zp;
			}
			if (zp->dst && zp->daylight && tmword(name, end, zp->daylight, NiL, 0))
			{
				if (dst)
					*dst = zp->dst;
				return zp;
			}
		}
		if (zp == tm_info.local)
			zp = tm_data.zone;
		else
			zp++;
	} while (zp->standard);
	return 0;
}
