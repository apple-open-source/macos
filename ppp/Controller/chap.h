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
 * chap.h - Challenge Handshake Authentication Protocol definitions.
 *
 * Copyright (c) 1993 The Australian National University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Australian National University.  The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Copyright (c) 1991 Gregory M. Christy
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the author.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: chap.h,v 1.2 2000/11/08 01:49:02 callie Exp $
 */

#ifndef __CHAP_INCLUDE__

/* Code + ID + length */
#define CHAP_HEADERLEN		4

/*
 * CHAP codes.
 */

#define CHAP_DIGEST_MD5		5	/* use MD5 algorithm */
#define MD5_SIGNATURE_SIZE	16	/* 16 bytes in a MD5 message digest */
#define CHAP_MICROSOFT		0x80	/* use Microsoft-compatible alg. */
#define MS_CHAP_RESPONSE_LEN	49	/* Response length for MS-CHAP */

#define CHAP_CHALLENGE		1
#define CHAP_RESPONSE		2
#define CHAP_SUCCESS		3
#define CHAP_FAILURE    	4

/*
 *  Challenge lengths (for challenges we send) and other limits.
 */
#define MIN_CHALLENGE_LENGTH	16
#define MAX_CHALLENGE_LENGTH	24
#define MAX_RESPONSE_LENGTH	64	/* sufficient for MD5 or MS-CHAP */

/*
 * Each interface is described by a chap structure.
 */

typedef struct chap_state {
    int unit;			/* Interface unit number */
    int clientstate;		/* Client state */
    int serverstate;		/* Server state */
    u_char challenge[MAX_CHALLENGE_LENGTH]; /* last challenge string sent */
    u_char chal_len;		/* challenge length */
    u_char chal_id;		/* ID of last challenge */
    u_char chal_type;		/* hash algorithm for challenges */
    u_char id;			/* Current id */
    char *chal_name;		/* Our name to use with challenge */
    int chal_interval;		/* Time until we challenge peer again */
    int timeouttime;		/* Timeout time in seconds */
    int max_transmits;		/* Maximum # of challenge transmissions */
    int chal_transmits;		/* Number of transmissions of challenge */
    int resp_transmits;		/* Number of transmissions of response */
    u_char response[MAX_RESPONSE_LENGTH];	/* Response to send */
    u_char resp_length;		/* length of response */
    u_char resp_id;		/* ID for response messages */
    u_char resp_type;		/* hash algorithm for responses */
    char *resp_name;		/* Our name to send with response */
} chap_state;


/*
 * Client (peer) states.
 */
#define CHAPCS_INITIAL		0	/* Lower layer down, not opened */
#define CHAPCS_CLOSED		1	/* Lower layer up, not opened */
#define CHAPCS_PENDING		2	/* Auth us to peer when lower up */
#define CHAPCS_LISTEN		3	/* Listening for a challenge */
#define CHAPCS_RESPONSE		4	/* Sent response, waiting for status */
#define CHAPCS_OPEN		5	/* We've received Success */

/*
 * Server (authenticator) states.
 */
#define CHAPSS_INITIAL		0	/* Lower layer down, not opened */
#define CHAPSS_CLOSED		1	/* Lower layer up, not opened */
#define CHAPSS_PENDING		2	/* Auth peer when lower up */
#define CHAPSS_INITIAL_CHAL	3	/* We've sent the first challenge */
#define CHAPSS_OPEN		4	/* We've sent a Success msg */
#define CHAPSS_RECHALLENGE	5	/* We've sent another challenge */
#define CHAPSS_BADAUTH		6	/* We've sent a Failure msg */

/*
 * Timeouts.
 */
#define CHAP_DEFTIMEOUT		3	/* Timeout time in seconds */
#define CHAP_DEFTRANSMITS	10	/* max # times to send challenge */


void ChapAuthWithPeer __P((struct ppp *, char *, int));
void ChapAuthPeer __P((struct ppp *, char *, int));

extern struct protent chap_protent;

#define __CHAP_INCLUDE__
#endif /* __CHAP_INCLUDE__ */
