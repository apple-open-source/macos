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
 * fsm.h - {Link, IP} Control Protocol Finite State Machine definitions.
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
 * $Id: fsm.h,v 1.2 2000/11/08 01:49:03 callie Exp $
 */

/*
 * Packet header = Code, id, length.
 */
#define HEADERLEN	4


/*
 *  CP (LCP, IPCP, etc.) codes.
 */
#define CONFREQ		1	/* Configuration Request */
#define CONFACK		2	/* Configuration Ack */
#define CONFNAK		3	/* Configuration Nak */
#define CONFREJ		4	/* Configuration Reject */
#define TERMREQ		5	/* Termination Request */
#define TERMACK		6	/* Termination Ack */
#define CODEREJ		7	/* Code Reject */


/*
 * Each FSM is described by an fsm structure and fsm callbacks.
 */
typedef struct fsm {
    struct ppp *ppp;		/* the associated ppp structure */
    int unit;			/* Interface unit number */
    int protocol;		/* Data Link Layer Protocol field value */
    int state;			/* State */
    CFRunLoopTimerRef TORef;	/* timer reference */
    int flags;			/* Contains option bits */
    u_char id;			/* Current id */
    u_char reqid;		/* Current request id */
    u_char seen_ack;		/* Have received valid Ack/Nak/Rej to Req */
    int timeouttime;		/* Timeout time in milliseconds */
    int maxconfreqtransmits;	/* Maximum Configure-Request transmissions */
    int retransmits;		/* Number of retransmissions left */
    int maxtermtransmits;	/* Maximum Terminate-Request transmissions */
    int nakloops;		/* Number of nak loops since last ack */
    int maxnakloops;		/* Maximum number of nak loops tolerated */
    struct fsm_callbacks *callbacks;	/* Callback routines */
    char *term_reason;		/* Reason for closing protocol */
    int term_reason_len;	/* Length of term_reason */
} fsm;


typedef struct fsm_callbacks {
    void (*resetci)		/* Reset our Configuration Information */
		__P((fsm *));
    int  (*cilen)		/* Length of our Configuration Information */
		__P((fsm *));
    void (*addci) 		/* Add our Configuration Information */
		__P((fsm *, u_char *, int *));
    int  (*ackci)		/* ACK our Configuration Information */
		__P((fsm *, u_char *, int));
    int  (*nakci)		/* NAK our Configuration Information */
		__P((fsm *, u_char *, int));
    int  (*rejci)		/* Reject our Configuration Information */
		__P((fsm *, u_char *, int));
    int  (*reqci)		/* Request peer's Configuration Information */
		__P((fsm *, u_char *, int *, int));
    void (*up)			/* Called when fsm reaches OPENED state */
		__P((fsm *));
    void (*down)		/* Called when fsm leaves OPENED state */
		__P((fsm *));
    void (*starting)		/* Called when we want the lower layer */
		__P((fsm *));
    void (*finished)		/* Called when we don't want the lower layer */
		__P((fsm *));
    void (*protreject)		/* Called when Protocol-Reject received */
		__P((int));
    void (*retransmit)		/* Retransmission is necessary */
		__P((fsm *));
    int  (*extcode)		/* Called when unknown code received */
		__P((fsm *, int, int, u_char *, int));
    char *proto_name;		/* String name for protocol (for messages) */
} fsm_callbacks;


/*
 * Link states.
 */
#define INITIAL		0	/* Down, hasn't been opened */
#define STARTING	1	/* Down, been opened */
#define CLOSED		2	/* Up, hasn't been opened */
#define STOPPED		3	/* Open, waiting for down event */
#define CLOSING		4	/* Terminating the connection, not open */
#define STOPPING	5	/* Terminating, but open */
#define REQSENT		6	/* We've sent a Config Request */
#define ACKRCVD		7	/* We've received a Config Ack */
#define ACKSENT		8	/* We've sent a Config Ack */
#define OPENED		9	/* Connection available */


/*
 * Flags - indicate options controlling FSM operation
 */
#define OPT_PASSIVE	1	/* Don't die if we don't get a response */
#define OPT_RESTART	2	/* Treat 2nd OPEN as DOWN, UP */
#define OPT_SILENT	4	/* Wait for peer to speak first */


/*
 * Timeouts.
 */
#define DEFTIMEOUT	3	/* Timeout time in seconds */
#define DEFMAXTERMREQS	2	/* Maximum Terminate-Request transmissions */
#define DEFMAXCONFREQS	10	/* Maximum Configure-Request transmissions */
#define DEFMAXNAKLOOPS	5	/* Maximum number of nak loops */


/*
 * Prototypes
 */
void fsm_init __P((fsm *));
void fsm_lowerup __P((fsm *));
void fsm_lowerdown __P((fsm *));
void fsm_open __P((fsm *));
void fsm_close __P((fsm *, char *));
void fsm_input __P((fsm *, u_char *, int));
void fsm_protreject __P((fsm *));
void fsm_sdata __P((fsm *, int, int, u_char *, int));

