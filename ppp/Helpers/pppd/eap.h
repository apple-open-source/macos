/*
 * eap.h - Extensible Authentication Protocol definitions.
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
 * $Id: eap.h,v 1.2 2002/03/13 22:44:35 callie Exp $
 */

#ifndef __EAP_INCLUDE__

/* Code + ID + length */
#define EAP_HEADERLEN		4

/*
 * EAP codes.
 */
 
/* support for request types 1..4 is mandatory */
#define EAP_REQ_IDENTITY	1	/* request for identity */
#define EAP_REQ_NOTIFICATION	2	/* notification message */
#define EAP_REQ_NAK		3	/* nak request */
#define EAP_REQ_MD5CHALLENGE	4	/* password MD5 coded */

#define EAP_DIGEST_MD5		5	/* use MD5 algorithm */
#define MD5_SIGNATURE_SIZE	16	/* 16 bytes in a MD5 message digest */
#define EAP_MICROSOFT		0x80	/* use Microsoft-compatible alg. */
#define MS_EAP_RESPONSE_LEN	49	/* Response length for MS-EAP */
#define EAP_MICROSOFT_V2	0x81	/* use MS-EAP v2 */

#define EAP_REQUEST		1
#define EAP_RESPONSE		2
#define EAP_SUCCESS		3
#define EAP_FAILURE    		4
#define EAP_SUCCESS_R		13	/* Send response, not text message */

/*
 *  Challenge lengths (for challenges we send) and other limits.
 */
#define MIN_CHALLENGE_LENGTH	16
#define MAX_CHALLENGE_LENGTH	24
#define MAX_RESPONSE_LENGTH	1024	/* Max len for the EAP data part */

/*
 * Each interface is described by a eap structure.
 */

typedef struct eap_state {
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
} eap_state;


/*
 * Client (peer) states.
 */
#define EAPCS_INITIAL		0	/* Lower layer down, not opened */
#define EAPCS_CLOSED		1	/* Lower layer up, not opened */
#define EAPCS_PENDING		2	/* Auth us to peer when lower up */
#define EAPCS_LISTEN		3	/* Listening for a challenge */
#define EAPCS_RESPONSE		4	/* Sent response, waiting for status */
#define EAPCS_OPEN		5	/* We've received Success */

/*
 * Server (authenticator) states.
 */
#define EAPSS_INITIAL		0	/* Lower layer down, not opened */
#define EAPSS_CLOSED		1	/* Lower layer up, not opened */
#define EAPSS_PENDING		2	/* Auth peer when lower up */
#define EAPSS_INITIAL_CHAL	3	/* We've sent the first challenge */
#define EAPSS_OPEN		4	/* We've sent a Success msg */
#define EAPSS_RECHALLENGE	5	/* We've sent another challenge */
#define EAPSS_BADAUTH		6	/* We've sent a Failure msg */

/*
 * Timeouts.
 */
#define EAP_DEFTIMEOUT		3	/* Timeout time in seconds */
#define EAP_DEFTRANSMITS	10	/* max # times to send challenge */

extern eap_state eap[];

void EapAuthWithPeer __P((int, char *));
void EapAuthPeer __P((int, char *));
void EapGenChallenge __P((eap_state *));

int reqeap(char **);

extern struct protent eap_protent;

#define __EAP_INCLUDE__
#endif /* __EAP_INCLUDE__ */
