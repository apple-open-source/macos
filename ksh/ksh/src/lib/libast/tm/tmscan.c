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
 * scan date expression in s using format
 * if non-null, e points to the first invalid sequence in s
 * if non-null, f points to the first unused format char
 * clock provides default values
 */

#include <ast.h>
#include <tm.h>
#include <ctype.h>

typedef struct
{
	int		year;
	int		mon;
	int		week;
	int		yday;
	int		mday;
	int		wday;
	int		hour;
	int		min;
	int		sec;
	int		meridian;
	int		zone;
} Set_t;

#define CLEAR(s)	(s.year=s.mon=s.week=s.yday=s.mday=s.wday=s.hour=s.min=s.sec=s.meridian=(-1),s.zone=TM_LOCALZONE)

#define INDEX(m,x)	(((n)>=((x)-(m)))?((n)-=((x)-(m))):(n))

#define NUMBER(d,m,x)	do \
			{ \
				n = 0; \
				t = (char*)s; \
				while (s < (const char*)(t + d) && *s >= '0' && *s <= '9') \
					n = n * 10 + *s++ - '0'; \
				if (t == (char*)s || n < m || n > x) \
					goto done; \
			} while (0)

static time_t
scan(register const char* s, char** e, const char* format, char** f, time_t* clock, long flags)
{
	register int	d;
	register int	n;
	register char*	p;
	register Tm_t*	tm;
	char*		t;
	char*		stack[4];
	int		m;
	int		hi;
	int		lo;
	int		pedantic;
	time_t		tt;
	Set_t		set;
	Tm_zone_t*	zp;

	char**		sp = &stack[0];

	CLEAR(set);
	tm = tmmake(clock);
	tm_info.date = tm_info.zone;
	pedantic = (flags & TM_PEDANTIC) != 0;
	while (isspace(*s))
		s++;
	while (*s)
	{
		if (!(d = *format++))
		{
			if (sp <= &stack[0])
			{
				format--;
				break;
			}
			format = (const char*)*--sp;
		}
		else if (d == '%' && (d = *format) && format++ && d != '%')
		{
		more:
			switch (d)
			{
			case 'a':
				lo = TM_DAY_ABBREV;
				hi = pedantic ? TM_DAY : TM_TIME;
				goto get_wday;
			case 'A':
				lo = pedantic ? TM_DAY : TM_DAY_ABBREV;
				hi = TM_TIME;
			get_wday:
				if ((n = tmlex(s, &t, tm_info.format + lo, hi - lo, NiL, 0)) < 0)
					goto done;
				s = t;
				INDEX(TM_DAY_ABBREV, TM_DAY);
				set.wday = n;
				continue;
			case 'b':
			case 'h':
				lo = TM_MONTH_ABBREV;
				hi = pedantic ? TM_MONTH : TM_DAY_ABBREV;
				goto get_mon;
			case 'B':
				lo = pedantic ? TM_MONTH : TM_MONTH_ABBREV;
				hi = TM_DAY_ABBREV;
			get_mon:
				if ((n = tmlex(s, &t, tm_info.format + lo, hi - lo, NiL, 0)) < 0)
					goto done;
				s = t;
				INDEX(TM_MONTH_ABBREV, TM_MONTH);
				set.mon = n;
				continue;
			case 'c':
				p = "%a %b %e %T %Y";
				break;
			case 'C':
				NUMBER(2, 19, 99);
				set.year = (n - 19) * 100 + tm->tm_year % 100;
				continue;
			case 'd':
				if (pedantic && !isdigit(*s))
					goto done;
				/*FALLTHROUGH*/
			case 'e':
				NUMBER(2, 1, 31);
				set.mday = n;
				continue;
			case 'D':
				p = "%m/%d/%y";
				break;
			case 'E':
			case 'O':
				if (*format)
				{
					d = *format++;
					goto more;
				}
				continue;
			case 'H':
			case 'k':
				NUMBER(2, 0, 23);
				set.hour = n;
				continue;
			case 'I':
			case 'l':
				NUMBER(2, 1, 12);
				set.hour = n;
				continue;
			case 'j':
				NUMBER(3, 1, 366);
				set.yday = n - 1;
				continue;
			case 'm':
				NUMBER(2, 1, 12);
				set.mon = n - 1;
				continue;
			case 'M':
				NUMBER(2, 0, 59);
				set.min = n;
				continue;
			case 'n':
				if (pedantic)
					while (*s == '\n')
						s++;
				else
					while (isspace(*s))
						s++;
				continue;
			case 'p':
				if ((n = tmlex(s, &t, tm_info.format + TM_MERIDIAN, TM_UT - TM_MERIDIAN, NiL, 0)) < 0)
					goto done;
				set.meridian = n;
				s = t;
				continue;
			case 'r':
				p = "%I:%M:%S %p";
				break;
			case 'R':
				p = "%H:%M:%S";
				break;
			case 's':
				tt = strtoul(s, &t, 0);
				if (s == t)
					goto done;
				tm = tmmake(&tt);
				s = t;
				CLEAR(set);
				continue;
			case 'S':
				NUMBER(2, 0, 61);
				set.sec = n;
				continue;
			case 't':
				if (pedantic)
					while (*s == '\n')
						s++;
				else
					while (isspace(*s))
						s++;
				continue;
			case 'U':
				NUMBER(2, 0, 53);
				set.week = (n << 1);
				continue;
			case 'w':
				NUMBER(2, 0, 6);
				set.wday = n;
				continue;
			case 'W':
				NUMBER(2, 0, 53);
				set.week = (n << 1) | 1;
				continue;
			case 'x':
				p = tm_info.format[TM_DATE];
				break;
			case 'X':
				p = tm_info.format[TM_TIME];
				break;
			case 'y':
				NUMBER(2, 0, 99);
				if (n < TM_WINDOW)
					n += 100;
				set.year = n;
				continue;
			case 'Y':
				NUMBER(4, 1969, 2100);
				set.year = n - 1900;
				continue;
			case 'Z':
			case 'N':
				if (zp = tmtype(s, &t))
				{
					s = t;
					t = zp->type;
				}
				else
					t = 0;
				if (d == 'N')
					continue;
			case 'z':
				if ((zp = tmzone(s, &t, t, &m)))
				{
					s = t;
					set.zone = zp->west + m;
					tm_info.date = zp;
				}
				continue;
			default:
				goto done;
			}
			if (sp >= &stack[elementsof(stack)])
				goto done;
			*sp++ = (char*)format;
			format = (const char*)p;
		}
		else if (isspace(d))
			while (isspace(*s))
				s++;
		else if (*s != d)
			break;
		else s++;
	}
 done:
	if (e)
	{
		while (isspace(*s))
			s++;
		*e = (char*)s;
	}
	if (f)
	{
		while (isspace(*format))
			format++;
		*f = (char*)format;
	}
	if (set.year >= 0)
		tm->tm_year = set.year;
	if (set.mon >= 0)
	{
		if (set.year < 0 && set.mon < tm->tm_mon)
			tm->tm_year++;
		tm->tm_mon = set.mon;
		if (set.yday < 0 && set.mday < 0)
			tm->tm_mday = set.mday = 1;
	}
	if (set.week >= 0)
	{
		if (set.mon < 0)
		{
			tm->tm_mon = 0;
			tm->tm_mday = (set.week >> 1) * 7 + 1;
		}
	}
	else if (set.yday >= 0)
	{
		if (set.mon < 0)
			tm->tm_mday += set.yday - tm->tm_yday;
	}
	else if (set.mday >= 0)
		tm->tm_mday = set.mday;
	if (set.hour >= 0)
	{
		if (set.hour < tm->tm_hour && set.yday < 0 && set.mday < 0 && set.wday < 0)
			tm->tm_mday++;
		tm->tm_hour = set.hour;
		tm->tm_min = (set.min >= 0) ? set.min : 0;
		tm->tm_sec = (set.sec >= 0) ? set.sec : 0;
	}
	else if (set.min >= 0)
	{
		tm->tm_min = set.min;
		tm->tm_sec = (set.sec >= 0) ? set.sec : 0;
	}
	else if (set.sec >= 0)
		tm->tm_sec = set.sec;
	if (set.meridian > 0)
	{
		if (tm->tm_hour < 12)
			tm->tm_hour += 12;
	}
	else if (set.meridian == 0)
	{
		if (tm->tm_hour >= 12)
			tm->tm_hour -= 12;
	}
	tt = tmtime(tm, set.zone);
	tm = 0;
	if (set.yday >= 0)
	{
		tm = tmmake(&tt);
		tm->tm_mday += set.yday - tm->tm_yday;
	}
	else if (set.week >= 0)
	{
		/*HERE*/
	}
	else if (set.wday >= 0)
	{
		tm = tmmake(&tt);
		if ((n = set.wday - tm->tm_wday) < 0)
			n += 7;
		tm->tm_mday += n;
	}
	return tm ? tmtime(tm, set.zone) : tt;
}

