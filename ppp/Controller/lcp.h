/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 * lcp.h - Link Control Protocol definitions.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: lcp.h,v 1.2 2000/11/08 01:49:03 callie Exp $
 */

/*
 * Options.
 */
#define CI_MRU		1	/* Maximum Receive Unit */
#define CI_ASYNCMAP	2	/* Async Control Character Map */
#define CI_AUTHTYPE	3	/* Authentication Type */
#define CI_QUALITY	4	/* Quality Protocol */
#define CI_MAGICNUMBER	5	/* Magic Number */
#define CI_PCOMPRESSION	7	/* Protocol Field Compression */
#define CI_ACCOMPRESSION 8	/* Address/Control Field Compression */
#define CI_CALLBACK	13	/* callback */

/*
 * LCP-specific packet types.
 */
#define PROTREJ		8	/* Protocol Reject */
#define ECHOREQ		9	/* Echo Request */
#define ECHOREP		10	/* Echo Reply */
#define DISCREQ		11	/* Discard Request */
#define CBCP_OPT	6	/* Use callback control protocol */

/*
 * The state of options is described by an lcp_options structure.
 */
typedef struct lcp_options {
    u_char passive;		/* Don't die if we don't get a response */
    u_char silent;		/* Wait for the other end to start first */
    u_char restart;		/* Restart vs. exit after close */
    u_char neg_mru;		/* Negotiate the MRU? */
    u_char neg_asyncmap;		/* Negotiate the async map? */
    u_char neg_upap;		/* Ask for UPAP authentication? */
    u_char neg_chap;		/* Ask for CHAP authentication? */
    u_char neg_magicnumber;	/* Ask for magic number? */
    u_char neg_pcompression;	/* HDLC Protocol Field Compression? */
    u_char neg_accompression;	/* HDLC Address/Control Field Compression? */
    u_char neg_lqr;		/* Negotiate use of Link Quality Reports */
    u_char neg_cbcp;		/* Negotiate use of CBCP */
    int  mru;			/* Value of MRU */
    u_char chap_mdtype;		/* which MD type (hashing algorithm) */
    u_int32_t asyncmap;		/* Value of async map */
    u_int32_t magicnumber;
    int numloops;		/* Number of loops during magic number neg. */
    u_int32_t lqr_period;	/* Reporting period for LQR 1/100ths second */
} lcp_options;

#define DEFMRU	1500		/* Try for this */
#define MINMRU	128		/* No MRUs below this */
#define MAXMRU	16384		/* Normally limit MRU to this */

void lcp_open(struct ppp *);
void lcp_close(struct ppp *, char *);
void lcp_lowerup(struct ppp *);
void lcp_lowerdown(struct ppp *);
void lcp_sprotrej(struct ppp *, u_char *, int);	/* send protocol reject */

extern struct protent lcp_protent;

/* Default number of times we receive our magic number from the peer
   before deciding the link is looped-back. */
#define DEFLOOPBACKFAIL	10
