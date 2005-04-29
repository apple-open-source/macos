/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * util.c
 * - contains miscellaneous routines
 */
#import <stdio.h>
#import <unistd.h>
#import <stdlib.h>
#import <netinet/in.h>
#import <sys/types.h>
#import <sys/stat.h>
#import <sys/param.h>
#import <sys/syslog.h>
#import <errno.h>
#import <mach/boolean.h>
#import <string.h>
#import <ctype.h>

#import "util.h"

int 
ipRangeCmp(ip_range_t * a_p, ip_range_t * b_p, boolean_t * overlap)
{
    u_long		b_start = iptohl(b_p->start);
    u_long		b_end = iptohl(b_p->end);
    u_long		a_start = iptohl(a_p->start);
    u_long		a_end = iptohl(a_p->end);
    int			result = 0;
    
    *overlap = TRUE;
    if (a_start == b_start) {
	result = 0;
    }
    else if (a_start < b_start) {
	result = -1;
	if (a_end < b_start)
	    *overlap = FALSE;
    }
    else {
	result = 1;
	if (b_end < a_start)
	    *overlap = FALSE;
    }
    return (result);
}

/* 
 * Function: nbits_host
 * Purpose:
 *   Return the number of bits of host address 
 */
int
nbits_host(struct in_addr mask)
{
    u_long l = iptohl(mask);
    int i;

    for (i = 0; i < 32; i++) {
	if (l & (1 << i))
	    return (32 - i);
    }
    return (32);
}

/*
 * Function: inet_nettoa
 * Purpose:
 *   Turns a network address (expressed as an IP address and mask)
 *   into a string e.g. 17.202.40.0 and mask 255.255.252.0 yields
 *   the string  "17.202.40/22".
 */
u_char *
inet_nettoa(struct in_addr addr, struct in_addr mask)
{
    u_char *		addr_p;
    int 		nbits = nbits_host(mask);
    int 		nbytes;
    static u_char 	sbuf[32];
    u_char 		tmp[8];

#define NBITS_PER_BYTE	8    
    sbuf[0] = '\0';
    nbytes = (nbits + NBITS_PER_BYTE - 1) / NBITS_PER_BYTE;
//    printf("-- nbits %d, nbytes %d--", nbits, nbytes);
    for (addr_p = (u_char *)&addr.s_addr; nbytes > 0; addr_p++) {

	sprintf(tmp, "%d%s", *addr_p, nbytes > 1 ? "." : "");
	strcat(sbuf, tmp);
	nbytes--;
    }
    if (nbits % NBITS_PER_BYTE) {
	sprintf(tmp, "/%d", nbits);
	strcat(sbuf, tmp);
    }
    return (sbuf);
}

/* 
 * Function: random_range
 * Purpose:
 *   Return a random number in the given range.
 */
long
random_range(long bottom, long top)
{
    long ret;
    long number = top - bottom + 1;
    long range_size = LONG_MAX / number;
    if (range_size == 0)
	return (bottom);
    ret = (random() / range_size) + bottom;
    return (ret);
}

/*
 * Function: timeval_subtract
 *
 * Purpose:
 *   Computes result = tv1 - tv2.
 */
void
timeval_subtract(struct timeval tv1, struct timeval tv2, 
		 struct timeval * result)
{
    result->tv_sec = tv1.tv_sec - tv2.tv_sec;
    result->tv_usec = tv1.tv_usec - tv2.tv_usec;
    if (result->tv_usec < 0) {
	result->tv_usec += USECS_PER_SEC;
	result->tv_sec--;
    }
    return;
}

/*
 * Function: timeval_add
 *
 * Purpose:
 *   Computes result = tv1 + tv2.
 */
void
timeval_add(struct timeval tv1, struct timeval tv2,
	    struct timeval * result)
{
    result->tv_sec = tv1.tv_sec + tv2.tv_sec;
    result->tv_usec = tv1.tv_usec + tv2.tv_usec;
    if (result->tv_usec > USECS_PER_SEC) {
	result->tv_usec -= USECS_PER_SEC;
	result->tv_sec++;
    }
    return;
}

/*
 * Function: timeval_compare
 *
 * Purpose:
 *   Compares two timeval values, tv1 and tv2.
 *
 * Returns:
 *   -1		if tv1 is less than tv2
 *   0 		if tv1 is equal to tv2
 *   1 		if tv1 is greater than tv2
 */
int
timeval_compare(struct timeval tv1, struct timeval tv2)
{
    struct timeval result;

    timeval_subtract(tv1, tv2, &result);
    if (result.tv_sec < 0 || result.tv_usec < 0)
	return (-1);
    if (result.tv_sec == 0 && result.tv_usec == 0)
	return (0);
    return (1);
}

