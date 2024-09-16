/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1992 Keith Muller.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)gen_subs.c	8.1 (Berkeley) 5/31/93";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <langinfo.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef __APPLE__
#include <unistd.h>
#include <stdlib.h>
#include <vis.h>
#endif /* __APPLE__ */
#include "pax.h"
#include "extern.h"

/*
 * a collection of general purpose subroutines used by pax
 */

/*
 * constants used by ls_list() when printing out archive members
 */
#define MODELEN 20
#define DATELEN 64
#define SIXMONTHS	 ((365 / 2) * 86400)
#define CURFRMTM	"%b %e %H:%M"
#define OLDFRMTM	"%b %e  %Y"
#define CURFRMTD	"%e %b %H:%M"
#define OLDFRMTD	"%e %b  %Y"
#ifdef __APPLE__
#define NAME_WIDTH	8
#endif /* __APPLE__ */

static int d_first = -1;

/*
 * ls_list()
 *	list the members of an archive in ls format
 */

void
ls_list(ARCHD *arcn, time_t now, FILE *fp)
{
	struct stat *sbp;
	char f_mode[MODELEN];
	char f_date[DATELEN];
	const char *timefrmt;
#ifdef __APPLE__
	int term;

	term = zeroflag ? '\0' : '\n';	/* path termination character */
#endif /* __APPLE__ */

	/*
	 * if not verbose, just print the file name
	 */
	if (!vflag) {
#ifdef __APPLE__
		if (zeroflag)
			(void)fputs(arcn->name, fp);
		else
			safe_print(arcn->name, fp);
		(void)putc(term, fp);
#else
		(void)fprintf(fp, "%s\n", arcn->name);
#endif /* __APPLE__*/
		(void)fflush(fp);
		return;
	}

#ifdef __APPLE__
	if (pax_list_opt_format) {
		pax_format_list_output(arcn, now, fp, term);
		return;
	}
#endif /* __APPLE__ */

	if (d_first < 0)
		d_first = (*nl_langinfo(D_MD_ORDER) == 'd');
	/*
	 * user wants long mode
	 */
	sbp = &(arcn->sb);
	strmode(sbp->st_mode, f_mode);

	/*
	 * time format based on age compared to the time pax was started.
	 */
#ifdef __APPLE__
	if ((sbp->st_mtime + SIXMONTHS) <= now ||
		sbp->st_mtime > now)
#else
	if ((sbp->st_mtime + SIXMONTHS) <= now)
#endif /* __APPLE__ */
		timefrmt = d_first ? OLDFRMTD : OLDFRMTM;
	else
		timefrmt = d_first ? CURFRMTD : CURFRMTM;

	/*
	 * print file mode, link count, uid, gid and time
	 */
	if (strftime(f_date,DATELEN,timefrmt,localtime(&(sbp->st_mtime))) == 0)
		f_date[0] = '\0';
#ifdef __APPLE__
#define UT_NAMESIZE 8
	(void)fprintf(fp, "%s%2u %-*.*s %-*.*s ", f_mode, sbp->st_nlink,
		NAME_WIDTH, UT_NAMESIZE, name_uid(sbp->st_uid, 1),
		NAME_WIDTH, UT_NAMESIZE, name_gid(sbp->st_gid, 1));
#else
	(void)fprintf(fp, "%s%2ju %-12s %-12s ", f_mode,
		(uintmax_t)sbp->st_nlink,
		name_uid(sbp->st_uid, 1), name_gid(sbp->st_gid, 1));
#endif /* __APPLE__ */

	/*
	 * print device id's for devices, or sizes for other nodes
	 */
	if ((arcn->type == PAX_CHR) || (arcn->type == PAX_BLK))
		(void)fprintf(fp, "%4lu,%4lu ", (unsigned long)MAJOR(sbp->st_rdev),
		    (unsigned long)MINOR(sbp->st_rdev));
	else {
#ifdef __APPLE__
		/*
		 * UNIX compliance fix: printing filename length for soft links
		 * from arcn->ln_nlen instead of sbp->st_size, which is 0.
		 */
		off_t nlen;
		if (arcn->type == PAX_SLK) {
			nlen = arcn->ln_nlen;
		} else {
			nlen = sbp->st_size;
		}
		(void)fprintf(fp, "%9ju ", (uintmax_t)nlen);
#else
		(void)fprintf(fp, "%9ju ", (uintmax_t)sbp->st_size);
#endif /* __APPLE__ */
	}

	/*
	 * print name and link info for hard and soft links
	 */
#ifdef __APPLE__
	(void)fputs(f_date, fp);
	(void)putc(' ', fp);
	safe_print(arcn->name, fp);
	if ((arcn->type == PAX_HLK) || (arcn->type == PAX_HRG)) {
		fputs(" == ", fp);
		safe_print(arcn->ln_name, fp);
	} else if (arcn->type == PAX_SLK) {
		fputs(" -> ", fp);
		safe_print(arcn->ln_name, fp);
	}
	(void)putc(term, fp);
#else
	(void)fprintf(fp, "%s %s", f_date, arcn->name);
	if ((arcn->type == PAX_HLK) || (arcn->type == PAX_HRG))
		(void)fprintf(fp, " == %s\n", arcn->ln_name);
	else if (arcn->type == PAX_SLK)
		(void)fprintf(fp, " => %s\n", arcn->ln_name);
	else
		(void)putc('\n', fp);
#endif /* __APPLE__ */
	(void)fflush(fp);
	return;
}

