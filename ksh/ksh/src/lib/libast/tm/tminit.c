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
#include <ctype.h>
#include <namval.h>

#ifndef tzname
#	if defined(__DYNAMIC__)
#		define	tzname		__DYNAMIC__(tzname)
#	else
#		if !_dat_tzname
#			if _dat__tzname
#				undef	_dat_tzname
#				define _dat_tzname	1
#				define tzname		_tzname
#			endif
#		endif
#	endif
#	if _dat_tzname
		extern char*		tzname[];
#	endif
#endif

#define TM_type		(-1)

static const Namval_t		options[] =
{
	"adjust",	TM_ADJUST,
	"format",	TM_DEFAULT,
	"leap",		TM_LEAP,
	"type",		TM_type,
	"utc",		TM_UTC,
	0,		0
};

Tm_info_t	_tm_info_ = { 0 };

__EXTERN__(Tm_info_t, _tm_info_);

/*
 * return minutes west of GMT for local time clock
 *
 * isdst will point to non-zero if DST is in effect
 * this routine also kicks in the local initialization
 */

static int
tzwest(time_t* clock, int* isdst)
{
	register Tm_t*	tp;
	register int	n;
	register int	m;
	int		h;
	time_t		epoch;

	/*
	 * convert to GMT assuming local time
	 */

	if (!(tp = (Tm_t*)gmtime(clock)))
	{
		/*
		 * some systems return 0 for negative time_t
		 */

		epoch = 0;
		clock = &epoch;
		tp = (Tm_t*)gmtime(clock);
	}
	n = tp->tm_yday;
	h = tp->tm_hour;
	m = tp->tm_min;

	/*
	 * localtime() handles DST and GMT offset
	 */

	tp = (Tm_t*)localtime(clock);
	if (n = tp->tm_yday - n)
	{
		if (n > 1)
			n = -1;
		else if (n < -1)
			n = 1;
	}
	*isdst = tp->tm_isdst;
	return (h - tp->tm_hour - n * 24) * 60 + m - tp->tm_min;
}

/*
 * stropt() option handler
 */

static int
tmopt(void* a, const void* p, int n, const char* v)
{
	Tm_zone_t*	zp;

	NoP(a);
	if (p)
		switch (((Namval_t*)p)->value)
		{
		case TM_DEFAULT:
			tm_info.deformat = (n && (n = strlen(v)) > 0 && (n < 2 || v[n-2] != '%' || v[n-1] != '?')) ? strdup(v) : tm_info.format[TM_DEFAULT];
			break;
		case TM_type:
			tm_info.local->type = (n && *v) ? ((zp = tmtype(v, NiL)) ? zp->type : strdup(v)) : 0;
			break;
		default:
			if (n)
				tm_info.flags |= ((Namval_t*)p)->value;
			else
				tm_info.flags &= ~((Namval_t*)p)->value;
			break;
		}
	return 0;
}

/*
 * initialize the local timezone
 */

