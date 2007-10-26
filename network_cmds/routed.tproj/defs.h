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
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgment:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)defs.h	8.1 (Berkeley) 6/5/93
 */

/*
 * Internal data structure definitions for
 * user routing process.  Based on Xerox NS
 * protocol specs with mods relevant to more
 * general addressing scheme.
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

struct mbuf; /* forward reference */
#include <net/route.h>
#include <netinet/in.h>
#include <protocols/routed.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

/* NeXT */
#ifndef EXTERN
#define EXTERN extern
#endif

#include "trace.h"
#include "interface.h"
#include "table.h"
#include "af.h"

/*
 * When we find any interfaces marked down we rescan the
 * kernel every CHECK_INTERVAL seconds to see if they've
 * come up.
 */
#define	CHECK_INTERVAL	(1*60)

#define equal(a1, a2) \
	(memcmp((a1), (a2), sizeof (struct sockaddr)) == 0)

EXTERN struct	sockaddr_in addr;	/* address of daemon's socket */

EXTERN int	s;			/* source and sink of all data */
EXTERN int	r;			/* routing socket */
EXTERN pid_t	pid;			/* process id for identifying messages */
EXTERN uid_t	uid;			/* user id for identifying messages */
EXTERN int	seqno;			/* sequence number for identifying messages */
EXTERN int	kmem;
extern int	supplier;		/* process should supply updates */
extern int	install;		/* if 1 call kernel */
extern int	lookforinterfaces;	/* if 1 probe kernel for new up interfaces */
EXTERN int	performnlist;		/* if 1 check if /vmunix has changed */
extern int	externalinterfaces;	/* # of remote and local interfaces */
EXTERN struct	timeval now;		/* current idea of time */
EXTERN struct	timeval lastbcast;	/* last time all/changes broadcast */
EXTERN struct	timeval lastfullupdate;	/* last time full table broadcast */
EXTERN struct	timeval nextbcast;	/* time to wait before changes broadcast */
EXTERN int	needupdate;		/* true if we need update at nextbcast */

EXTERN char	packet[MAXPACKETSIZE+1];
extern struct	rip *msg;

EXTERN char	**argv0;
EXTERN struct	servent *sp;

#define ADD 1
#define DELETE 2
#define CHANGE 3

/* arpa/inet.h */
in_addr_t inet_addr(const char *);
in_addr_t inet_network(const char *);
char *inet_ntoa(struct in_addr);

/* inet.c */
struct in_addr inet_makeaddr(u_long, u_long);
int inet_netof(struct in_addr);
int inet_lnaof(struct in_addr);
int inet_maskof(u_long);
int inet_rtflags(struct sockaddr_in *);
int inet_sendroute(struct rt_entry *, struct sockaddr_in *);

/* input.c */
void rip_input(struct sockaddr *, struct rip *, int);

/* main.c */
void timevaladd(struct timeval *, struct timeval *);

/* startup.c */
void quit(char *);
void rt_xaddrs(caddr_t, caddr_t, struct rt_addrinfo *);
void ifinit(void);
void addrouteforif(struct interface *);
void add_ptopt_localrt(struct interface *);
void gwkludge(void);
int getnetorhostname(char *, char *, struct sockaddr_in *);
int gethostnameornumber(char *, struct sockaddr_in *);

/* output.c */
void toall(int (*)(), int, struct interface *);
void sndmsg(struct sockaddr *, int, struct interface *, int);
void supply(struct sockaddr *, int, struct interface *, int);
