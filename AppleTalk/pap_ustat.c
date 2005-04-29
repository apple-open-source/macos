/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *	pap_status_update()
 *
 *	This routine updates the global array pap_status (defined here).
 *	It converts data returned by the ImageWriter and ImageWriter LQ,
 *	but passes untouched data from other types.
 *
 *	Revision History:
 *		11/03/88	Created.  JMS
 */

#include <netat/appletalk.h>

#define	TYPE_IW2	"ImageWriter"
#define	TYPE_IW2_LEN	11
#define	TYPE_LQ		"LQ"
#define	TYPE_LQ_LEN	2

#define	IW2_PRINTERBUSY	0x0080
#define	IW2_HEADMOVING	0x0100
#define	IW2_PRFAULTFLG	0x0200
#define	IW2_PPRJAMFLG	0x0400
#define	IW2_OFFLNFLG	0x0800
#define	IW2_CVROPNFLG	0x1000
#define	IW2_PPROUTFLG	0x2000
#define	IW2_SHFDRFLG	0x4000
#define	IW2_COLORFLG	0x8000

static const struct stat_msg {
	unsigned short flag;
	char *string;
} imagewriter_status[] = {
/*	busy flag appears to be always set on the ImageWriter!	*/
/*	IW2_PRINTERBUSY,	"printer busy",			*/
	IW2_HEADMOVING,		"printer busy",
/*	fault flag is always set with other fault conditions	*/
/*	IW2_PRFAULTFLG,		"printer fault",		*/
	IW2_PPRJAMFLG,		"paper jam",
	IW2_CVROPNFLG,		"cover open",
	IW2_PPROUTFLG,		"out of paper",
/*	off-line flag is listed last; don't mask other faults	*/
	IW2_OFFLNFLG,		"off-line",
/*	by default, the printer is idle....			*/
	0,			"idle",
};

char pap_status_str[512];

pap_status_update(type, status, status_len)
char *type;
unsigned char *status;
u_char status_len;
{
	/* ImageWriter and LQ status codes are the same! */
	if (nocase_strncmp(TYPE_IW2, type, TYPE_IW2_LEN) == 0 ||
		nocase_strncmp(TYPE_LQ, type, TYPE_LQ_LEN) == 0) {
		register unsigned short *sflag = (unsigned short *) status;
		register struct stat_msg *sptr;

		for (sptr = imagewriter_status; sptr->flag; sptr++)
			if ((*sflag & sptr->flag) == sptr->flag) {
				break;
			}
		(void) sprintf(pap_status_str, "status: %s", sptr->string);
	}
	else {
		/* the LaserWriter actually provides a string! Yea! */
		strncpy(pap_status_str, status, status_len);
		pap_status_str[status_len] = '\0';
	}
}

static	nocase_strncmp(str1, str2, count)
char	*str1, *str2;
int	count;
{
	int	i;
	char	ch1,ch2;

	/* case insensitive strncmp */
	for (i=0; i<count; i++) {
		ch1 = (str1[i] >= 'a' && str1[i] <= 'z') ?
			(str1[i] + 'A' - 'a') : str1[i];
		ch2 = (str2[i] >= 'a' && str2[i] <= 'z') ?
			(str2[i] + 'A' - 'a') : str2[i];
		if (ch1 != ch2)
			return(-1);
		/* if both the strings are of same length, shorter than 'count',
		 * then they're same.
		 */
		if (ch1 == '\0' && ch2 == '\0')
			return(0);
	}

	return(0);
}

char	*pap_status_get()
{
	return (pap_status_str);
}
