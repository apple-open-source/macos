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
 * Copyright (c) 1992, 1993
 *	Regents of the University of California.  All rights reserved.
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
 *    must display the following acknowledgement:
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
 *	@(#)netstat.h	8.2 (Berkeley) 1/4/94
 */

#include <sys/cdefs.h>
#include <sys/types.h>

#ifndef EXTERN
#define EXTERN extern
#endif
EXTERN int	Aflag;		/* show addresses of protocol control block */
EXTERN int	aflag;		/* show all sockets (including servers) */
EXTERN int	bflag;		/* show i/f total bytes in/out */
EXTERN int	dflag;		/* show i/f dropped packets */
EXTERN int	gflag;		/* show group (multicast) routing or stats */
EXTERN int	iflag;		/* show static interfaces */
EXTERN int	mflag;		/* show memory stats */
EXTERN int	nflag;		/* show addresses numerically */
EXTERN int	pflag;		/* show given protocol */
EXTERN int	rflag;		/* show routing tables (or routing stats) */
EXTERN int	sflag;		/* show protocol statistics */
EXTERN int	tflag;		/* show i/f watchdog timers */

EXTERN int	interval;	/* repeat interval for i/f stats */

EXTERN char	*interface;	/* desired i/f for stats, or NULL for all i/fs */
EXTERN int	unit;		/* unit number for above */

EXTERN int	af;		/* address family */

int	kread __P((u_long addr, char *buf, int size));
char	*plural __P((int));
char	*plurales __P((int));
void	trimdomain __P((char *));

void	protopr __P((u_long, char *));
void	tcp_stats __P((u_long, char *));
void	udp_stats __P((u_long, char *));
void	ip_stats __P((u_long, char *));
void	icmp_stats __P((u_long, char *));
void	igmp_stats __P((u_long, char *));
void	protopr __P((u_long, char *));

void	mbpr __P((u_long));

void	hostpr __P((u_long, u_long));
void	impstats __P((u_long, u_long));

void	intpr __P((int, u_long));

void	pr_rthdr __P(());
void	pr_family __P((int));
void	rt_stats __P((u_long));
char	*ipx_pnet __P((struct sockaddr *));
char	*ipx_phost __P((struct sockaddr *));
char	*ns_phost __P((struct sockaddr *));
void	upHex __P((char *));

char	*routename __P((u_long));
char	*netname __P((u_long, u_long));
char	*atalk_print __P((struct sockaddr *, int));
char	*atalk_print2 __P((struct sockaddr *, struct sockaddr *, int));
char	*ipx_print __P((struct sockaddr *));
char	*ns_print __P((struct sockaddr *));
void	routepr __P((u_long));

void	ipxprotopr __P((u_long, char *));
void	spx_stats __P((u_long, char *));
void	ipx_stats __P((u_long, char *));
void	ipxerr_stats __P((u_long, char *));

void	nsprotopr __P((u_long, char *));
void	spp_stats __P((u_long, char *));
void	idp_stats __P((u_long, char *));
void	nserr_stats __P((u_long, char *));

void	atalkprotopr __P((u_long, char *));
void	ddp_stats __P((u_long, char *));

void	intpr __P((int, u_long));

void	unixpr __P((void));

void	esis_stats __P((u_long, char *));
void	clnp_stats __P((u_long, char *));
void	cltp_stats __P((u_long, char *));
void	iso_protopr __P((u_long, char *));
void	iso_protopr1 __P((u_long, int));
void	tp_protopr __P((u_long, char *));
void	tp_inproto __P((u_long));
void	tp_stats __P((caddr_t, caddr_t));

void	mroutepr __P((u_long, u_long));
void	mrt_stats __P((u_long));

