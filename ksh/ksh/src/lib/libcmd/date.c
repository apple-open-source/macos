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
 * AT&T Research
 *
 * date -- set/display date
 */

static const char usage[] =
"[-?\n@(#)$Id: date (AT&T Labs Research) 2004-02-29 $\n]"
USAGE_LICENSE
"[+NAME?date - set/list/convert dates]"
"[+DESCRIPTION?\bdate\b sets the current date and time (with appropriate"
"	privilege), lists the current date or file dates, or converts"
"	dates.]"
"[+?Most common \adate\a forms are recognized, including those for"
"	\bcrontab\b(1), \bls\b(1), \btouch\b(1), and the default"
"	output from \bdate\b itself.]"
"[+?If the \adate\a operand consists of 4, 6, 8, 10 or 12 digits followed"
"	by an optional \b.\b and two digits then it is interpreted as:"
"	\aHHMM.SS\a, \addHHMM.SS\a, \ammddHHMM.SS\a, \ammddHHMMyy.SS\a or"
"	\ayymmddHHMM.SS\a, or \ammddHHMMccyy.SS\a or \accyymmddHHMM.SS\a."
"	Conflicting standards and practice allow a leading or trailing"
"	2 or 4 digit year for the 10 and 12 digit forms; the X/Open trailing"
"	form is used to disambiguate (\btouch\b(1) uses the leading form.)"
"	Avoid the 10 digit form to avoid confusion. The digit fields are:]{"
"		[+cc?Century - 1, 19-20.]"
"		[+yy?Year in century, 00-99.]"
"		[+mm?Month, 01-12.]"
"		[+dd?Day of month, 01-31.]"
"		[+HH?Hour, 00-23.]"
"		[+MM?Minute, 00-59.]"
"		[+SS?Seconds, 00-60.]"
"}"
"[+?If more than one \adate\a operand is specified then:]{"
"		[+1.?Each operand sets the reference date for the next"
"			operand.]"
"		[+2.?The date is listed for each operand.]"
"		[+3.?The system date is not set.]"
"}"

"[a:access-time|atime?List file argument access times.]"
"[c:change-time|ctime?List file argument change times.]"
"[d:date?Use \adate\a as the current date and do not set the system"
"	clock.]:[date]"
"[e:epoch?Output the date in seconds since the epoch."
"	Equivalent to \b--format=%#\b.]"
"[E:elapsed?Interpret pairs of arguments as start and stop dates, sum the"
"	differences between all pairs, and list the result as a"
"	\bfmtelapsed\b(3) elapsed time on the standard output. If there are"
"	an odd number of arguments then the last time argument is differenced"
"	with the current time.]"
"[f:format?Output the date according to the \bstrftime\b(3) \aformat\a."
"	For backwards compatibility, a first argument of the form"
"	\b+\b\aformat\a is equivalent to \b-f\b format."
"	\aformat\a is in \bprintf\b(3) style, where %\afield\a names"
"	fixed size field, zero padded if necessary,"
"	and \\\ac\a and \\\annn\a sequences are as in C. Invalid"
"	%\afield\a specifications and all other characters are copied"
"	without change. \afield\a may be preceded by \b%-\b to turn off"
"	padding or \b%_\b to pad with space, otherwise numeric fields"
"	are padded with \b0\b and string fields are padded with space."
"	\afield\a may also be preceded by \bE\b for alternate era"
"	representation or \bO\b for alternate digit representation (if"
"	supported by the current locale.) The fields are:]:[format]{"
"		[+%?% character]"
"		[+a?abbreviated weekday name]"
"		[+A?full weekday name]"
"		[+b?abbreviated month name]"
"		[+c?\bctime\b(3) style date without the trailing newline]"
"		[+C?2-digit century]"
"		[+d?day of month number]"
"		[+D?date as \amm/dd/yy\a]"
"		[+e?blank padded day of month number]"
"		[+E?unpadded day of month number]"
"		[+f?locale default override date format]"
"		[+F?locale default date format]"
"		[+g?\bls\b(1) \b-l\b recent date with \ahh:mm\a]"
"		[+G?\bls\b(1) \b-l\b distant date with \ayyyy\a]"
"		[+h?abbreviated month name]"
"		[+H?24-hour clock hour]"
"		[+i?international \bdate\b(1) date with time zone type name]"
"		[+I?12-hour clock hour]"
"		[+j?1-offset Julian date]"
"		[+J?0-offset Julian date]"
"		[+k?\bdate\b(1) style date]"
"		[+K?all numeric date; equivalent to \b%Y-%m-%d+%H:%M:%S\b]"
"		[+l?\bls\b(1) \b-l\b date; equivalent to \b%Q/%g/%G/\b]"
"		[+m?month number]"
"		[+M?minutes]"
"		[+n?newline character]"
"		[+N?time zone type name]"
"		[+p?meridian (e.g., \bAM\b or \bPM\b)]"
"		[+Q?\a<del>recent<del>distant<del>\a: \a<del>\a is a unique"
"			delimter character; \arecent\a format for recent"
"			dates, \adistant\a format otherwise]"
"		[+r?12-hour time as \ahh:mm:ss meridian\a]"
"		[+R?24-hour time as \ahh:mm\a]"
"		[+s?number of seconds since the epoch]"
"		[+S?seconds]"
"		[+t?tab character]"
"		[+T?24-hour time as \ahh:mm:ss\a]"
"		[+u?weekday number 1(Monday)-7]"
"		[+U?week number with Sunday as the first day]"
"		[+V?ISO week number (i18n is \afun\a)]"
"		[+w?weekday number 0(Sunday)-6]"
"		[+W?week number with Monday as the first day]"
"		[+x?locale date style that includes month, day and year]"
"		[+X?locale time style that includes hours and minutes]"
"		[+y?2-digit year (you'll be sorry)]"
"		[+Y?4-digit year]"
"		[+z?time zone \aSHHMM\a west of GMT offset where S is"
"			\b+\b or \b-\b]"
"		[+Z?time zone name]"
"		[++|!flag?set (+) or clear (!) \aflag\a for the remainder"
"			of \aformat\a. \aflag\a may be:]{"
"			[+l?enable leap second adjustments]"
"			[+u?UTC time zone]"
"		}"
"		[+#?number of seconds since the epoch]"
"		[+??alternate?use \aalternate\a format if a default format"
"			override has not been specified, e.g., \bls\b(1) uses"
"			\"%?%l\"; export TM_OPTIONS=\"format='\aoverride\a'\""
"			to override the default]"
"}"
"[i:incremental|adjust?Set the system time in incrementatl adjustments to"
"	avoid complete time shift shock. Negative adjustments still maintain"
"	monotonic increasing time. Not available on all systems.]"
"[l:leap-seconds?Include leap seconds in time calculations. Leap seconds"
"	after the ast library release date are not accounted for.]"
"[m:modify-time|mtime?List file argument modify times.]"
"[n!:network?Set network time.]"
"[p:parse?Add \aformat\a to the list of \bstrptime\b(3) parse conversion"
"	formats. \aformat\a follows the same conventions as the"
"	\b--format\b option.]:[format]"
"[s:show?Show the date without setting the system time.]"
"[u:utc|gmt|zulu?Output dates in \acoordinated universal time\a (UTC).]"