static void
tmlocal(void)
{
	register Tm_zone_t*	zp;
	register int		n;
	register char*		s;
	register char*		e;
	int			i;
	int			m;
	int			isdst;
	char*			t;
	Tm_t*			tp;
	time_t			now;
	char			buf[20];

	static Tm_zone_t	local;

#if _lib_tzset
	tzset();
#endif
#if _dat_tzname
	local.standard = tzname[0];
	local.daylight = tzname[1];
#endif
	tmlocale();

	/*
	 * tm_info.local
	 */

	tm_info.zone = tm_info.local = &local;
	time(&now);
	n = tzwest(&now, &isdst);

	/*
	 * compute local DST offset by roaming
	 * through the last 12 months until tzwest() changes
	 */

	for (i = 0; i < 12; i++)
	{
		now -= 31 * 24 * 60 * 60;
		if ((m = tzwest(&now, &isdst)) != n)
		{
			if (!isdst)
			{
				isdst = n;
				n = m;
				m = isdst;
			}
			m -= n;
			break;
		}
	}
	local.west = n;
	local.dst = m;

	/*
	 * now get the time zone names
	 */

#if _dat_tzname
	if (tzname[0])
	{
		/*
		 * POSIX
		 */

		if (!local.standard)
			local.standard = tzname[0];
		if (!local.daylight)
			local.daylight = tzname[1];
	}
	else
#endif
	if ((s = getenv("TZNAME")) && *s && (s = strdup(s)))
	{
		/*
		 * BSD
		 */

		local.standard = s;
		if (s = strchr(s, ','))
			*s++ = 0;
		else
			s = "";
		local.daylight = s;
	}
	else if ((s = getenv("TZ")) && *s && *s != ':' && (s = strdup(s)))
	{
		/*
		 * POSIX style but skipped by localtime()
		 */

		local.standard = s;
		if (*++s && *++s && *++s)
		{
			*s++ = 0;
			tmgoff(s, &t, 0);
			for (s = t; isalpha(*t); t++);
			*t = 0;
		}
		else
			s = "";
		local.daylight = s;
	}
	else
	{
		/*
		 * tm_data.zone table lookup
		 */

		t = 0;
		for (zp = tm_data.zone; zp->standard; zp++)
		{
			if (zp->type)
				t = zp->type;
			if (zp->west == n && zp->dst == m)
			{
				local.type = t;
				local.standard = zp->standard;
				if (!(s = zp->daylight))
				{
					e = (s = buf) + sizeof(buf);
					s = tmpoff(s, e - s, zp->standard, 0, 0);
					if (s < e - 1)
					{
						*s++ = ' ';
						tmpoff(s, e - s, tm_info.format[TM_DT], m, TM_DST);
					}
					s = strdup(buf);
				}
				local.daylight = s;
				break;
			}
		}
		if (!zp->standard)
		{
			/*
			 * not in the table
			 */

			e = (s = buf) + sizeof(buf);
			s = tmpoff(s, e - s, tm_info.format[TM_UT], n, 0);
			local.standard = strdup(buf);
			if (s < e - 1)
			{
				*s++ = ' ';
				tmpoff(s, e - s, tm_info.format[TM_UT], m, TM_DST);
				local.daylight = strdup(buf);
			}
		}
	}

	/*
	 * set the options
	 */

	stropt(getenv("TM_OPTIONS"), options, sizeof(*options), tmopt, NiL);

	/*
	 * the time zone type is probably related to the locale
	 */

	if (!local.type)
	{
		s = local.standard;
		t = 0;
		for (zp = tm_data.zone; zp->standard; zp++)
		{
			if (zp->type)
				t = zp->type;
			if (tmword(s, NiL, zp->standard, NiL, 0))
			{
				local.type = t;
				break;
			}
		}
	}

	/*
	 * tm_info.flags
	 */

	if (!(tm_info.flags & TM_ADJUST))
	{
		now = (time_t)78811200;		/* Jun 30 1972 23:59:60 */
		tp = (Tm_t*)localtime(&now);
		if (tp->tm_sec != 60)
			tm_info.flags |= TM_ADJUST;
	}
	if (!(tm_info.flags & TM_UTC))
	{
		s = local.standard;
		for (zp = tm_data.zone; !zp->type && zp->standard; zp++)
			if (tmword(s, NiL, zp->standard, NiL, 0))
			{
				tm_info.flags |= TM_UTC;
				break;
			}
	}
}

/*
 * initialize tm data
 */

void
tminit(register Tm_zone_t* zp)
{
	static unsigned _ast_int4_t	serial = ~(_ast_int4_t)0;

	if (serial != ast.env_serial)
	{
		serial = ast.env_serial;
		if (tm_info.local)
		{
			memset(tm_info.local, 0, sizeof(*tm_info.local));
			tm_info.local = 0;
		}
	}
	if (!tm_info.local)
		tmlocal();
	if (!zp)
		zp = tm_info.local;
	tm_info.zone = zp;
}
