/*
 * Copyright 1995, 1999, 2007 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 */

#include "heim.h"
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

mit_krb5_error_code KRB5_CALLCONV
krb5_string_to_timestamp(char *string, mit_krb5_timestamp *timestampp)
{
    int i;
    struct tm timebuf, timebuf2;
    time_t now, ret_time;
    char *s;
    static const char * const atime_format_table[] = {
	"%Y%m%d%H%M%S",		/* yyyymmddhhmmss		*/
	"%Y.%m.%d.%H.%M.%S",	/* yyyy.mm.dd.hh.mm.ss		*/
	"%y%m%d%H%M%S",		/* yymmddhhmmss			*/
	"%y.%m.%d.%H.%M.%S",	/* yy.mm.dd.hh.mm.ss		*/
	"%y%m%d%H%M",		/* yymmddhhmm			*/
	"%H%M%S",		/* hhmmss			*/
	"%H%M",			/* hhmm				*/
	"%T",			/* hh:mm:ss			*/
	"%R",			/* hh:mm			*/
	/* The following not really supported unless native strptime present */
	"%x:%X",		/* locale-dependent short format */
	"%d-%b-%Y:%T",		/* dd-month-yyyy:hh:mm:ss	*/
	"%d-%b-%Y:%R"		/* dd-month-yyyy:hh:mm		*/
    };
    static const int atime_format_table_nents =
	sizeof(atime_format_table)/sizeof(atime_format_table[0]);


    now = time((time_t *) NULL);
    if (localtime_r(&now, &timebuf2) == NULL)
	return EINVAL;
    for (i=0; i<atime_format_table_nents; i++) {
        /* We reset every time throughout the loop as the manual page
	 * indicated that no guarantees are made as to preserving timebuf
	 * when parsing fails
	 */
	timebuf = timebuf2;
	if ((s = strptime(string, atime_format_table[i], &timebuf))
	    && (s != string)) {
 	    /* See if at end of buffer - otherwise partial processing */
	    while(*s != 0 && isspace((unsigned char) *s)) s++;
	    if (*s != 0)
	        continue;
	    if (timebuf.tm_year <= 0)
		continue;	/* clearly confused */
	    ret_time = mktime(&timebuf);
	    if (ret_time == (time_t) -1)
		continue;	/* clearly confused */
	    *timestampp = (krb5_timestamp) ret_time;
	    return 0;
	}
    }
    return(EINVAL);
}