/*
 *  format==0	DATEMSK
 * *format==0	DATEMSK and tmdate()
 * *format!=0	format
 */

time_t
tmscan(const char* s, char** e, const char* format, char** f, time_t* clock, long flags)
{
	register char*	v;
	register char**	p;
	char*		q;
	char*		r;
	time_t		t;

	static char**	datemask;

	tmlocale();
	if (!format || !*format)
	{
		if (!datemask)
		{
			register Sfio_t*	sp;
			register int		n;
			off_t			m;

			if ((v = getenv("DATEMSK")) && *v && (sp = sfopen(NiL, v, "r")))
			{
				for (n = 1; sfgetr(sp, '\n', 0); n++);
				m = sfseek(sp, 0L, SEEK_CUR);
				if (p = newof(0, char*, n, m))
				{
					sfseek(sp, 0L, SEEK_SET);
					v = (char*)(p + n);
					if (sfread(sp, v, m) != m)
					{
						free(p);
						p = 0;
					}
					else
					{
						datemask = p;
						v[m] = 0;
						while (*v)
						{
							*p++ = v;
							if (!(v = strchr(v, '\n')))
								break;
							*v++ = 0;
						}
						*p = 0;
					}
				}
			}
			if (!datemask)
				datemask = (char**)&datemask;
		}
		if ((p = datemask) != (char**)&datemask)
			while (v = *p++)
			{
				t = scan(s, &q, v, &r, clock, flags);
				if (!*q && !*r)
				{
					if (e)
						*e = q;
					if (f)
						*f = r;
					return t;
				}
			}
		if (f)
			*f = (char*)format;
		if (format)
			return tmdate(s, e, clock);
		if (e)
			*e = (char*)s;
		return 0;
	}
	return scan(s, e, format, f, clock, flags);
}
