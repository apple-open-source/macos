/* DateSize.c */

#include "Sys.h"

#include <ctype.h>

#include "Util.h"
#include "RCmd.h"
#include "Cmds.h"
#include "Xfer.h"
#include "List.h"
#include "DateSize.h"


/* Cheezy, but somewhat portable way to get GMT offset. */
#ifdef HAVE_MKTIME
static
time_t GetUTCOffset(int year, int mon, int mday)
{
	struct tm local_tm, utc_tm, *utc_tmptr;
	time_t local_t, utc_t, utcOffset;

	ZERO(local_tm);
	ZERO(utc_tm);
	utcOffset = 0;
	
	local_tm.tm_year = year;
	local_tm.tm_mon = mon;
	local_tm.tm_mday = mday;
	local_tm.tm_hour = 12;
	local_tm.tm_isdst = -1;
	local_t = mktime(&local_tm);
	
	if (local_t != (time_t) -1) {
		utc_tmptr = gmtime(&local_t);
		utc_tm.tm_year = utc_tmptr->tm_year;
		utc_tm.tm_mon = utc_tmptr->tm_mon;
		utc_tm.tm_mday = utc_tmptr->tm_mday;
		utc_tm.tm_hour = utc_tmptr->tm_hour;
		utc_tm.tm_isdst = -1;
		utc_t = mktime(&utc_tm);

		if (utc_t != (time_t) -1)
			utcOffset = (local_t - utc_t);
	}
	return (utcOffset);
}	/* GetUTCOffset */
#endif	/* HAVE_MKTIME */




/* Converts an "ls" date, in either the "Feb  4  1992" or "Jan 16 13:42"
 * format to a time_t.
 */
time_t UnLSDate(char *dstr)
{
#ifndef HAVE_MKTIME
	return (kModTimeUnknown);
#else
	char *cp = dstr;
	int mon, day, year, hr, min;
	time_t now, mt;
	time_t result = kModTimeUnknown;
	struct tm ut, *t;

	switch (*cp++) {
		case 'A':
			mon = (*cp == 'u') ? 7 : 3;
			break;
		case 'D':
			mon = 11;
			break;
		case 'F':
			mon = 1;
			break;
		default:					   /* shut up un-init warning */
		case 'J':
			if (*cp++ == 'u')
				mon = (*cp == 'l') ? 6 : 5;
			else
				mon = 0;
			break;
		case 'M':
			mon = (*++cp == 'r') ? 2 : 4;
			break;
		case 'N':
			mon = 10;
			break;
		case 'O':
			mon = 9;
			break;
		case 'S':
			mon = 8;
	}
	cp = dstr + 4;
	day = 0;
	if (*cp != ' ')
		day = 10 * (*cp - '0');
	cp++;
	day += *cp++ - '0';
	min = 0;
	
	(void) time(&now);
	t = localtime(&now);

	if (*++cp != ' ') {
		/* It's a time, XX:YY, not a year. */
		cp[2] = ' ';
		(void) sscanf(cp, "%d %d", &hr, &min);
		cp[2] = ':';
		year = t->tm_year;
		if (mon > t->tm_mon)
			--year;
	} else {
		hr = min = 0;
		(void) sscanf(cp, "%d", &year);
		year -= 1900;
	}
	/* Copy the whole structure of the 'tm' pointed to by t, so it will
	 * also set all fields we don't specify explicitly to be the same as
	 * they were in t.  That way we copy non-standard fields such as
	 * tm_gmtoff, if it exists or not.
	 */
	ut = *t;
	ut.tm_sec = 1;
	ut.tm_min = min;
	ut.tm_hour = hr;
	ut.tm_mday = day;
	ut.tm_mon = mon;
	ut.tm_year = year;
	ut.tm_wday = ut.tm_yday = 0;
	ut.tm_isdst = -1;	/* Let mktime figure this out for us. */
	mt = mktime(&ut);
	if (mt != (time_t) -1)
		result = (time_t) mt;
	return (result);
#endif	/* HAVE_MKTIME */
}	/* UnLSDate */



/* Converts a MDTM date, like "19930602204445"
 * format to a time_t.
 */
time_t UnMDTMDate(char *dstr)
{
#ifndef HAVE_MKTIME
	return (kModTimeUnknown);
#else
	struct tm ut, *t;
	time_t mt, now;
	time_t result = kModTimeUnknown;

	(void) time(&now);
	t = localtime(&now);

	/* Copy the whole structure of the 'tm' pointed to by t, so it will
	 * also set all fields we don't specify explicitly to be the same as
	 * they were in t.  That way we copy non-standard fields such as
	 * tm_gmtoff, if it exists or not.
	 */
	ut = *t;

	/* The time we get back from the server is (should be) in UTC. */
	if (sscanf(dstr, "%04d%02d%02d%02d%02d%02d",
		&ut.tm_year,
		&ut.tm_mon,
		&ut.tm_mday,
		&ut.tm_hour,
		&ut.tm_min,
		&ut.tm_sec) == 6)
	{	
		--ut.tm_mon;
		ut.tm_year -= 1900;
		ut.tm_isdst = -1;
		mt = mktime(&ut);
		if (mt != (time_t) -1) {
			mt += GetUTCOffset(ut.tm_year, ut.tm_mon, ut.tm_mday);
			result = (time_t) mt;
		}
	}
	return result;
#endif	/* HAVE_MKTIME */
}	/* UnMDTMDate */




