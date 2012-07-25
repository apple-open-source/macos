/*
 * Copyright (c) 1999-2008 Apple Inc. All rights reserved.
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
 * bootpdfile.c
 * - read bootptab to get the default boot file and path
 * - parse the list of hardware address to ip bindings
 * - lookup host entries in the file-based host list
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/bootp.h>
#include <netinet/if_ether.h>
#include <mach/boolean.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <sys/uio.h>

#include "bootpdfile.h"
#include "hostlist.h"

#define HTYPE_ETHER		1
#define NUM_EN_ADDR_BYTES	6

#define HOSTNAME_MAX		64
#define BOOTFILE_MAX		128

#define ETC_BOOTPTAB	"/etc/bootptab"
static long	modtime = 0;	/* last modification time of bootptab */
static struct hosts * 	S_file_hosts = NULL; /* list of host entries from the file */

/*
 * Get next field from 'line' buffer into 'str'.  'linep' is the 
 * pointer to current position.
 */
static void
S_getfield(char * * line_p, int linenum, char *str, int len)
{
	register char *cp = str;
	char * linep = *line_p;

	for ( ; *linep && (*linep == ' ' || *linep == '\t') ; linep++)
		;	/* skip spaces/tabs */
	if (*linep == 0) {
		*cp = 0;
		goto done;
	}
	len--;	/* save a spot for a null */
	for ( ; *linep && *linep != ' ' && *linep != '\t' ; linep++) {
		*cp++ = *linep;
		if (--len <= 0) {
			*cp = 0;
			syslog(LOG_NOTICE, "string truncated: %s,"
			       " on line %d of bootptab", str, linenum);
			goto done;
		}
	}
	*cp = 0;
 done:
	*line_p = linep;
}

/*
 * Read bootptab database file.  Avoid rereading the file if the
 * write date hasnt changed since the last time we read it.
 */
void
bootp_readtab(const char * filename)
{
    struct stat st;
    register char *cp;
    int	host_count = 0;
    int host_count_all = 0;
    register int i;
    char line[256];	/* line buffer for reading bootptab */
    char temp[64], tempcpy[64];
    register struct hosts *hp, *thp;
    char * linep;
    int linenum;
    int skiptopercent;
    FILE * fp = NULL;
    
    if (filename == NULL) {
	filename = ETC_BOOTPTAB;
    }
    if ((fp = fopen(filename, "r")) == NULL) {
	syslog(LOG_INFO, "can't open %s", filename);
	return;
    }
    if (fstat(fileno(fp), &st) == 0 
	&& st.st_mtime == modtime) {
	fclose(fp);
	return;	/* hasn't been modified */
    }
    syslog(LOG_NOTICE, "re-reading %s", filename);
    modtime = st.st_mtime;
    linenum = 0;
    skiptopercent = 1;
    
    /*
     * Free old file entries.
     */
    hp = S_file_hosts;
    while (hp) {
	thp = hp->next;
	hostfree(&S_file_hosts, hp);
	hp = thp;
    }
    
    /*
     * read and parse each line in the file.
     */
    for (;;) {
	boolean_t	all_zeroes;
	boolean_t	good_hwaddr;
	char hostname[HOSTNAME_MAX];
	char bootfile[BOOTFILE_MAX];
	struct in_addr iaddr;
	int htype;
	int hlen;
	char haddr[32];
	
	if (fgets(line, sizeof line, fp) == NULL)
	    break;	/* done */

	if ((i = strlen(line)) != 0)
	    line[i-1] = 0;	/* remove trailing newline */

	linep = line;
	linenum++;
	if (line[0] == '#' || line[0] == 0 || line[0] == ' ')
	    continue;	/* skip comment lines */

	if (skiptopercent) {	/* allow for future leading fields */
	    if (line[0] != '%')
		continue;
	    skiptopercent = 0;
	    continue;
	}
	host_count_all++;
	/* fill in host table */
	S_getfield(&linep, linenum, hostname, sizeof(hostname) - 1);
	S_getfield(&linep, linenum, temp, sizeof temp);
	sscanf(temp, "%d", &htype);
	S_getfield(&linep, linenum, temp, sizeof temp);
	strcpy(tempcpy, temp);
	cp = tempcpy;
	/* parse hardware address */
	good_hwaddr = TRUE;
	all_zeroes = TRUE;
	for (hlen = 0; hlen < sizeof(haddr);) {
	    char *cpold;
	    char c;
	    int v;

	    cpold = cp;
	    while (*cp != '.' && *cp != ':' && *cp != 0)
		cp++;
	    c = *cp;	/* save original terminator */
	    *cp = 0;
	    cp++;
	    if (sscanf(cpold, "%x", &v) != 1) {
		good_hwaddr = FALSE;
		syslog(LOG_NOTICE, "bad hex address: %s,"
		       " at line %d of bootptab", temp, linenum);
		break;
	    }
	    haddr[hlen++] = v;
	    if (v != 0) {
		all_zeroes = FALSE;
	    }
	    if (c == 0)
		break;
	}
	if (good_hwaddr == FALSE) {
	    continue;
	}
	if (all_zeroes) {
	    syslog(LOG_NOTICE, "zero hex address: %s,"
		   " at line %d of bootptab", temp, linenum);
	    continue;
	}
	if (htype == HTYPE_ETHER && hlen != NUM_EN_ADDR_BYTES) {
	    syslog(LOG_NOTICE, "bad hex address: %s,"
		   " at line %d of bootptab", temp, linenum);
	    continue;
	}
	S_getfield(&linep, linenum, temp, sizeof(temp));
	iaddr.s_addr = inet_addr(temp);
	if (iaddr.s_addr == -1 || iaddr.s_addr == 0) {
	    syslog(LOG_NOTICE, "bad internet address: %s,"
		   " at line %d of bootptab", temp, linenum);
	    continue;
	}
	S_getfield(&linep, linenum, bootfile, sizeof(bootfile) - 1);
	(void)hostadd(&S_file_hosts, NULL, htype, haddr, hlen, &iaddr,
		      hostname, bootfile);
	host_count++;
    }
    fclose(fp);
    syslog(LOG_NOTICE, "Loaded %d entries from bootptab (%d bad)",
	   host_count, host_count_all - host_count);
    return;
}