"\n"
"\n[ +format | date ... | file ... ]\n"
"\n"

"[+SEE ALSO?\bcrontab\b(1), \bls\b(1), \btouch\b(1), \bfmtelapsed\b(3),"
"	\bstrftime\b(3), \bstrptime\b(3)]"
;

#include <cmdlib.h>
#include <ls.h>
#include <proc.h>
#include <tm.h>
#include <times.h>

#include "FEATURE/time"

typedef struct Fmt
{
	struct Fmt*	next;
	char*		format;
} Fmt_t;

#ifndef ENOSYS
#define ENOSYS		EINVAL
#endif

/*
 * set the system clock
 * the standards wimped out here
 */

static int
settime(const char* cmd, time_t clock, int adjust, int network)
{
	char*		s;
	char**		argv;
	char*		args[5];
	char		buf[128];

#if _lib_stime || _lib_settimeofday
	if (!adjust && !network)
	{
#if _lib_stime
		return stime(&clock);
#else
#if _lib_settimeofday
		struct timeval	tv;
		memset(&tv, 0, sizeof(tv));
		tv.tv_sec = clock;
#if _lib_2_timeofday
		return settimeofday(&tv, NiL);
#else
		return settimeofday(&tv);
#endif
#endif
#endif
	}
#endif
	argv = args;
	s = "/usr/bin/date";
	if (!streq(cmd, s) && (!access(s, X_OK) || !access(s+=4, X_OK)))
	{
		*argv++ = s;
		if (streq(astconf("UNIVERSE", NiL, NiL), "att"))
		{
			tmfmt(buf, sizeof(buf), "%m%d%H%M%Y.%S", &clock);
			if (adjust)
				*argv++ = "-a";
		}
		else
		{
			tmfmt(buf, sizeof(buf), "%Y%m%d%H%M.%S", &clock);
			if (network)
				*argv++ = "-n";
			if (tm_info.flags & TM_UTC)
				*argv++ = "-u";
		}
		*argv++ = buf;
		*argv = 0;
		if (!procrun(s, args))
			return 0;
	}
	return -1;
}

/*
 * convert s to time_t with error checking
 */