/* Given a filename, do an "ls -ld" and from the output determine the
 * size and date of that file.  Since this is UNIX dependent, we
 * would rather use the SIZE and MDTM commands if we can.
 */
long GetDateSizeFromLSLine(char *fName, time_t *modifTime)
{
	char *cp, *np;
	string lsline;
	long size = kSizeUnknown;
	int n;
	static int depth = 0;
	LineList fileList;

	depth++;	/* Try to prevent infinite recursion. */
	*modifTime = kModTimeUnknown;
	InitLineList(&fileList);
	ListToMemory(&fileList, "LIST", kListDirNamesOnlyMode, fName);
	if (fileList.first == NULL)
		goto aa;

	(void) STRNCPY(lsline, fileList.first->line);
	DisposeLineListContents(&fileList);

	/* See if this line looks like a unix-style ls line. 
	 * If so, we can grab the date and size from it.
	 */	
	if (strpbrk(lsline, "-dlsbcp") == lsline) {
		/* See if it looks like a typical '-rwxrwxrwx' line. */
		cp = lsline + 1;
		if (*cp != 'r' && *cp != '-')
			goto aa;
		++cp;
		if (*cp != 'w' && *cp != '-')
			goto aa;
		cp += 2;
		if (*cp != 'r' && *cp != '-')
			goto aa;
 
 		/* skip mode, links, owner (and possibly group) */
 		for (n = 0; n < 4; n++) {
 			np = cp;
 			while (*cp != '\0' && !isspace(*cp))
 				cp++;
 			while (*cp != '\0' &&  isspace(*cp))
 				cp++;
 		}
 		if (!isdigit(*cp))
 			cp = np;	/* back up (no group) */
 		(void) sscanf(cp, "%ld%n", &size, &n);
 
 		*modifTime = UnLSDate(cp + n + 1);

		if (size <= 512L) {
			/* May be the size of a link to the file, instead of the file. */
			if ((cp = strstr(lsline, " -> ")) != NULL) {
				/* Yes, it was a link. */
				size = (depth>4) ? kSizeUnknown :
					GetDateAndSize(cp + 4, modifTime);
				/* Try the file. */
			}
		}
	}	
aa:
	--depth;
	return (size);
}	/* GetDateSizeFromLSLine */




/* The caller wanted to know the modification date and size of the remote
 * file given to us.  We try to get this information by using the SIZE
 * and MDTM ftp commands, and if that didn't work we try sending the site
 * a "ls -l <fName>" and try to get that information from the line it
 * sends us back.  It is possible that we won't be able to determine
 * either of these, though.
 */
long GetDateAndSize(char *fName, time_t *modifTime)
{
	time_t mdtm, ls_mdtm;
	long size, ls_size;
	int have_mdtm, have_size;

	size = ls_size = kSizeUnknown;
	mdtm = ls_mdtm = kModTimeUnknown;
	if (fName != NULL) {
		have_mdtm = have_size = 0;
		have_size = ((DoSize(fName, &size)) == 0);

#ifdef HAVE_MKTIME
		/* This would use mktime() to un-mangle the reply. */
		have_mdtm = ((DoMdtm(fName, (time_t *) &mdtm)) == 0);
#endif /* HAVE_MKTIME */

		if (!have_mdtm || !have_size)
			ls_size = GetDateSizeFromLSLine(fName, &ls_mdtm);

		/* Try to use the information from the real SIZE/MDTM commands if
		 * we could, since some maverick ftp server may be using a non-standard
		 * ls command, and we could parse it wrong.
		 */
		
		if (!have_mdtm)
			mdtm = ls_mdtm;
		if (!have_size)
			size = ls_size;

		DebugMsg("Used SIZE: %s.  Used MDTM: %s.\n",
			have_size ? "yes" : "no",
			have_mdtm ? "yes" : "no"
		);

		if (size != kSizeUnknown)
			DebugMsg("Size: %ld\n", size);
		else
			DebugMsg("Size: ??\n");
		if (mdtm != kModTimeUnknown)
			DebugMsg("Mdtm: %s", ctime(&mdtm));
		else
			DebugMsg("Mdtm: ??\n");
	}
	*modifTime = mdtm;
	return size;
}	/* GetDateAndSize */