/*
 * tty_ls()
 * 	print a short summary of file to tty.
 */

void
ls_tty(ARCHD *arcn)
{
	char f_date[DATELEN];
	char f_mode[MODELEN];
	const char *timefrmt;

	if (d_first < 0)
		d_first = (*nl_langinfo(D_MD_ORDER) == 'd');

	if ((arcn->sb.st_mtime + SIXMONTHS) <= time(NULL))
		timefrmt = d_first ? OLDFRMTD : OLDFRMTM;
	else
		timefrmt = d_first ? CURFRMTD : CURFRMTM;

	/*
	 * convert time to string, and print
	 */
	if (strftime(f_date, DATELEN, timefrmt,
	    localtime(&(arcn->sb.st_mtime))) == 0)
		f_date[0] = '\0';
	strmode(arcn->sb.st_mode, f_mode);
	tty_prnt("%s%s %s\n", f_mode, f_date, arcn->name);
	return;
}

/*
 * l_strncpy()
 *	copy src to dest up to len chars (stopping at first '\0').
 *	when src is shorter than len, pads to len with '\0'. 
 * Return:
 *	number of chars copied. (Note this is a real performance win over
 *	doing a strncpy(), a strlen(), and then a possible memset())
 */

int
l_strncpy(char *dest, const char *src, int len)
{
	char *stop;
	char *start;

	stop = dest + len;
	start = dest;
	while ((dest < stop) && (*src != '\0'))
		*dest++ = *src++;
	len = dest - start;
	while (dest < stop)
		*dest++ = '\0';
	return(len);
}

#ifdef __APPLE__
void
safe_print(const char *str, FILE *fp)
{
	/*
	 * if printing to a tty, use strvis(3) to print special characters.
	 */
	if (isatty(fileno(fp))) {
		/*
		 * The size of visbuf must be four times the number
		 * of bytes encoded from str (plus one for the NUL).
		 */
#ifdef __APPLE__
		size_t visbufsz = 4 * strlen(str) + 1;
		char *visbuf = malloc(visbufsz);
#else
		char *visbuf = malloc(4 * strlen(str) + 1);
#endif
		if (visbuf == NULL) {
			paxwarn(1, "Out of memory");
			return;
		}
		/*
		 * using strvis(3) instead of vis(3) to account for multibyte
		 * characters
		 */
#ifdef __APPLE__
		if (strnvis(visbuf, visbufsz, str, VIS_CSTYLE) == -1) {
			paxwarn(1, "Visual encoding failed");
			free(visbuf);
			return;
		}
#else
		(void)strvis(visbuf, str, VIS_CSTYLE);
#endif
		(void)fputs(visbuf, fp);
		free(visbuf);
	} else {
		(void)fputs(str, fp);
	}
}
#endif /* __APPLE__ */

/*
 * asc_ul()
 *	convert hex/octal character string into a u_long. We do not have to
 *	check for overflow! (the headers in all supported formats are not large
 *	enough to create an overflow).
 *	NOTE: strings passed to us are NOT TERMINATED.
 * Return:
 *	unsigned long value
 */

