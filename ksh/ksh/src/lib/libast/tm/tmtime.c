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
 * convert Tm_t to time_t
 *
 * if west==TM_LOCALZONE then the local timezone is used
 * otherwise west is the number of minutes west
 * of GMT with DST taken into account
 *
 * this routine works with a copy of Tm_t to avoid clashes
 * with the low level localtime() and gmtime()
 */

time_t
tmtime(register Tm_t* tm, int west)
{
	register _ast_int4_t	clock;
	register Tm_leap_t*	lp;
	register Tm_t*		tn;
	Tm_t*			to;
	int			n;
	int			sec;
	time_t			now;
	Tm_t			ts;

	ts = *tm;
	to = tm;
	tm = &ts;
	tmset(tm_info.zone);
	tmfix(tm);
	if (tm->tm_year < 69 || tm->tm_year > (2038 - 1900))
		return -1;
	clock = (tm->tm_year * (4 * 365 + 1) - 69) / 4 - 70 * 365;
	if ((n = tm->tm_mon) > 11)
		n = 11;
	if (n > 1 && !(tm->tm_year % 4) && ((tm->tm_year % 100) || !((1900 + tm->tm_year) % 400)))
		clock++;
	clock += tm_data.sum[n] + tm->tm_mday - 1;
	clock *= 24;
	clock += tm->tm_hour;
	clock *= 60;
	clock += tm->tm_min;
	clock *= 60;
	clock += sec = tm->tm_sec;
	tn = 0;
	if (!(tm_info.flags & TM_UTC))
	{
		/*
		 * time zone adjustments
		 */

		if (west == TM_LOCALZONE)
		{
			clock += tm_info.zone->west * 60;
			if (!tm_info.zone->daylight)
				tm->tm_isdst = 0;
			else
			{
				now = clock;
				if (!(tn = tmmake(&now)))
					return -1;
				if (tm->tm_isdst = tn->tm_isdst)
					clock += tm_info.zone->dst * 60;
			}
		}
		else
		{
			clock += west * 60;
			if (!tm_info.zone->daylight)
				tm->tm_isdst = 0;
			else if (tm->tm_isdst < 0)
			{
				now = clock;
				if (!(tn = tmmake(&now)))
					return -1;
				tm->tm_isdst = tn->tm_isdst;
			}
		}
	}
	else if (tm->tm_isdst)
		tm->tm_isdst = 0;
	if (!tn)
	{
		now = clock;
		if (!(tn = tmmake(&now)))
			return -1;
	}
	tm->tm_wday = tn->tm_wday;
	tm->tm_yday = tn->tm_yday;
	*to = *tm;
	if (tm_info.flags & TM_LEAP)
	{
		/*
		 * leap second adjustments
		 */

		if (clock > 0)
		{
			for (lp = &tm_data.leap[0]; clock < lp->time - (lp+1)->total; lp++);
			clock += lp->total;
			n = lp->total - (lp+1)->total;
			if (clock <= (lp->time + n) && (n > 0 && sec > 59 || n < 0 && sec > (59 + n) && sec <= 59))
				clock -= n;
		}
	}
	if (clock < 0)
		return -1;
	return clock;
}
