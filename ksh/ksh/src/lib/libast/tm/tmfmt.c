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

#define warped(t,n)	((t)<((n)-6L*30L*24L*60L*60L)||(t)>((n)+24L*60L*60L))

/*
 * format n with padding p into s
 * return end of s
 *
 * p:	<0	blank padding
 *	 0	no padding
 *	>0	0 padding
 */

static char*
number(register char* s, register char* e, register long n, register int p, int pad)
{
	switch (pad)
	{
	case '-':
		p = 0;
		break;
	case '_':
		if (p > 0)
			p = -p;
		break;
	case '0':
		if (p < 0)
			p = -p;
		break;
	}
	if (p > 0)
		s += sfsprintf(s, e - s, "%0*lu", p, n);
	else if (p < 0)
		s += sfsprintf(s, e - s, "%*lu", -p, n);
	else
		s += sfsprintf(s, e - s, "%lu", n);
	return s;
}

typedef struct Stack_s
{
	char*		format;
	int		delimiter;
} Stack_t;

/*
 * format date given clock into buf of length len
 * end of buf is returned
 */

char*
tmfmt(char* buf, size_t len, const char* format, time_t* clock)
{
	register char*	cp;
	register char*	ep;
	register char*	p;
	register int	n;
	int		c;
	int		i;
	int		flags;
	int		pad;
	int		delimiter;
	Tm_t*		tp;
	Tm_zone_t*	zp;
	time_t		now;
	Stack_t*	sp;
	Stack_t		stack[4];

	tmlocale();
	tp = tmmake(clock);
	if (!format || !*format)
		format = tm_info.deformat;
	flags = tm_info.flags;
	sp = &stack[0];
	cp = buf;
	ep = buf + len - 1;
	delimiter = 0;
	for (;;)
	{
		if ((c = *format++) == delimiter)
		{
			delimiter = 0;
			if (sp <= &stack[0])
				break;
			sp--;
			format = sp->format;
			delimiter = sp->delimiter;
			continue;
		}
		if (c != '%')
		{
			if (cp < ep)
				*cp++ = c;
			continue;
		}
		pad = 0;
		for (;;)
		{
			switch (*format)
			{
			case '_':
			case '-':
			case '0':
				pad = *format++;
				continue;
			case 'E':
			case 'O':
				if (!isalpha(*(format + 1)))
					break;
				format++;
				continue;
			default:
				break;
			}
			break;
		}
		switch (*format++)
		{
		case 0:
			format--;
			continue;
		case '%':
			if (cp < ep)
				*cp++ = '%';
			continue;
		case '?':
			if (tm_info.deformat != tm_info.format[TM_DEFAULT])
				format = tm_info.deformat;
			else if (!*format)
				format = tm_info.format[TM_DEFAULT];
			continue;
		case 'a':	/* abbreviated day of week name */
			n = TM_DAY_ABBREV + tp->tm_wday;
			goto index;
		case 'A':	/* day of week name */
			n = TM_DAY + tp->tm_wday;
			goto index;
		case 'b':	/* abbreviated month name */
		case 'h':
			n = TM_MONTH_ABBREV + tp->tm_mon;
			goto index;
		case 'B':	/* month name */
			n = TM_MONTH + tp->tm_mon;
			goto index;
		case 'c':	/* `ctime(3)' date sans newline */
			p = tm_info.format[TM_CTIME];
			goto push;
		case 'C':	/* 2 digit century */
			cp = number(cp, ep, (long)(1900 + tp->tm_year) / 100, 2, pad);
			continue;
		case 'd':	/* day of month */
			cp = number(cp, ep, (long)tp->tm_mday, 2, pad);
			continue;
		case 'D':	/* date */
			p = tm_info.format[TM_DATE];
			goto push;
		case 'E':       /* OBSOLETE no pad day of month */
			cp = number(cp, ep, (long)tp->tm_mday, 0, pad);
			continue;
		case 'e':       /* blank padded day of month */
			cp = number(cp, ep, (long)tp->tm_mday, -2, pad);
			continue;
		case 'f':	/* TM_DEFAULT override */
		case 'o':	/* OBSOLETE */
			p = tm_info.deformat;
			goto push;
		case 'F':	/* TM_DEFAULT */
		case 'O':	/* OBSOLETE */
			p = tm_info.format[TM_DEFAULT];
			goto push;
		case 'g':	/* `ls -l' recent date */
			p = tm_info.format[TM_RECENT];
			goto push;
		case 'G':	/* `ls -l' distant date */
			p = tm_info.format[TM_DISTANT];
			goto push;
		case 'H':	/* hour (0 - 23) */
			cp = number(cp, ep, (long)tp->tm_hour, 2, pad);
			continue;
		case 'i':	/* international `date(1)' date */
			p = tm_info.format[TM_INTERNATIONAL];
			goto push;
		case 'I':	/* hour (0 - 12) */
			if ((n = tp->tm_hour) > 12) n -= 12;
			else if (n == 0) n = 12;
			cp = number(cp, ep, (long)n, 2, pad);
			continue;
		case 'J':	/* Julian date (0 offset) */
			cp = number(cp, ep, (long)tp->tm_yday, 3, pad);
			continue;
		case 'j':	/* Julian date (1 offset) */
			cp = number(cp, ep, (long)(tp->tm_yday + 1), 3, pad);
			continue;
		case 'k':	/* `date(1)' date */
			p = tm_info.format[TM_DATE_1];
			goto push;
		case 'K':
			p = "%Y-%m-%d+%H:%M:%S";
			goto push;
		case 'l':	/* `ls -l' date */
			if (clock)
			{
				time(&now);
				if (warped(*clock, now))
				{
					p = tm_info.format[TM_DISTANT];
					goto push;
				}
			}
			p = tm_info.format[TM_RECENT];
			goto push;
		case 'm':	/* month number */
			cp = number(cp, ep, (long)(tp->tm_mon + 1), 2, pad);
			continue;
		case 'M':	/* minutes */
			cp = number(cp, ep, (long)tp->tm_min, 2, pad);
			continue;
		case 'n':
			if (cp < ep)
				*cp++ = '\n';
			continue;
		case 'N':	/* time zone type (nation code) */
			if (!(tm_info.flags & TM_UTC))
			{
				if ((zp = tm_info.zone) != tm_info.local)
					for (; zp >= tm_data.zone; zp--)
						if (p = zp->type)
							goto string;
				else if (p = zp->type)
					goto string;
			}
			continue;
		case 'p':	/* meridian */
			n = TM_MERIDIAN + (tp->tm_hour >= 12);
			goto index;
		case 'Q':	/* %Q<delim>recent<delim>distant<delim> */
			if (c = *format)
			{
				format++;
				if (clock)
				{
					time(&now);
					p = warped(*clock, now) ? (char*)0 : (char*)format;
				}
				else
					p = (char*)format;
				i = 0;
				while (n = *format)
				{
					format++;
					if (n == c)
					{
						if (!p)
							p = (char*)format;
						if (++i == 2)
							goto push_delimiter;
					}
				}
			}
			continue;
		case 'r':
			p = tm_info.format[TM_MERIDIAN_TIME];
			goto push;
		case 'R':
			p = "%H:%M";
			goto push;
		case 's':	/* seconds since the epoch */
		case '#':
			if (clock)
				now = *clock;
			else
				time(&now);
			cp = number(cp, ep, (long)now, 0, pad);
			continue;
		case 'S':	/* seconds */
			cp = number(cp, ep, (long)tp->tm_sec, 2, pad);
			continue;
		case 't':
			if (cp < ep)
				*cp++ = '\t';
			continue;
		case 'T':
			p = tm_info.format[TM_TIME];
			goto push;
		case 'u':	/* weekday number [1(Monday)-7] */
			if (!(i = tp->tm_wday))
				i = 7;
			cp = number(cp, ep, (long)i, 0, pad);
			continue;
		case 'U':	/* week number, Sunday as first day */
			i = tp->tm_yday - tp->tm_wday;
		week:
			n = (i >= 0) ? (i + 1) / 7 + 1 : 0;
			cp = number(cp, ep, (long)n, 2, pad);
			continue;
		case 'V':	/* ISO week number */
			if (tp->tm_wday == 0)
				i = tp->tm_yday - 6;
			else
				i = tp->tm_yday - tp->tm_wday + 1;
			n = (i >= -3) ? (i + 1) / 7 + 1 : 53;
			cp = number(cp, ep, (long)n, 2, pad);
			continue;
		case 'W':	/* week number, Monday as first day */
			if (tp->tm_wday == 0)
				i = tp->tm_yday - 6;
			else
				i = tp->tm_yday - tp->tm_wday + 1;
			goto week;
		case 'w':	/* weekday number [0(Sunday)-6] */
			cp = number(cp, ep, (long)tp->tm_wday, 0, pad);
			continue;
		case 'x':
			p = tm_info.format[TM_DATE];
			goto push;
		case 'X':
			p = tm_info.format[TM_TIME];
			goto push;
		case 'y':	/* year in the form yy */
			cp = number(cp, ep, (long)(tp->tm_year % 100), 2, pad);
			continue;
		case 'Y':	/* year in the form ccyy */
			cp = number(cp, ep, (long)(1900 + tp->tm_year), 4, pad);
			continue;
		case 'z':	/* time zone west offset */
			if ((ep - cp) >= 16)
				cp = tmpoff(cp, ep - cp, "", (tm_info.flags & TM_UTC) ? 0 : tm_info.zone->west, 24 * 60);
			continue;
		case 'Z':	/* time zone */
			p = (tm_info.flags & TM_UTC) ? tm_info.format[TM_UT] : tp->tm_isdst && tm_info.zone->daylight ? tm_info.zone->daylight : tm_info.zone->standard;
			goto string;
		case '!':
		case '|':
		case '+':
			switch (*format++)
			{
			case 0:
				format--;
				continue;
			case 'l':
				n = TM_LEAP;
				break;
			case 'u':
				n = TM_UTC;
				break;
			}
			if (*(format - 2) == '+')
			{
				if (!(tm_info.flags & n))
				{
					tm_info.flags |= n;
					tp = tmmake(clock);
				}
			}
			else if (tm_info.flags & n)
			{
				tm_info.flags &= ~n;
				tp = tmmake(clock);
			}
			continue;
		default:
			if (cp < ep)
				*cp++ = c;
			if (cp < ep)
				*cp++ = *(format - 1);
			continue;
		}
	index:
		p = tm_info.format[n];
	string:
		while (*cp = *p++)
			if (cp < ep)
				cp++;
		continue;
	push:
		c = 0;
	push_delimiter:
		if (sp < &stack[elementsof(stack)])
		{
			sp->format = (char*)format;
			format = p;
			sp->delimiter = delimiter;
			delimiter = c;
			sp++;
		}
		continue;
	}
	tm_info.flags = flags;
	*cp = 0;
	return cp;
}