u_long
asc_ul(char *str, int len, int base)
{
	char *stop;
	u_long tval = 0;

	stop = str + len;

	/*
	 * skip over leading blanks and zeros
	 */
	while ((str < stop) && ((*str == ' ') || (*str == '0')))
		++str;

	/*
	 * for each valid digit, shift running value (tval) over to next digit
	 * and add next digit
	 */
	if (base == HEX) {
		while (str < stop) {
			if ((*str >= '0') && (*str <= '9'))
				tval = (tval << 4) + (*str++ - '0');
			else if ((*str >= 'A') && (*str <= 'F'))
				tval = (tval << 4) + 10 + (*str++ - 'A');
			else if ((*str >= 'a') && (*str <= 'f'))
				tval = (tval << 4) + 10 + (*str++ - 'a');
			else
				break;
		}
	} else {
 		while ((str < stop) && (*str >= '0') && (*str <= '7'))
			tval = (tval << 3) + (*str++ - '0');
	}
	return(tval);
}

/*
 * ul_asc()
 *	convert an unsigned long into an hex/oct ascii string. pads with LEADING
 *	ascii 0's to fill string completely
 *	NOTE: the string created is NOT TERMINATED.
 */

int
ul_asc(u_long val, char *str, int len, int base)
{
	char *pt;
	u_long digit;

	/*
	 * WARNING str is not '\0' terminated by this routine
	 */
	pt = str + len - 1;

	/*
	 * do a tailwise conversion (start at right most end of string to place
	 * least significant digit). Keep shifting until conversion value goes
	 * to zero (all digits were converted)
	 */
	if (base == HEX) {
		while (pt >= str) {
			if ((digit = (val & 0xf)) < 10)
				*pt-- = '0' + (char)digit;
			else
				*pt-- = 'a' + (char)(digit - 10);
			if ((val = (val >> 4)) == (u_long)0)
				break;
		}
	} else {
		while (pt >= str) {
			*pt-- = '0' + (char)(val & 0x7);
			if ((val = (val >> 3)) == (u_long)0)
				break;
		}
	}

	/*
	 * pad with leading ascii ZEROS. We return -1 if we ran out of space.
	 */
	while (pt >= str)
		*pt-- = '0';
	if (val != (u_long)0)
		return(-1);
	return(0);
}

/*
 * asc_uqd()
 *	convert hex/octal character string into a u_quad_t. We do not have to
 *	check for overflow! (the headers in all supported formats are not large
 *	enough to create an overflow).
 *	NOTE: strings passed to us are NOT TERMINATED.
 * Return:
 *	u_quad_t value
 */

u_quad_t
asc_uqd(char *str, int len, int base)
{
	char *stop;
	u_quad_t tval = 0;

	stop = str + len;

	/*
	 * skip over leading blanks and zeros
	 */
	while ((str < stop) && ((*str == ' ') || (*str == '0')))
		++str;

	/*
	 * for each valid digit, shift running value (tval) over to next digit
	 * and add next digit
	 */
	if (base == HEX) {
		while (str < stop) {
			if ((*str >= '0') && (*str <= '9'))
				tval = (tval << 4) + (*str++ - '0');
			else if ((*str >= 'A') && (*str <= 'F'))
				tval = (tval << 4) + 10 + (*str++ - 'A');
			else if ((*str >= 'a') && (*str <= 'f'))
				tval = (tval << 4) + 10 + (*str++ - 'a');
			else
				break;
		}
	} else {
 		while ((str < stop) && (*str >= '0') && (*str <= '7'))
			tval = (tval << 3) + (*str++ - '0');
	}
	return(tval);
}

/*
 * uqd_asc()
 *	convert an u_quad_t into a hex/oct ascii string. pads with LEADING
 *	ascii 0's to fill string completely
 *	NOTE: the string created is NOT TERMINATED.
 */

int
uqd_asc(u_quad_t val, char *str, int len, int base)
{
	char *pt;
	u_quad_t digit;

	/*
	 * WARNING str is not '\0' terminated by this routine
	 */
	pt = str + len - 1;

	/*
	 * do a tailwise conversion (start at right most end of string to place
	 * least significant digit). Keep shifting until conversion value goes
	 * to zero (all digits were converted)
	 */
	if (base == HEX) {
		while (pt >= str) {
			if ((digit = (val & 0xf)) < 10)
				*pt-- = '0' + (char)digit;
			else
				*pt-- = 'a' + (char)(digit - 10);
			if ((val = (val >> 4)) == (u_quad_t)0)
				break;
		}
	} else {
		while (pt >= str) {
			*pt-- = '0' + (char)(val & 0x7);
			if ((val = (val >> 3)) == (u_quad_t)0)
				break;
		}
	}

	/*
	 * pad with leading ascii ZEROS. We return -1 if we ran out of space.
	 */
	while (pt >= str)
		*pt-- = '0';
	if (val != (u_quad_t)0)
		return(-1);
	return(0);
}
