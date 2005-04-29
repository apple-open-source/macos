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
 * AT&T Bell Laboratories
 *
 * time conversion support
 */

#include <ast.h>
#include <tm.h>

/*
 * return Tm_t for clock
 * time zone and leap seconds accounted for in return value
 */

Tm_t*
tmmake(time_t* clock)
{
	register Tm_t*		tp;
	register Tm_leap_t*	lp;
	int			leapsec;
	time_t			now;

	tmset(tm_info.zone);
	if (clock)
		now = *clock;
	else
		time(&now);
	leapsec = 0;
	if ((tm_info.flags & (TM_ADJUST|TM_LEAP)) == (TM_ADJUST|TM_LEAP) && now > 0)
	{
		for (lp = &tm_data.leap[0]; now < lp->time; lp++);
		if (lp->total)
		{
			if (now == lp->time && (leapsec = (lp->total - (lp+1)->total)) < 0)
				leapsec = 0;
			now -= lp->total;
		}
	}
	if (!(tm_info.flags & TM_UTC))
	{
		now += 60 * ((tm_info.local->west + (tm_info.local->daylight ? tm_info.local->dst : 0)) - (tm_info.zone->west + (tm_info.zone->daylight ? tm_info.zone->dst : 0)));
		if (!(tp = (Tm_t*)localtime(&now)))
		{
			now = 0;
			tp = (Tm_t*)localtime(&now);
		}
	}
	else if (!(tp = (Tm_t*)gmtime(&now)))
	{
		now = 0;
		tp = (Tm_t*)gmtime(&now);
	}
	if (tp)
		tp->tm_sec += leapsec;
	return tp;
}
