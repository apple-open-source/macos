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

#define DAYS(p)	(tm_data.days[(p)->tm_mon]+((p)->tm_mon==1&&LEAP(p)))
#define LEAP(p)	(tmisleapyear((p)->tm_year))

/*
 * correct out of bounds fields in tm
 *
 * tm_wday, tm_yday and tm_isdst are not changed
 * as these can be computed from the other fields
 *
 * tm is the return value
 */

Tm_t*
tmfix(register Tm_t* tm)
{
	register int	n;
	register int	w;
	Tm_t*		p;
	time_t		t;

	/*
	 * check for special case that adjusts tm_wday at the end
	 * this happens during
	 *	nl_langinfo() => strftime() => tmfmt()
	 */

	if (w = !(tm->tm_sec | tm->tm_min | tm->tm_mday | tm->tm_year | tm->tm_yday | tm->tm_isdst))
	{
		tm->tm_year = 99;
		tm->tm_mday = 2;
	}

	/*
	 * adjust from shortest to longest units
	 */

	if ((n = tm->tm_sec) < 0)
	{
		tm->tm_min -= (60 - n) / 60;
		tm->tm_sec = 60 - (-n) % 60;
	}
	else if (n > (59 + TM_MAXLEAP))
	{
		tm->tm_min += n / 60;
		tm->tm_sec %= 60;
	}
	if ((n = tm->tm_min) < 0)
	{
		tm->tm_hour -= (60 - n) / 60;
		tm->tm_min = 60 - (-n) % 60;
	}
	else if (n > 59)
	{
		tm->tm_hour += n / 60;
		tm->tm_min %= 60;
	}
	if ((n = tm->tm_hour) < 0)
	{
		tm->tm_mday -= (23 - n) / 24;
		tm->tm_hour = 24 - (-n) % 24;
	}
	else if (n > 24)
	{
		tm->tm_mday += n / 24;
		tm->tm_hour %= 24;
	}
	if (tm->tm_mon >= 12)
	{
		tm->tm_year += tm->tm_mon / 12;
		tm->tm_mon %= 12;
	}
	else if (tm->tm_mon < 0)
	{
		tm->tm_year -= (12 - tm->tm_mon) / 12;
		tm->tm_mon = (12 - tm->tm_mon) % 12;
	}
	while (tm->tm_mday < -365)
	{
		tm->tm_year--;
		tm->tm_mday += 365 + LEAP(tm);
	}
	while (tm->tm_mday < 1)
	{
		if (--tm->tm_mon < 0)
		{
			tm->tm_mon = 11;
			tm->tm_year--;
		}
		tm->tm_mday += DAYS(tm);
	}
	while (tm->tm_mday > 365)
	{
		tm->tm_mday -= 365 + LEAP(tm);
		tm->tm_year++;
	}
	while (tm->tm_mday > (n = DAYS(tm)))
	{
		tm->tm_mday -= n;
		if (++tm->tm_mon > 11)
		{
			tm->tm_mon = 0;
			tm->tm_year++;
		}
	}
	if (w)
	{
		w = tm->tm_wday;
		t = tmtime(tm, TM_LOCALZONE);
		p = tmmake(&t);
		if (w = (w - p->tm_wday))
		{
			if (w < 0)
				w += 7;
			tm->tm_wday += w;
			if ((tm->tm_mday += w) > DAYS(tm))
				tm->tm_mday -= 7;
		}
	}

	/*
	 * tm_isdst is adjusted by tmtime()
	 */

	return tm;
}
