/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * hostlist.c
 * - in-core host entry list manipulation routines
 * - these are used for storing the in-core version of the 
 *   file-based host list and the in-core ignore list
 */
#import <stdio.h>
#import <stdlib.h>
#import <unistd.h>
#import <ctype.h>
#import <errno.h>
#import <mach/boolean.h>
#import <netdb.h>
#import <sys/socket.h>
#import <sys/ioctl.h>
#import <netinet/in.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/udp.h>
#import <net/if.h>
#import <netinet/if_ether.h>
#import <arpa/inet.h>
#import <signal.h>
#import <string.h>
#import <sys/file.h>
#import <sys/stat.h>
#import <sys/time.h>
#import <sys/types.h>
#import <sys/uio.h>
#import <syslog.h>
#import "hostlist.h"

char *  	ether_ntoa(struct ether_addr *e);

void
hostinsert(struct hosts * * hosts, struct hosts * hp)
{
    hp->next = *hosts;
    hp->prev = NULL;
    if (*hosts)
	(*hosts)->prev = hp;
    *hosts = hp;
}

void
hostremove(struct hosts * * hosts, struct hosts * hp)
{
    if (hp->prev)
	hp->prev->next = hp->next;
    else
	*hosts = hp->next;
    if (hp->next)
	hp->next->prev = hp->prev;
}

void
hostprint(struct hosts * hp)
{
    if (hp)
	syslog(LOG_INFO, 
	       "hw %s type %d len %d ip %s host '%s' bootfile '%s'\n", 
	       ether_ntoa((struct ether_addr *)&hp->haddr), hp->htype,
	       hp->hlen, inet_ntoa(hp->iaddr), 
	       hp->hostname ? hp->hostname : "", 
	       hp->bootfile ? hp->bootfile : "");
}

void
hostfree(struct hosts * * hosts,
	 struct hosts *hp
	 )
{
    hostremove(hosts, hp);
    if (hp->hostname) {
	free(hp->hostname);
	hp->hostname = NULL;
    }
    if (hp->bootfile) {
	free(hp->bootfile);
	hp->bootfile = NULL;
    }
    free((char *)hp);
}

void
hostlistfree(struct hosts * * hosts)
{
    while (*hosts)
	hostfree(hosts, *hosts); /* pop off the head of the queue */
    return;
}

struct hosts * 
hostadd(struct hosts * * hosts, struct timeval * tv_p, int htype,
	char * haddr, int hlen, struct in_addr * iaddr_p, 
	char * hostname, char * bootfile)
{
    struct hosts * hp;

    hp = (struct hosts *)malloc(sizeof(*hp));
    if (!hp)
	return (NULL);
    bzero(hp, sizeof(*hp));
    if (tv_p)
	hp->tv = *tv_p;
    hp->htype = htype;
    hp->hlen = hlen;
    bcopy(haddr, &hp->haddr, MIN(hlen, sizeof(hp->haddr)));
    if (iaddr_p)
	hp->iaddr = *iaddr_p;
    if (hostname)
	hp->hostname = strdup(hostname);
    if (bootfile)
	hp->bootfile = strdup(bootfile);
    hostinsert(hosts, hp);
    return (hp);
}
