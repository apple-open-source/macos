/*
** This file is in the public domain, so clarified as of
** 1996-06-05 by Arthur David Olson (arthur_david_olson@nih.gov).
*/

#include <sys/cdefs.h>
#ifndef lint
#ifndef NOID
static char	elsieid[] __unused = "@(#)asctime.c	7.9";
#endif /* !defined NOID */
#endif /* !defined lint */
__FBSDID("$FreeBSD: src/lib/libc/stdtime/asctime.c,v 1.12 2004/06/14 10:31:52 stefanf Exp $");

/*LINTLIBRARY*/

#include "namespace.h"
#include "private.h"
#include "un-namespace.h"
#include "tzfile.h"

/*
** A la ISO/IEC 9945-1, ANSI/IEEE Std 1003.1, Second Edition, 1996-07-12.
*/

#define	EXPECTEDLEN	26

char *
asctime_r(const struct tm * __restrict timeptr, char * __restrict buf)
{
	static const char	wday_name[][3] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};
	static const char	mon_name[][3] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	const char *	wn;
	const char *	mn;
	int		len;
	char		tmp[EXPECTEDLEN];

	if (timeptr->tm_wday < 0 || timeptr->tm_wday >= DAYSPERWEEK)
		wn = "???";
	else	wn = wday_name[timeptr->tm_wday];
	if (timeptr->tm_mon < 0 || timeptr->tm_mon >= MONSPERYEAR)
		mn = "???";
	else	mn = mon_name[timeptr->tm_mon];
	/*
	** The X3J11-suggested format is
	**	"%.3s %.3s%3d %02.2d:%02.2d:%02.2d %d\n"
	** Since the .2 in 02.2d is ignored, we drop it.
	*/
	/*
	** Because various values in the tm structure may cause the
	** resulting string to be longer than the 26-bytes that is
	** specified in the spec, we should return NULL rather than
	** possibly overwrite beyond the string.
	*/
	len = snprintf(tmp, EXPECTEDLEN, "%.3s %.3s%3d %02d:%02d:%02d %d\n",
		wn, mn,
		timeptr->tm_mday, timeptr->tm_hour,
		timeptr->tm_min, timeptr->tm_sec,
		TM_YEAR_BASE + timeptr->tm_year);
	if (len >= EXPECTEDLEN)
		return NULL;
	strcpy(buf, tmp);
	return buf;
}

char *
asctime(timeptr)
const struct tm *	timeptr;
{
	static char		result[EXPECTEDLEN];

	return asctime_r(timeptr, result);
}