/*
 * Function: fprint_data
 * Purpose:
 *   Displays the buffer as a series of 8-bit hex numbers with an ASCII
 *   representation off to the side.
 */
void
fprint_data(FILE * f, u_char * data_p, int n_bytes)
{
#define CHARS_PER_LINE 	16
    char		line_buf[CHARS_PER_LINE + 1];
    int			line_pos;
    int			offset;

    for (line_pos = 0, offset = 0; offset < n_bytes; offset++, data_p++) {
	if (line_pos == 0)
	    fprintf(f, "%04x ", offset);

	line_buf[line_pos] = isprint(*data_p) ? *data_p : '.';
	printf(" %02x", *data_p);
	line_pos++;
	if (line_pos == CHARS_PER_LINE) {
	    line_buf[CHARS_PER_LINE] = '\0';
	    printf("  %s\n", line_buf);
	    line_pos = 0;
	}
	else if (line_pos == (CHARS_PER_LINE / 2))
	    printf(" ");
    }
    if (line_pos) { /* need to finish up the line */
	for (; line_pos < CHARS_PER_LINE; line_pos++) {
	    printf("   ");
	    line_buf[line_pos] = ' ';
	}
	line_buf[CHARS_PER_LINE] = '\0';
	printf("  %s\n", line_buf);
    }
}

void
print_data(u_char * data_p, int n_bytes)
{
    fprint_data(stdout, data_p, n_bytes);
}

/*
 * Function: create_path
 *
 * Purpose:
 *   Create the given directory hierarchy.  Return -1 if anything
 *   went wrong, 0 if successful.
 */
int
create_path(u_char * dirname, mode_t mode)
{
    boolean_t	done = FALSE;
    u_char *	scan;

    if (mkdir(dirname, mode) == 0 || errno == EEXIST)
	return (0);

    if (errno != ENOENT)
	return (-1);

    {
	u_char	path[PATH_MAX];

	for (path[0] = '\0', scan = dirname; done == FALSE;) {
	    u_char * 	next_sep;
	    
	    if (scan == NULL || *scan != '/')
		return (FALSE);
	    scan++;
	    next_sep = strchr(scan, '/');
	    if (next_sep == 0) {
		done = TRUE;
		next_sep = dirname + strlen(dirname);
	    }
	    strncpy(path, dirname , next_sep - dirname);
	    path[next_sep - dirname] = '\0';
	    if (mkdir(path, mode) == 0 || errno == EEXIST)
		;
	    else
		return (-1);
	    scan = next_sep;
	}
    }
    return (0);
}

static __inline__ char *
find_char(char ch, char * data, char * data_end)
{
    char * scan;

    for (scan = data; scan < data_end; scan++) {
	if (*scan == ch)
	    return (scan);
    }
    return (NULL);
}

char *
tagtext_get(char * data, char * data_end, char * tag, char * * end_p)
{
    int		tag_len = strlen(tag);
    char * 	start = NULL;
    char * 	scan;
    *end_p = NULL;

    for (scan = data; scan < data_end; ) {
	scan = find_char('<', scan, data_end);
	if (scan == NULL)
	    goto done;
	if (start == NULL) {
	    if ((scan + 1 + tag_len + 1) > data_end)
		goto done;
	    if (strncmp(scan + 1, tag, tag_len) == 0
		&& scan[tag_len + 1] == '>') {
		start = scan + tag_len + 1 + 1;
#ifdef DEBUG
		printf("start of %s found at %d\n", tag,
		       start - data);
#endif DEBUG
		scan += 1 + tag_len + 1;
	    }
	    else
		scan++;
	}
	else {
	    if ((scan + 1 + tag_len + 1 + 1) > data_end) {
		goto done;
	    }
	    if (scan[1] == '/'
		&& strncmp(scan + 2, tag, tag_len) == 0
		&& scan[1 + tag_len + 1] == '>') {
		*end_p = scan;
#ifdef DEBUG
		printf("end of %s found at %d\n", tag, *end_p - data);
#endif DEBUG
		goto done;
	    }
	    else
		scan++;
	}
    }
 done:
    if (*end_p == NULL)
	return (NULL);
    return (start);
}

int
ether_cmp(struct ether_addr * e1, struct ether_addr * e2)
{
    int i;
    u_char * c1 = e1->octet;
    u_char * c2 = e2->octet;

    for (i = 0; i < sizeof(e1->octet); i++, c1++, c2++) {
	if (*c1 == *c2)
	    continue;
	return ((int)*c1 - (int)*c2);
    }
    return (0);
}