boolean_t
bootp_getbyhw_file(uint8_t hwtype, void * hwaddr, int hwlen, 
		   subnet_match_func_t * func, void * arg,
		   struct in_addr * iaddr_p, 
		   char * * hostname_p, char * * bootfile_p)
{
    struct hosts * hp;

    hp = hostbyaddr(S_file_hosts, hwtype, hwaddr, hwlen,
		    func, arg);
    if (hp == NULL)
	return (FALSE);
    if (hostname_p)
	*hostname_p = strdup(hp->hostname);
    if (bootfile_p)
	*bootfile_p = strdup(hp->bootfile);
    *iaddr_p = hp->iaddr;
    return (TRUE);
}

boolean_t
bootp_getbyip_file(struct in_addr ciaddr, char * * hostname_p, 
		   char * * bootfile_p)
{
    struct hosts * hp;

    hp = hostbyip(S_file_hosts, ciaddr);
    if (hp == NULL)
	return (FALSE);

    if (hostname_p)
	*hostname_p = strdup(hp->hostname);
    if (bootfile_p)
	*bootfile_p = strdup(hp->bootfile);
    return (TRUE);
}

#ifdef MAIN
#define USECS_PER_SEC	1000000
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

void
timestamp_printf(char * msg)
{
    static struct timeval	tvp = {0,0};
    struct timeval		tv;

    gettimeofday(&tv, 0);
    if (tvp.tv_sec) {
	struct timeval result;
	
	timeval_subtract(tv, tvp, &result);
	printf("%d.%06d (%d.%06d): %s\n", 
	       (int)tv.tv_sec, 
	       (int)tv.tv_usec,
	       (int)result.tv_sec,
	       (int)result.tv_usec, msg);
    }
    else 
	printf("%d.%06d (%d.%06d): %s\n", 
	       (int)tv.tv_sec, (int)tv.tv_usec, 0, 0, msg);
    tvp = tv;
}

int
main(int argc, char * argv[])
{
    struct ether_addr *	ea;
    struct in_addr ip;
    char *	host = NULL;
    boolean_t	found;
    const char *	file = NULL;

    if (argc < 2) {
	fprintf(stderr, "usage: %s ethernet_address\n", argv[0]);
	exit(1);
    }
    ea = ether_aton(argv[1]);
    if (ea == NULL) {
	fprintf(stderr, "invalid ethernet address '%s'\n", argv[1]);
	exit(1);
    }
    if (argc > 2) {
	file = argv[2];
    }
    timestamp_printf("before read");
    bootp_readtab(file);
    timestamp_printf("after read");

    timestamp_printf("before lookup");
    found = bootp_getbyhw_file(HTYPE_ETHER, ea, 6,
			       NULL, NULL,
			       &ip, &host, NULL);
    timestamp_printf("after lookup");
    if (found) {
	printf("%s IP is %s\n", host, inet_ntoa(ip));
    }
    else {
	printf("Not found\n");
    }
    if (host != NULL) free(host);
    exit(0);

}
#endif /* MAIN */