static time_t
convert(register Fmt_t* f, char* s, time_t now)
{
	char*	t;
	char*	u;

	do
	{
		now = tmscan(s, &t, f->format, &u, now ? &now : (time_t*)0, 0L);
		if (!*t && (!f->format || !*u))
			break;
	} while (f = f->next);
	if (!f || *t)
		error(3, "%s: invalid date specification", f ? t : s);
	return now;
}

int
b_date(int argc, register char** argv, void* context)
{
	register int	n;
	register char*	s;
	register Fmt_t*	f;
	char*		t;
	time_t		now;
	unsigned long	ts;
	unsigned long	te;
	unsigned long	e;
	char		buf[128];
	Fmt_t*		fmts;
	Fmt_t		fmt;
	struct stat	st;

	time_t*		clock = 0;	/* use this time		*/
	time_t*		filetime = 0;	/* use this st_ time field	*/
	char*		cmd = argv[0];	/* original command path	*/
	char*		format = 0;	/* tmfmt() format		*/
	char*		string = 0;	/* date string			*/
	int		elapsed = 0;	/* args are start/stop pairs	*/
	int		increment = 0;	/* incrementally adjust time	*/
	int		network = 0;	/* don't set network time	*/
	int		show = 0;	/* show date and don't set	*/

	NoP(argc);
	cmdinit(argv, context, ERROR_CATALOG, 0);
	setlocale(LC_ALL, "");
	tm_info.flags |= TM_DATESTYLE;
	fmts = &fmt;
	fmt.format = "";
	fmt.next = 0;
	for (;;)
	{
		switch (optget(argv, usage))
		{
		case 'a':
			filetime = &st.st_atime;
			continue;
		case 'c':
			filetime = &st.st_ctime;
			continue;
		case 'd':
			string = opt_info.arg;
			show = 1;
			continue;
		case 'e':
			format = "%#";
			continue;
		case 'E':
			elapsed = 1;
			continue;
		case 'f':
			format = opt_info.arg;
			continue;
		case 'i':
			increment = 1;
			continue;
		case 'l':
			tm_info.flags |= TM_LEAP;
			continue;
		case 'm':
			filetime = &st.st_mtime;
			continue;
		case 'n':
			network = 1;
			continue;
		case 'p':
			if (!(f = newof(0, Fmt_t, 1, 0)))
				error(ERROR_SYSTEM|3, "out of space [format]");
			f->next = fmts;
			f->format = opt_info.arg;
			fmts = f;
			continue;
		case 's':
			show = 1;
			continue;
		case 'u':
			tm_info.flags |= TM_UTC;
			continue;
		case '?':
			error(ERROR_USAGE|4, "%s", opt_info.arg);
			continue;
		case ':':
			error(2, "%s", opt_info.arg);
			continue;
		}
		break;
	}
	argv += opt_info.index;
	if (error_info.errors)
		error(ERROR_USAGE|4, "%s", optusage(NiL));
	if (elapsed)
	{
		e = 0;
		while (s = *argv++)
		{
			if (!(t = *argv++))
			{
				argv--;
				t = "now";
			}
			ts = convert(fmts, s, 0);
			te = convert(fmts, t, 0);
			if (te > ts)
				e += te - ts;
			else
				e += ts - te;
		}
		sfputr(sfstdout, fmtelapsed(e, 1), '\n');
	}
	else if (filetime)
	{
		if (!*argv)
			error(ERROR_USAGE|4, "%s", optusage(NiL));
		n = argv[1] != 0;
		while (s = *argv++)
		{
			if (stat(s, &st))
				error(2, "%s: not found", s);
			else
			{
				tmfmt(buf, sizeof(buf), format, filetime);
				if (n)
					sfprintf(sfstdout, "%s: %s\n", s, buf);
				else
					sfprintf(sfstdout, "%s\n", buf);
			}
		}
	}
	else
	{
		if ((s = *argv) && !format && *s == '+')
		{
			format = s + 1;
			argv++;
			s = *argv;
		}
		if (s || (s = string))
		{
			if (*argv && string)
				error(ERROR_USAGE|4, "%s", optusage(NiL));
			now = convert(fmts, s, 0);
			clock = &now;
			if (*argv && (s = *++argv))
			{
				show = 1;
				do
				{
					tmfmt(buf, sizeof(buf), format, clock);
					sfprintf(sfstdout, "%s\n", buf);
					now = convert(fmts, s, now);
				} while (s = *++argv);
			}
		}
		else
			show = 1;
		if (format || show)
		{
			tmfmt(buf, sizeof(buf), format, clock);
			sfprintf(sfstdout, "%s\n", buf);
		}
		else if (settime(cmd, *clock, increment, network))
			error(ERROR_SYSTEM|3, "cannot set system time");
	}
	while (fmts != &fmt)
	{
		f = fmts;
		fmts = fmts->next;
		free(f);
	}
	return error_info.errors != 0;
}
