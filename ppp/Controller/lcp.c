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
 * lcp.c - PPP Link Control Protocol.
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
 */

/* -----------------------------------------------------------------------------
includes
----------------------------------------------------------------------------- */
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <CoreFoundation/CoreFoundation.h>

#include "ppp_msg.h"
#include "../Family/PPP.kmodproj/ppp.h"

#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "chap.h"
#include "upap.h"
#include "auth.h"
#include "ppp_client.h"
#include "ppp_utils.h"
#include "ppp_command.h"
#include "ppp_manager.h"
#include "ppp_utils.h"


/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

/*
 * Callbacks for fsm code.  (CI = Configuration Information)
 */

static void lcp_resetci(fsm *);		/* Reset our CI */
static int  lcp_cilen(fsm *);		/* Return length of our CI */
static void lcp_addci(fsm *, u_char *, int *); /* Add our CI to pkt */
static int  lcp_ackci(fsm *, u_char *, int); /* Peer ack'd our CI */
static int  lcp_nakci(fsm *, u_char *, int); /* Peer nak'd our CI */
static int  lcp_rejci(fsm *, u_char *, int); /* Peer rej'd our CI */
static int  lcp_reqci(fsm *, u_char *, int *, int); /* Rcv peer CI */
static void lcp_up(fsm *);		/* We're UP */
static void lcp_down(fsm *);		/* We're DOWN */
static void lcp_starting(fsm *);	/* We need lower layer up */
static void lcp_finished(fsm *);	/* We need lower layer down */
static int  lcp_extcode(fsm *, int, int, u_char *, int);
static void lcp_rprotrej(fsm *, u_char *, int);

/*
 * routines to send LCP echos to peer
 */

static void lcp_echo_lowerup(struct ppp *ppp);
static void lcp_echo_lowerdown(struct ppp *ppp);
static void LcpEchoTimeout(CFRunLoopTimerRef, void *);
static void lcp_received_echo_reply(fsm *, int, u_char *, int);
static void LcpSendEchoRequest(fsm *);
static void LcpLinkFailure(fsm *);
static void LcpEchoCheck(fsm *);

static void lcp_init(struct ppp *);
static void lcp_input(struct ppp *, u_char *, int);
static void lcp_protrej(struct ppp *);
static int  lcp_printpkt(struct ppp *ppp, u_char *, int, void (*)(struct ppp *, void *, char *, ...), void *);

/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

static fsm_callbacks lcp_callbacks = {	/* LCP callback routines */
    lcp_resetci,		/* Reset our Configuration Information */
    lcp_cilen,			/* Length of our Configuration Information */
    lcp_addci,			/* Add our Configuration Information */
    lcp_ackci,			/* ACK our Configuration Information */
    lcp_nakci,			/* NAK our Configuration Information */
    lcp_rejci,			/* Reject our Configuration Information */
    lcp_reqci,			/* Request peer's Configuration Information */
    lcp_up,			/* Called when fsm reaches OPENED state */
    lcp_down,			/* Called when fsm leaves OPENED state */
    lcp_starting,		/* Called when we want the lower layer up */
    lcp_finished,		/* Called when we want the lower layer down */
    NULL,			/* Called when Protocol-Reject received */
    NULL,			/* Retransmission is necessary */
    lcp_extcode,		/* Called to handle LCP-specific codes */
    "LCP"			/* String name of protocol */
};

/* Protocol entry points. Some of these are called directly. */


struct protent lcp_protent = {
    PPP_LCP,
    lcp_init,
    lcp_input,
    lcp_protrej,
    lcp_lowerup,
    lcp_lowerdown,
    lcp_open,
    lcp_close,
    lcp_printpkt,
    NULL,
    "LCP",
    NULL,
    NULL, //lcp_option_list,
    NULL,
    NULL,
    NULL
};


/*
 * Length of each type of configuration option (in octets)
 */
#define CILEN_VOID	2
#define CILEN_CHAR	3
#define CILEN_SHORT	4	/* CILEN_VOID + 2 */
#define CILEN_CHAP	5	/* CILEN_VOID + 2 + 1 */
#define CILEN_LONG	6	/* CILEN_VOID + 4 */
#define CILEN_LQR	8	/* CILEN_VOID + 2 + 4 */
#define CILEN_CBCP	3

#define CODENAME(x)	((x) == CONFACK ? "ACK" : \
			 (x) == CONFNAK ? "NAK" : "REJ")


/* -----------------------------------------------------------------------------
* lcp_init - Initialize LCP.
----------------------------------------------------------------------------- */
static void lcp_init(struct ppp *ppp)
{
    fsm 	*f = &ppp->lcp_fsm;
    lcp_options *wo = &ppp->lcp_wantoptions;
    lcp_options *ao = &ppp->lcp_allowoptions;
    lcp_options *ho = &ppp->lcp_hisoptions;
    lcp_options *go = &ppp->lcp_gotoptions;


    f->ppp = ppp;
    f->unit = ppp->unit;
    f->protocol = PPP_LCP;
    f->callbacks = &lcp_callbacks;

    fsm_init(f);
    memset(wo, 0, sizeof(*wo));
    memset(ao, 0, sizeof(*ao));
    memset(go, 0, sizeof(*go));
    memset(ho, 0, sizeof(*ho));

    wo->passive = 0;
    wo->silent = 0;
    wo->restart = 0;			/* Set to 1 in kernels or multi-line
					   implementations */
    wo->neg_mru = 1;
    wo->mru = DEFMRU;
    wo->neg_asyncmap = 1;
    wo->asyncmap = 0;
    wo->neg_chap = 0;			/* Set to 1 on server */
    wo->neg_upap = 0;			/* Set to 1 on server */
    wo->chap_mdtype = CHAP_DIGEST_MD5;
    wo->neg_magicnumber = 1;
    wo->neg_pcompression = 1;
    wo->neg_accompression = 1;
    wo->neg_lqr = 0;			/* no LQR implementation yet */
    wo->neg_cbcp = 0;

    ao->neg_mru = 1;
    ao->mru = MAXMRU;
    ao->neg_asyncmap = 1;
    ao->asyncmap = 0;
    ao->neg_chap = 1;
    ao->chap_mdtype = CHAP_DIGEST_MD5;
    ao->neg_upap = 1;
    ao->neg_magicnumber = 1;
    ao->neg_pcompression = 1;
    ao->neg_accompression = 1;
    ao->neg_lqr = 0;			/* no LQR implementation yet */
#ifdef CBCP_SUPPORT
    ao->neg_cbcp = 1;
#else
    ao->neg_cbcp = 0;
#endif

    memset(ppp->lcp_xmit_accm, 0, sizeof(ppp->lcp_xmit_accm));
    ppp->lcp_xmit_accm[3] = 0x60000000;

    ppp->lcp_loopbackfail = DEFLOOPBACKFAIL;

    ppp->lcp_echo_interval = 0;
    ppp->lcp_echo_fails = 0;
    ppp->lcp_lax_recv = 1;

    ppp->lcp_echoTORef = 0;
}

/* -----------------------------------------------------------------------------
* lcp_open - LCP is allowed to come up.
----------------------------------------------------------------------------- */
void lcp_open(struct ppp *ppp)
{
    fsm 	*f = &ppp->lcp_fsm;
    lcp_options *wo = &ppp->lcp_wantoptions;

    f->flags = 0;
    if (wo->passive)
	f->flags |= OPT_PASSIVE;
    if (wo->silent)
	f->flags |= OPT_SILENT;
    fsm_open(f);
}

/* -----------------------------------------------------------------------------
* lcp_close - Take LCP down.
----------------------------------------------------------------------------- */
void lcp_close(struct ppp *ppp, char *reason)
{
    fsm 	*f = &ppp->lcp_fsm;

    if (ppp->phase != PPP_IDLE)
        ppp_new_phase(ppp, PPP_TERMINATE);
    if (f->state == STOPPED && f->flags & (OPT_PASSIVE|OPT_SILENT)) {
	/*
	 * This action is not strictly according to the FSM in RFC1548,
	 * but it does mean that the program terminates if you do a
	 * lcp_close() in passive/silent mode when a connection hasn't
	 * been established.
	 */
	f->state = CLOSED;
	lcp_finished(f);

    } else
	fsm_close(f, reason);
}

/* -----------------------------------------------------------------------------
* lcp_lowerup - The lower layer is up.
----------------------------------------------------------------------------- */
void lcp_lowerup(struct ppp *ppp)
{
    lcp_options *wo = &ppp->lcp_wantoptions;

    ppp_new_event(ppp, PPP_EVT_LOWERLAYER_UP);
    ppp_new_phase(ppp, PPP_ESTABLISH);
   /*
     * Don't use A/C or protocol compression on transmission,
     * but accept A/C and protocol compressed packets
     * if we are going to ask for A/C and protocol compression.
     */
    ppp_set_xaccm(ppp, ppp->lcp_xmit_accm);
    ppp_send_config(ppp, PPP_MRU, 0xffffffff, 0, 0);
    ppp_recv_config(ppp, PPP_MRU, (ppp->lcp_lax_recv? 0: 0xffffffff),
		    wo->neg_pcompression, wo->neg_accompression);
    ppp->lcp_peer_mru = PPP_MRU;

    ppp->lcp_wantoptions.asyncmap = ppp->lcp_xmit_accm[0];

    fsm_lowerup(&ppp->lcp_fsm);
}


/* -----------------------------------------------------------------------------
* lcp_lowerdown - The lower layer is down.
----------------------------------------------------------------------------- */
void lcp_lowerdown(struct ppp *ppp)
{
    ppp_new_event(ppp, PPP_EVT_LOWERLAYER_DOWN);    
    fsm_lowerdown(&ppp->lcp_fsm);
}

/* -----------------------------------------------------------------------------
* lcp_input - Input LCP packet.
----------------------------------------------------------------------------- */
static void lcp_input(struct ppp *ppp, u_char *p, int len)
{
    fsm *f = &ppp->lcp_fsm;

    fsm_input(f, p, len);
}

/* -----------------------------------------------------------------------------
* lcp_extcode - Handle a LCP-specific code.
----------------------------------------------------------------------------- */
static int lcp_extcode(fsm *f, int code, int id, u_char *inp, int len)
{
    u_char 	*magp;
    struct ppp 	*ppp = f->ppp;

    switch( code ){
    case PROTREJ:
	lcp_rprotrej(f, inp, len);
	break;
    
    case ECHOREQ:
	if (f->state != OPENED)
	    break;
	magp = inp;
	PUTLONG(ppp->lcp_gotoptions.magicnumber, magp);
	fsm_sdata(f, ECHOREP, id, inp, len);
	break;
    
    case ECHOREP:
	lcp_received_echo_reply(f, id, inp, len);
	break;

    case DISCREQ:
	break;

    default:
	return 0;
    }
    return 1;
}

/* -----------------------------------------------------------------------------
* lcp_rprotrej - Receive an Protocol-Reject.
*
* Figure out which protocol is rejected and inform it.
----------------------------------------------------------------------------- */
static void lcp_rprotrej(fsm *f, u_char *inp, int len)
{
    int 		i;
    struct protent 	*protp;
    u_short 		prot;
    struct ppp 		*ppp = f->ppp;

    if (len < 2) {
	LCPDEBUG(("lcp_rprotrej: Rcvd short Protocol-Reject packet!"));
	return;
    }

    GETSHORT(prot, inp);

    /*
     * Protocol-Reject packets received in any state other than the LCP
     * OPENED state SHOULD be silently discarded.
     */
    if( f->state != OPENED ){
	LCPDEBUG(("Protocol-Reject discarded: LCP in state %d", f->state));
	return;
    }

    /*
     * Upcall the proper Protocol-Reject routine.
     */
    for (i = 0; (protp = ppp->protocols[i]) != NULL; ++i)
	if (protp->protocol == prot) {
	    (*protp->protrej)(ppp);
	    return;
	}

    warn(ppp, "Protocol-Reject for unsupported protocol 0x%x", prot);
}

/* -----------------------------------------------------------------------------
* lcp_protrej - A Protocol-Reject was received.
----------------------------------------------------------------------------- */
static void lcp_protrej(struct ppp *ppp)
{
    /*
     * Can't reject LCP!
     */
    error(ppp, "Received Protocol-Reject for LCP!");
    fsm_protreject(&ppp->lcp_fsm);
}

/* -----------------------------------------------------------------------------
* lcp_sprotrej - Send a Protocol-Reject for some protocol.
----------------------------------------------------------------------------- */
void lcp_sprotrej(struct ppp *ppp, u_char *p, int len)
{
    /*
     * Send back the protocol and the information field of the
     * rejected packet.  We only get here if LCP is in the OPENED state.
     */
    p += 2;
    len -= 2;

    fsm_sdata(&ppp->lcp_fsm, PROTREJ, ++ppp->lcp_fsm.id, p, len);
}

/* -----------------------------------------------------------------------------
* lcp_resetci - Reset our CI.
----------------------------------------------------------------------------- */
static void lcp_resetci(fsm *f)
{
    struct ppp 		*ppp = f->ppp;
   
    ppp->lcp_wantoptions.magicnumber = random();
    ppp->lcp_wantoptions.numloops = 0;

    ppp->lcp_gotoptions = ppp->lcp_wantoptions;
    ppp->lcp_peer_mru = PPP_MRU;
    auth_reset(ppp);
}

/* -----------------------------------------------------------------------------
* lcp_cilen - Return length of our CI.
----------------------------------------------------------------------------- */
static int lcp_cilen(fsm *f)
{
    struct ppp 		*ppp = f->ppp;
    lcp_options 	*go = &ppp->lcp_gotoptions;

#define LENCIVOID(neg)	((neg) ? CILEN_VOID : 0)
#define LENCICHAP(neg)	((neg) ? CILEN_CHAP : 0)
#define LENCISHORT(neg)	((neg) ? CILEN_SHORT : 0)
#define LENCILONG(neg)	((neg) ? CILEN_LONG : 0)
#define LENCILQR(neg)	((neg) ? CILEN_LQR: 0)
#define LENCICBCP(neg)	((neg) ? CILEN_CBCP: 0)
    /*
     * NB: we only ask for one of CHAP and UPAP, even if we will
     * accept either.
     */
    return (LENCISHORT(go->neg_mru && go->mru != DEFMRU) +
	    LENCILONG(go->neg_asyncmap && go->asyncmap != 0xFFFFFFFF) +
	    LENCICHAP(go->neg_chap) +
	    LENCISHORT(!go->neg_chap && go->neg_upap) +
	    LENCILQR(go->neg_lqr) +
	    LENCICBCP(go->neg_cbcp) +
	    LENCILONG(go->neg_magicnumber) +
	    LENCIVOID(go->neg_pcompression) +
	    LENCIVOID(go->neg_accompression));
}

/* -----------------------------------------------------------------------------
* lcp_addci - Add our desired CIs to a packet.
----------------------------------------------------------------------------- */
static void lcp_addci(fsm *f, u_char *ucp, int *lenp)
{
    struct ppp 		*ppp = f->ppp;
    lcp_options 	*go = &ppp->lcp_gotoptions;
    u_char 		*start_ucp = ucp;

#define ADDCIVOID(opt, neg) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(CILEN_VOID, ucp); \
    }
#define ADDCISHORT(opt, neg, val) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(CILEN_SHORT, ucp); \
	PUTSHORT(val, ucp); \
    }
#define ADDCICHAP(opt, neg, val, digest) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(CILEN_CHAP, ucp); \
	PUTSHORT(val, ucp); \
	PUTCHAR(digest, ucp); \
    }
#define ADDCILONG(opt, neg, val) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(CILEN_LONG, ucp); \
	PUTLONG(val, ucp); \
    }
#define ADDCILQR(opt, neg, val) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(CILEN_LQR, ucp); \
	PUTSHORT(PPP_LQR, ucp); \
	PUTLONG(val, ucp); \
    }
#define ADDCICHAR(opt, neg, val) \
    if (neg) { \
	PUTCHAR(opt, ucp); \
	PUTCHAR(CILEN_CHAR, ucp); \
	PUTCHAR(val, ucp); \
    }

    ADDCISHORT(CI_MRU, go->neg_mru && go->mru != DEFMRU, go->mru);
    ADDCILONG(CI_ASYNCMAP, go->neg_asyncmap && go->asyncmap != 0xFFFFFFFF,
	      go->asyncmap);
    ADDCICHAP(CI_AUTHTYPE, go->neg_chap, PPP_CHAP, go->chap_mdtype);
    ADDCISHORT(CI_AUTHTYPE, !go->neg_chap && go->neg_upap, PPP_PAP);
    ADDCILQR(CI_QUALITY, go->neg_lqr, go->lqr_period);
    ADDCICHAR(CI_CALLBACK, go->neg_cbcp, CBCP_OPT);
    ADDCILONG(CI_MAGICNUMBER, go->neg_magicnumber, go->magicnumber);
    ADDCIVOID(CI_PCOMPRESSION, go->neg_pcompression);
    ADDCIVOID(CI_ACCOMPRESSION, go->neg_accompression);

    if (ucp - start_ucp != *lenp) {
	/* this should never happen, because peer_mtu should be 1500 */
	error(ppp, "Bug in lcp_addci: wrong length");
    }
}

/* -----------------------------------------------------------------------------
* lcp_ackci - Ack our CIs.
* This should not modify any state if the Ack is bad.
*
* Returns:
*	0 - Ack was bad.
*	1 - Ack was good.
----------------------------------------------------------------------------- */
static int lcp_ackci(fsm *f, u_char *p, int len)
{
    struct ppp 		*ppp = f->ppp;
    lcp_options 	*go = &ppp->lcp_gotoptions;
    u_char cilen, citype, cichar;
    u_short cishort;
    u_int32_t cilong;

    /*
     * CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */
#define ACKCIVOID(opt, neg) \
    if (neg) { \
	if ((len -= CILEN_VOID) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_VOID || \
	    citype != opt) \
	    goto bad; \
    }
#define ACKCISHORT(opt, neg, val) \
    if (neg) { \
	if ((len -= CILEN_SHORT) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_SHORT || \
	    citype != opt) \
	    goto bad; \
	GETSHORT(cishort, p); \
	if (cishort != val) \
	    goto bad; \
    }
#define ACKCICHAR(opt, neg, val) \
    if (neg) { \
	if ((len -= CILEN_CHAR) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_CHAR || \
	    citype != opt) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (cichar != val) \
	    goto bad; \
    }
#define ACKCICHAP(opt, neg, val, digest) \
    if (neg) { \
	if ((len -= CILEN_CHAP) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_CHAP || \
	    citype != opt) \
	    goto bad; \
	GETSHORT(cishort, p); \
	if (cishort != val) \
	    goto bad; \
	GETCHAR(cichar, p); \
	if (cichar != digest) \
	  goto bad; \
    }
#define ACKCILONG(opt, neg, val) \
    if (neg) { \
	if ((len -= CILEN_LONG) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_LONG || \
	    citype != opt) \
	    goto bad; \
	GETLONG(cilong, p); \
	if (cilong != val) \
	    goto bad; \
    }
#define ACKCILQR(opt, neg, val) \
    if (neg) { \
	if ((len -= CILEN_LQR) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_LQR || \
	    citype != opt) \
	    goto bad; \
	GETSHORT(cishort, p); \
	if (cishort != PPP_LQR) \
	    goto bad; \
	GETLONG(cilong, p); \
	if (cilong != val) \
	  goto bad; \
    }

    ACKCISHORT(CI_MRU, go->neg_mru && go->mru != DEFMRU, go->mru);
    ACKCILONG(CI_ASYNCMAP, go->neg_asyncmap && go->asyncmap != 0xFFFFFFFF,
	      go->asyncmap);
    ACKCICHAP(CI_AUTHTYPE, go->neg_chap, PPP_CHAP, go->chap_mdtype);
    ACKCISHORT(CI_AUTHTYPE, !go->neg_chap && go->neg_upap, PPP_PAP);
    ACKCILQR(CI_QUALITY, go->neg_lqr, go->lqr_period);
    ACKCICHAR(CI_CALLBACK, go->neg_cbcp, CBCP_OPT);
    ACKCILONG(CI_MAGICNUMBER, go->neg_magicnumber, go->magicnumber);
    ACKCIVOID(CI_PCOMPRESSION, go->neg_pcompression);
    ACKCIVOID(CI_ACCOMPRESSION, go->neg_accompression);

    /*
     * If there are any remaining CIs, then this packet is bad.
     */
    if (len != 0)
	goto bad;
    return (1);
bad:
    LCPDEBUG(("lcp_acki: received bad Ack!"));
    return (0);
}

/* -----------------------------------------------------------------------------
* lcp_nakci - Peer has sent a NAK for some of our CIs.
* This should not modify any state if the Nak is bad
* or if LCP is in the OPENED state.
*
* Returns:
*	0 - Nak was bad.
*	1 - Nak was good.
----------------------------------------------------------------------------- */
static int lcp_nakci(fsm *f, u_char *p, int len)
{
    struct ppp 		*ppp = f->ppp;
    lcp_options 	*go = &ppp->lcp_gotoptions;
    lcp_options 	*wo = &ppp->lcp_wantoptions;
    u_char 		citype, cichar, *next;
    u_short 		cishort;
    u_int32_t 		cilong;
    lcp_options 	no;		/* options we've seen Naks for */
    lcp_options 	try;		/* options to request next time */
    int 		looped_back = 0;
    int 		cilen;

    BZERO(&no, sizeof(no));
    try = *go;

    /*
     * Any Nak'd CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */
#define NAKCIVOID(opt, neg, code) \
    if (go->neg && \
	len >= CILEN_VOID && \
	p[1] == CILEN_VOID && \
	p[0] == opt) { \
	len -= CILEN_VOID; \
	INCPTR(CILEN_VOID, p); \
	no.neg = 1; \
	code \
    }
#define NAKCICHAP(opt, neg, code) \
    if (go->neg && \
	len >= CILEN_CHAP && \
	p[1] == CILEN_CHAP && \
	p[0] == opt) { \
	len -= CILEN_CHAP; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	GETCHAR(cichar, p); \
	no.neg = 1; \
	code \
    }
#define NAKCICHAR(opt, neg, code) \
    if (go->neg && \
	len >= CILEN_CHAR && \
	p[1] == CILEN_CHAR && \
	p[0] == opt) { \
	len -= CILEN_CHAR; \
	INCPTR(2, p); \
	GETCHAR(cichar, p); \
	no.neg = 1; \
	code \
    }
#define NAKCISHORT(opt, neg, code) \
    if (go->neg && \
	len >= CILEN_SHORT && \
	p[1] == CILEN_SHORT && \
	p[0] == opt) { \
	len -= CILEN_SHORT; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	no.neg = 1; \
	code \
    }
#define NAKCILONG(opt, neg, code) \
    if (go->neg && \
	len >= CILEN_LONG && \
	p[1] == CILEN_LONG && \
	p[0] == opt) { \
	len -= CILEN_LONG; \
	INCPTR(2, p); \
	GETLONG(cilong, p); \
	no.neg = 1; \
	code \
    }
#define NAKCILQR(opt, neg, code) \
    if (go->neg && \
	len >= CILEN_LQR && \
	p[1] == CILEN_LQR && \
	p[0] == opt) { \
	len -= CILEN_LQR; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	GETLONG(cilong, p); \
	no.neg = 1; \
	code \
    }

    /*
     * We don't care if they want to send us smaller packets than
     * we want.  Therefore, accept any MRU less than what we asked for,
     * but then ignore the new value when setting the MRU in the kernel.
     * If they send us a bigger MRU than what we asked, accept it, up to
     * the limit of the default MRU we'd get if we didn't negotiate.
     */
    if (go->neg_mru && go->mru != DEFMRU) {
	NAKCISHORT(CI_MRU, neg_mru,
		   if (cishort <= wo->mru || cishort <= DEFMRU)
		       try.mru = cishort;
		   );
    }

    /*
     * Add any characters they want to our (receive-side) asyncmap.
     */
    if (go->neg_asyncmap && go->asyncmap != 0xFFFFFFFF) {
	NAKCILONG(CI_ASYNCMAP, neg_asyncmap,
		  try.asyncmap = go->asyncmap | cilong;
		  );
    }

    /*
     * If they've nak'd our authentication-protocol, check whether
     * they are proposing a different protocol, or a different
     * hash algorithm for CHAP.
     */
    if ((go->neg_chap || go->neg_upap)
	&& len >= CILEN_SHORT
	&& p[0] == CI_AUTHTYPE && p[1] >= CILEN_SHORT && p[1] <= len) {
	cilen = p[1];
	len -= cilen;
	no.neg_chap = go->neg_chap;
	no.neg_upap = go->neg_upap;
	INCPTR(2, p);
        GETSHORT(cishort, p);
	if (cishort == PPP_PAP && cilen == CILEN_SHORT) {
	    /*
	     * If we were asking for CHAP, they obviously don't want to do it.
	     * If we weren't asking for CHAP, then we were asking for PAP,
	     * in which case this Nak is bad.
	     */
	    if (!go->neg_chap)
		goto bad;
	    try.neg_chap = 0;

	} else if (cishort == PPP_CHAP && cilen == CILEN_CHAP) {
	    GETCHAR(cichar, p);
	    if (go->neg_chap) {
		/*
		 * We were asking for CHAP/MD5; they must want a different
		 * algorithm.  If they can't do MD5, we can ask for M$-CHAP
		 * if we support it, otherwise we'll have to stop
		 * asking for CHAP.
		 */
		if (cichar != go->chap_mdtype) {
		    if (cichar == CHAP_MICROSOFT)
			go->chap_mdtype = CHAP_MICROSOFT;
		    else
			try.neg_chap = 0;
		}
	    } else {
		/*
		 * Stop asking for PAP if we were asking for it.
		 */
		try.neg_upap = 0;
	    }

	} else {
	    /*
	     * We don't recognize what they're suggesting.
	     * Stop asking for what we were asking for.
	     */
	    if (go->neg_chap)
		try.neg_chap = 0;
	    else
		try.neg_upap = 0;
	    p += cilen - CILEN_SHORT;
	}
    }

    /*
     * If they can't cope with our link quality protocol, we'll have
     * to stop asking for LQR.  We haven't got any other protocol.
     * If they Nak the reporting period, take their value XXX ?
     */
    NAKCILQR(CI_QUALITY, neg_lqr,
	     if (cishort != PPP_LQR)
		 try.neg_lqr = 0;
	     else
		 try.lqr_period = cilong;
	     );

    /*
     * Only implementing CBCP...not the rest of the callback options
     */
    NAKCICHAR(CI_CALLBACK, neg_cbcp,
              try.neg_cbcp = 0;
              );

    /*
     * Check for a looped-back line.
     */
    NAKCILONG(CI_MAGICNUMBER, neg_magicnumber,
	      try.magicnumber = random();
	      looped_back = 1;
	      );

    /*
     * Peer shouldn't send Nak for protocol compression or
     * address/control compression requests; they should send
     * a Reject instead.  If they send a Nak, treat it as a Reject.
     */
    NAKCIVOID(CI_PCOMPRESSION, neg_pcompression,
	      try.neg_pcompression = 0;
	      );
    NAKCIVOID(CI_ACCOMPRESSION, neg_accompression,
	      try.neg_accompression = 0;
	      );

    /*
     * There may be remaining CIs, if the peer is requesting negotiation
     * on an option that we didn't include in our request packet.
     * If we see an option that we requested, or one we've already seen
     * in this packet, then this packet is bad.
     * If we wanted to respond by starting to negotiate on the requested
     * option(s), we could, but we don't, because except for the
     * authentication type and quality protocol, if we are not negotiating
     * an option, it is because we were told not to.
     * For the authentication type, the Nak from the peer means
     * `let me authenticate myself with you' which is a bit pointless.
     * For the quality protocol, the Nak means `ask me to send you quality
     * reports', but if we didn't ask for them, we don't want them.
     * An option we don't recognize represents the peer asking to
     * negotiate some option we don't support, so ignore it.
     */
    while (len > CILEN_VOID) {
	GETCHAR(citype, p);
	GETCHAR(cilen, p);
	if (cilen < CILEN_VOID || (len -= cilen) < 0)
	    goto bad;
	next = p + cilen - 2;

	switch (citype) {
	case CI_MRU:
	    if ((go->neg_mru && go->mru != DEFMRU)
		|| no.neg_mru || cilen != CILEN_SHORT)
		goto bad;
	    GETSHORT(cishort, p);
	    if (cishort < DEFMRU)
		try.mru = cishort;
	    break;
	case CI_ASYNCMAP:
	    if ((go->neg_asyncmap && go->asyncmap != 0xFFFFFFFF)
		|| no.neg_asyncmap || cilen != CILEN_LONG)
		goto bad;
	    break;
	case CI_AUTHTYPE:
	    if (go->neg_chap || no.neg_chap || go->neg_upap || no.neg_upap)
		goto bad;
	    break;
	case CI_MAGICNUMBER:
	    if (go->neg_magicnumber || no.neg_magicnumber ||
		cilen != CILEN_LONG)
		goto bad;
	    break;
	case CI_PCOMPRESSION:
	    if (go->neg_pcompression || no.neg_pcompression
		|| cilen != CILEN_VOID)
		goto bad;
	    break;
	case CI_ACCOMPRESSION:
	    if (go->neg_accompression || no.neg_accompression
		|| cilen != CILEN_VOID)
		goto bad;
	    break;
	case CI_QUALITY:
	    if (go->neg_lqr || no.neg_lqr || cilen != CILEN_LQR)
		goto bad;
	    break;
	}
	p = next;
    }

    /*
     * OK, the Nak is good.  Now we can update state.
     * If there are any options left we ignore them.
     */
    if (f->state != OPENED) {
	if (looped_back) {
	    if (++try.numloops >= ppp->lcp_loopbackfail) {
		notice(ppp, "Serial line is looped back.");
		lcp_close(ppp, "Loopback detected");
                ppp->status = PPP_ERR_LOOPBACK;
                //ppp->status = EXIT_LOOPBACK;
	    }
	} else
	    try.numloops = 0;
	*go = try;
    }

    return 1;

bad:
    LCPDEBUG(("lcp_nakci: received bad Nak!"));
    return 0;
}

/* -----------------------------------------------------------------------------
* lcp_rejci - Peer has Rejected some of our CIs.
* This should not modify any state if the Reject is bad
* or if LCP is in the OPENED state.
*
* Returns:
*	0 - Reject was bad.
*	1 - Reject was good.
----------------------------------------------------------------------------- */
static int lcp_rejci(fsm *f, u_char *p, int len)
{
    struct ppp 		*ppp = f->ppp;
    lcp_options 	*go = &ppp->lcp_gotoptions;
    u_char 		cichar;
    u_short 		cishort;
    u_int32_t 		cilong;
    lcp_options 	try;		/* options to request next time */

    try = *go;

    /*
     * Any Rejected CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */
#define REJCIVOID(opt, neg) \
    if (go->neg && \
	len >= CILEN_VOID && \
	p[1] == CILEN_VOID && \
	p[0] == opt) { \
	len -= CILEN_VOID; \
	INCPTR(CILEN_VOID, p); \
	try.neg = 0; \
    }
#define REJCISHORT(opt, neg, val) \
    if (go->neg && \
	len >= CILEN_SHORT && \
	p[1] == CILEN_SHORT && \
	p[0] == opt) { \
	len -= CILEN_SHORT; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	/* Check rejected value. */ \
	if (cishort != val) \
	    goto bad; \
	try.neg = 0; \
    }
#define REJCICHAP(opt, neg, val, digest) \
    if (go->neg && \
	len >= CILEN_CHAP && \
	p[1] == CILEN_CHAP && \
	p[0] == opt) { \
	len -= CILEN_CHAP; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	GETCHAR(cichar, p); \
	/* Check rejected value. */ \
	if (cishort != val || cichar != digest) \
	    goto bad; \
	try.neg = 0; \
	try.neg_upap = 0; \
    }
#define REJCILONG(opt, neg, val) \
    if (go->neg && \
	len >= CILEN_LONG && \
	p[1] == CILEN_LONG && \
	p[0] == opt) { \
	len -= CILEN_LONG; \
	INCPTR(2, p); \
	GETLONG(cilong, p); \
	/* Check rejected value. */ \
	if (cilong != val) \
	    goto bad; \
	try.neg = 0; \
    }
#define REJCILQR(opt, neg, val) \
    if (go->neg && \
	len >= CILEN_LQR && \
	p[1] == CILEN_LQR && \
	p[0] == opt) { \
	len -= CILEN_LQR; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	GETLONG(cilong, p); \
	/* Check rejected value. */ \
	if (cishort != PPP_LQR || cilong != val) \
	    goto bad; \
	try.neg = 0; \
    }
#define REJCICBCP(opt, neg, val) \
    if (go->neg && \
	len >= CILEN_CBCP && \
	p[1] == CILEN_CBCP && \
	p[0] == opt) { \
	len -= CILEN_CBCP; \
	INCPTR(2, p); \
	GETCHAR(cichar, p); \
	/* Check rejected value. */ \
	if (cichar != val) \
	    goto bad; \
	try.neg = 0; \
    }

    REJCISHORT(CI_MRU, neg_mru, go->mru);
    REJCILONG(CI_ASYNCMAP, neg_asyncmap, go->asyncmap);
    REJCICHAP(CI_AUTHTYPE, neg_chap, PPP_CHAP, go->chap_mdtype);
    if (!go->neg_chap) {
	REJCISHORT(CI_AUTHTYPE, neg_upap, PPP_PAP);
    }
    REJCILQR(CI_QUALITY, neg_lqr, go->lqr_period);
    REJCICBCP(CI_CALLBACK, neg_cbcp, CBCP_OPT);
    REJCILONG(CI_MAGICNUMBER, neg_magicnumber, go->magicnumber);
    REJCIVOID(CI_PCOMPRESSION, neg_pcompression);
    REJCIVOID(CI_ACCOMPRESSION, neg_accompression);

    /*
     * If there are any remaining CIs, then this packet is bad.
     */
    if (len != 0)
	goto bad;
    /*
     * Now we can update state.
     */
    if (f->state != OPENED)
	*go = try;
    return 1;

bad:
    LCPDEBUG(("lcp_rejci: received bad Reject!"));
    return 0;
}

/* -----------------------------------------------------------------------------
* lcp_reqci - Check the peer's requested CIs and send appropriate response.
*
* Returns: CONFACK, CONFNAK or CONFREJ and input packet modified
* appropriately.  If reject_if_disagree is non-zero, doesn't return
* CONFNAK; returns CONFREJ if it can't return CONFACK.
----------------------------------------------------------------------------- */
static int lcp_reqci(fsm *f, u_char *inp, int *lenp, int reject_if_disagree)
{
    struct ppp 		*ppp = f->ppp;
    lcp_options 	*go = &ppp->lcp_gotoptions;
    lcp_options 	*ho = &ppp->lcp_hisoptions;
    lcp_options 	*ao = &ppp->lcp_allowoptions;
    u_char 		*cip, *next;		/* Pointer to current and next CIs */
    int 		cilen, citype, cichar;	/* Parsed len, type, char value */
    u_short 		cishort;		/* Parsed short value */
    u_int32_t 		cilong;		/* Parse long value */
    int 		rc = CONFACK;		/* Final packet return code */
    int 		orc;			/* Individual option return code */
    u_char 		*p;			/* Pointer to next char to parse */
    u_char 		*rejp;		/* Pointer to next char in reject frame */
    u_char 		*nakp;		/* Pointer to next char in Nak frame */
    int 		l = *lenp;		/* Length left */

    /*
     * Reset all his options.
     */
    BZERO(ho, sizeof(*ho));

    /*
     * Process all his options.
     */
    next = inp;
    nakp = ppp->lcp_nak_buffer;
    rejp = inp;
    while (l) {
	orc = CONFACK;			/* Assume success */
	cip = p = next;			/* Remember begining of CI */
	if (l < 2 ||			/* Not enough data for CI header or */
	    p[1] < 2 ||			/*  CI length too small or */
	    p[1] > l) {			/*  CI length too big? */
	    LCPDEBUG(("lcp_reqci: bad CI length!"));
	    orc = CONFREJ;		/* Reject bad CI */
	    cilen = l;			/* Reject till end of packet */
	    l = 0;			/* Don't loop again */
	    citype = 0;
	    goto endswitch;
	}
	GETCHAR(citype, p);		/* Parse CI type */
	GETCHAR(cilen, p);		/* Parse CI length */
	l -= cilen;			/* Adjust remaining length */
	next += cilen;			/* Step to next CI */

	switch (citype) {		/* Check CI type */
	case CI_MRU:
	    if (!ao->neg_mru ||		/* Allow option? */
		cilen != CILEN_SHORT) {	/* Check CI length */
		orc = CONFREJ;		/* Reject CI */
		break;
	    }
	    GETSHORT(cishort, p);	/* Parse MRU */

	    /*
	     * He must be able to receive at least our minimum.
	     * No need to check a maximum.  If he sends a large number,
	     * we'll just ignore it.
	     */
	    if (cishort < MINMRU) {
		orc = CONFNAK;		/* Nak CI */
		PUTCHAR(CI_MRU, nakp);
		PUTCHAR(CILEN_SHORT, nakp);
		PUTSHORT(MINMRU, nakp);	/* Give him a hint */
		break;
	    }
	    ho->neg_mru = 1;		/* Remember he sent MRU */
	    ho->mru = cishort;		/* And remember value */
	    break;

	case CI_ASYNCMAP:
	    if (!ao->neg_asyncmap ||
		cilen != CILEN_LONG) {
		orc = CONFREJ;
		break;
	    }
	    GETLONG(cilong, p);

	    /*
	     * Asyncmap must have set at least the bits
	     * which are set in lcp_allowoptions[unit].asyncmap.
	     */
	    if ((ao->asyncmap & ~cilong) != 0) {
		orc = CONFNAK;
		PUTCHAR(CI_ASYNCMAP, nakp);
		PUTCHAR(CILEN_LONG, nakp);
		PUTLONG(ao->asyncmap | cilong, nakp);
		break;
	    }
	    ho->neg_asyncmap = 1;
	    ho->asyncmap = cilong;
	    break;

	case CI_AUTHTYPE:
	    if (cilen < CILEN_SHORT ||
		!(ao->neg_upap || ao->neg_chap)) {
		/*
		 * Reject the option if we're not willing to authenticate.
		 */
		orc = CONFREJ;
		break;
	    }
	    GETSHORT(cishort, p);

	    /*
	     * Authtype must be PAP or CHAP.
	     *
	     * Note: if both ao->neg_upap and ao->neg_chap are set,
	     * and the peer sends a Configure-Request with two
	     * authenticate-protocol requests, one for CHAP and one
	     * for UPAP, then we will reject the second request.
	     * Whether we end up doing CHAP or UPAP depends then on
	     * the ordering of the CIs in the peer's Configure-Request.
	     */

	    if (cishort == PPP_PAP) {
		if (ho->neg_chap ||	/* we've already accepted CHAP */
		    cilen != CILEN_SHORT) {
		    LCPDEBUG(("lcp_reqci: rcvd AUTHTYPE PAP, rejecting..."));
		    orc = CONFREJ;
		    break;
		}
		if (!ao->neg_upap) {	/* we don't want to do PAP */
		    orc = CONFNAK;	/* NAK it and suggest CHAP */
		    PUTCHAR(CI_AUTHTYPE, nakp);
		    PUTCHAR(CILEN_CHAP, nakp);
		    PUTSHORT(PPP_CHAP, nakp);
		    PUTCHAR(ao->chap_mdtype, nakp);
		    /* XXX if we can do CHAP_MICROSOFT as well, we should
		       probably put in another option saying so */
		    break;
		}
		ho->neg_upap = 1;
		break;
	    }
	    if (cishort == PPP_CHAP) {
		if (ho->neg_upap ||	/* we've already accepted PAP */
		    cilen != CILEN_CHAP) {
		    LCPDEBUG(("lcp_reqci: rcvd AUTHTYPE CHAP, rejecting..."));
		    orc = CONFREJ;
		    break;
		}
		if (!ao->neg_chap) {	/* we don't want to do CHAP */
		    orc = CONFNAK;	/* NAK it and suggest PAP */
		    PUTCHAR(CI_AUTHTYPE, nakp);
		    PUTCHAR(CILEN_SHORT, nakp);
		    PUTSHORT(PPP_PAP, nakp);
		    break;
		}
		GETCHAR(cichar, p);	/* get digest type*/
		if (cichar != CHAP_DIGEST_MD5
		    && cichar != CHAP_MICROSOFT
		    ) {
		    orc = CONFNAK;
		    PUTCHAR(CI_AUTHTYPE, nakp);
		    PUTCHAR(CILEN_CHAP, nakp);
		    PUTSHORT(PPP_CHAP, nakp);
		    PUTCHAR(ao->chap_mdtype, nakp);
		    break;
		}
		ho->chap_mdtype = cichar; /* save md type */
		ho->neg_chap = 1;
		break;
	    }

	    /*
	     * We don't recognize the protocol they're asking for.
	     * Nak it with something we're willing to do.
	     * (At this point we know ao->neg_upap || ao->neg_chap.)
	     */
	    orc = CONFNAK;
	    PUTCHAR(CI_AUTHTYPE, nakp);
	    if (ao->neg_chap) {
		PUTCHAR(CILEN_CHAP, nakp);
		PUTSHORT(PPP_CHAP, nakp);
		PUTCHAR(ao->chap_mdtype, nakp);
	    } else {
		PUTCHAR(CILEN_SHORT, nakp);
		PUTSHORT(PPP_PAP, nakp);
	    }
	    break;

	case CI_QUALITY:
	    if (!ao->neg_lqr ||
		cilen != CILEN_LQR) {
		orc = CONFREJ;
		break;
	    }

	    GETSHORT(cishort, p);
	    GETLONG(cilong, p);

	    /*
	     * Check the protocol and the reporting period.
	     * XXX When should we Nak this, and what with?
	     */
	    if (cishort != PPP_LQR) {
		orc = CONFNAK;
		PUTCHAR(CI_QUALITY, nakp);
		PUTCHAR(CILEN_LQR, nakp);
		PUTSHORT(PPP_LQR, nakp);
		PUTLONG(ao->lqr_period, nakp);
		break;
	    }
	    break;

	case CI_MAGICNUMBER:
	    if (!(ao->neg_magicnumber || go->neg_magicnumber) ||
		cilen != CILEN_LONG) {
		orc = CONFREJ;
		break;
	    }
	    GETLONG(cilong, p);

	    /*
	     * He must have a different magic number.
	     */
	    if (go->neg_magicnumber &&
		cilong == go->magicnumber) {
		cilong = random();	/* Don't put random() inside macro! */
		orc = CONFNAK;
		PUTCHAR(CI_MAGICNUMBER, nakp);
		PUTCHAR(CILEN_LONG, nakp);
		PUTLONG(cilong, nakp);
		break;
	    }
	    ho->neg_magicnumber = 1;
	    ho->magicnumber = cilong;
	    break;


	case CI_PCOMPRESSION:
	    if (!ao->neg_pcompression ||
		cilen != CILEN_VOID) {
		orc = CONFREJ;
		break;
	    }
	    ho->neg_pcompression = 1;
	    break;

	case CI_ACCOMPRESSION:
	    if (!ao->neg_accompression ||
		cilen != CILEN_VOID) {
		orc = CONFREJ;
		break;
	    }
	    ho->neg_accompression = 1;
	    break;

	default:
	    LCPDEBUG(("lcp_reqci: rcvd unknown option %d", citype));
	    orc = CONFREJ;
	    break;
	}

endswitch:
	if (orc == CONFACK &&		/* Good CI */
	    rc != CONFACK)		/*  but prior CI wasnt? */
	    continue;			/* Don't send this one */

	if (orc == CONFNAK) {		/* Nak this CI? */
	    if (reject_if_disagree	/* Getting fed up with sending NAKs? */
		&& citype != CI_MAGICNUMBER) {
		orc = CONFREJ;		/* Get tough if so */
	    } else {
		if (rc == CONFREJ)	/* Rejecting prior CI? */
		    continue;		/* Don't send this one */
		rc = CONFNAK;
	    }
	}
	if (orc == CONFREJ) {		/* Reject this CI */
	    rc = CONFREJ;
	    if (cip != rejp)		/* Need to move rejected CI? */
		BCOPY(cip, rejp, cilen); /* Move it */
	    INCPTR(cilen, rejp);	/* Update output pointer */
	}
    }

    /*
     * If we wanted to send additional NAKs (for unsent CIs), the
     * code would go here.  The extra NAKs would go at *nakp.
     * At present there are no cases where we want to ask the
     * peer to negotiate an option.
     */

    switch (rc) {
    case CONFACK:
	*lenp = next - inp;
	break;
    case CONFNAK:
	/*
	 * Copy the Nak'd options from the nak_buffer to the caller's buffer.
	 */
        *lenp = nakp - ppp->lcp_nak_buffer;
        BCOPY(ppp->lcp_nak_buffer, inp, *lenp);
	break;
    case CONFREJ:
	*lenp = rejp - inp;
	break;
    }

    LCPDEBUG(("lcp_reqci: returning CONF%s.", CODENAME(rc)));
    return (rc);			/* Return final code */
}

/* -----------------------------------------------------------------------------
* lcp_up - LCP has come UP.
----------------------------------------------------------------------------- */
static void lcp_up(fsm *f)
{
    struct ppp 		*ppp = f->ppp;
    lcp_options 	*wo = &ppp->lcp_wantoptions;
    lcp_options 	*ho = &ppp->lcp_hisoptions;
    lcp_options 	*go = &ppp->lcp_gotoptions;
    lcp_options 	*ao = &ppp->lcp_allowoptions;

    ppp_new_event(ppp, PPP_EVT_LCP_UP);

    if (!go->neg_magicnumber)
	go->magicnumber = 0;
    if (!ho->neg_magicnumber)
	ho->magicnumber = 0;

    /*
     * Set our MTU to the smaller of the MTU we wanted and
     * the MRU our peer wanted.  If we negotiated an MRU,
     * set our MRU to the larger of value we wanted and
     * the value we got in the negotiation.
     */

    ppp->lcp_mtu = MIN(ao->mru, (ho->neg_mru? ho->mru: PPP_MRU));
    ppp->lcp_txasyncmap = ho->neg_asyncmap? ho->asyncmap: 0xffffffff;
    ppp->lcp_txpcomp = ho->neg_pcompression;
    ppp->lcp_txaccomp = ho->neg_accompression;

    ppp->lcp_mru = go->neg_mru ? MAX(wo->mru, go->mru): PPP_MRU;
    ppp->lcp_rcasyncmap = ppp->lcp_lax_recv? 0: go->neg_asyncmap? go->asyncmap: 0xffffffff;
    ppp->lcp_rcpcomp = go->neg_pcompression;
    ppp->lcp_rcaccomp = go->neg_accompression;

    ppp_send_config(ppp, ppp->lcp_mtu, ppp->lcp_txasyncmap, ppp->lcp_txpcomp, ppp->lcp_txaccomp);
    ppp_recv_config(ppp, ppp->lcp_mru, ppp->lcp_rcasyncmap, ppp->lcp_rcpcomp, ppp->lcp_rcaccomp);

    if (ho->neg_mru)
        ppp->lcp_peer_mru = ho->mru;

    lcp_echo_lowerup(ppp);  /* Enable echo messages */

    auth_link_established(ppp);
}

/* -----------------------------------------------------------------------------
* lcp_down - LCP has gone DOWN.
* Alert other protocols.
----------------------------------------------------------------------------- */
static void lcp_down(fsm *f)
{
    struct ppp 		*ppp = f->ppp;
    lcp_options 	*go = &ppp->lcp_gotoptions;

   ppp_new_event(ppp, PPP_EVT_LCP_DOWN);

    lcp_echo_lowerdown(ppp);

    auth_link_down(ppp);

    ppp_send_config(ppp, PPP_MRU, 0xffffffff, 0, 0);
    ppp_recv_config(ppp, PPP_MRU,
		    (go->neg_asyncmap? go->asyncmap: 0xffffffff),
		    go->neg_pcompression, go->neg_accompression);
    ppp->lcp_peer_mru = PPP_MRU;
}

/* -----------------------------------------------------------------------------
* lcp_starting - LCP needs the lower layer up.
----------------------------------------------------------------------------- */
static void lcp_starting(fsm *f)
{
    struct ppp 		*ppp = f->ppp;
    auth_link_required(ppp);
}

/* -----------------------------------------------------------------------------
* lcp_finished - LCP has finished with the lower layer.
----------------------------------------------------------------------------- */
static void lcp_finished(fsm *f)
{
    struct ppp 		*ppp = f->ppp;
    auth_link_terminated(ppp);
}

/* -----------------------------------------------------------------------------
* lcp_printpkt - print the contents of an LCP packet.
----------------------------------------------------------------------------- */
static char *lcp_codenames[] = {
    "ConfReq", "ConfAck", "ConfNak", "ConfRej",
    "TermReq", "TermAck", "CodeRej", "ProtRej",
    "EchoReq", "EchoRep", "DiscReq"
};

static int lcp_printpkt(struct ppp *ppp, u_char *p, int plen, void (*printer)(struct ppp *, void *, char *, ...), void *arg)
{
    int code, id, len, olen;
    u_char *pstart, *optend;
    u_short cishort;
    u_int32_t cilong;

    if (plen < HEADERLEN)
	return 0;
    pstart = p;
    GETCHAR(code, p);
    GETCHAR(id, p);
    GETSHORT(len, p);
    if (len < HEADERLEN || len > plen)
	return 0;

    if (code >= 1 && code <= sizeof(lcp_codenames) / sizeof(char *))
        printer(ppp, arg, " %s", lcp_codenames[code-1]);
    else
	printer(ppp, arg, " code=0x%x", code);
    printer(ppp, arg, " id=0x%x", id);
    len -= HEADERLEN;
    switch (code) {
    case CONFREQ:
    case CONFACK:
    case CONFNAK:
    case CONFREJ:
	/* print option list */
	while (len >= 2) {
	    GETCHAR(code, p);
	    GETCHAR(olen, p);
	    p -= 2;
	    if (olen < 2 || olen > len) {
		break;
	    }
            printer(ppp, arg, " <");
	    len -= olen;
	    optend = p + olen;
	    switch (code) {
	    case CI_MRU:
		if (olen == CILEN_SHORT) {
		    p += 2;
		    GETSHORT(cishort, p);
                    printer(ppp, arg, "mru %d", cishort);
		}
		break;
	    case CI_ASYNCMAP:
		if (olen == CILEN_LONG) {
		    p += 2;
		    GETLONG(cilong, p);
                    printer(ppp, arg, "asyncmap 0x%x", cilong);
		}
		break;
	    case CI_AUTHTYPE:
		if (olen >= CILEN_SHORT) {
		    p += 2;
                    printer(ppp, arg, "auth ");
		    GETSHORT(cishort, p);
		    switch (cishort) {
		    case PPP_PAP:
                        printer(ppp, arg, "pap");
			break;
		    case PPP_CHAP:
                        printer(ppp, arg, "chap");
			if (p < optend) {
			    switch (*p) {
			    case CHAP_DIGEST_MD5:
                                printer(ppp, arg, " MD5");
				++p;
				break;
			    case CHAP_MICROSOFT:
                                printer(ppp, arg, " m$oft");
				++p;
				break;
			    }
			}
			break;
		    default:
                        printer(ppp, arg, "0x%x", cishort);
		    }
		}
		break;
	    case CI_QUALITY:
		if (olen >= CILEN_SHORT) {
		    p += 2;
                    printer(ppp, arg, "quality ");
		    GETSHORT(cishort, p);
		    switch (cishort) {
		    case PPP_LQR:
                        printer(ppp, arg, "lqr");
			break;
		    default:
                        printer(ppp, arg, "0x%x", cishort);
		    }
		}
		break;
	    case CI_CALLBACK:
		if (olen >= CILEN_CHAR) {
		    p += 2;
                    printer(ppp, arg, "callback ");
		    GETCHAR(cishort, p);
		    switch (cishort) {
		    case CBCP_OPT:
                        printer(ppp, arg, "CBCP");
			break;
		    default:
                        printer(ppp, arg, "0x%x", cishort);
		    }
		}
		break;
	    case CI_MAGICNUMBER:
		if (olen == CILEN_LONG) {
		    p += 2;
		    GETLONG(cilong, p);
                    printer(ppp, arg, "magic 0x%x", cilong);
		}
		break;
	    case CI_PCOMPRESSION:
		if (olen == CILEN_VOID) {
		    p += 2;
                    printer(ppp, arg, "pcomp");
		}
		break;
	    case CI_ACCOMPRESSION:
		if (olen == CILEN_VOID) {
		    p += 2;
                    printer(ppp, arg, "accomp");
		}
		break;
	    }
	    while (p < optend) {
		GETCHAR(code, p);
                printer(ppp, arg, " %.2x", code);
	    }
            printer(ppp, arg, ">");
	}
	break;

    case TERMACK:
    case TERMREQ:
	if (len > 0 && *p >= ' ' && *p < 0x7f) {
            printer(ppp, arg, " ");
            print_string(ppp, p, len, printer, arg);
	    p += len;
	    len = 0;
	}
	break;

    case ECHOREQ:
    case ECHOREP:
    case DISCREQ:
	if (len >= 4) {
	    GETLONG(cilong, p);
            printer(ppp, arg, " magic=0x%x", cilong);
	    p += 4;
	    len -= 4;
	}
	break;
    }

    /* print the rest of the bytes in the packet */
    for (; len > 0; --len) {
	GETCHAR(code, p);
        printer(ppp, arg, " %.2x", code);
    }

    return p - pstart;
}

/* -----------------------------------------------------------------------------
* Time to shut down the link because there is nothing out there.
----------------------------------------------------------------------------- */
static void LcpLinkFailure (fsm *f)
{
    struct ppp 		*ppp = f->ppp;
    if (f->state == OPENED) {
        info(ppp, "No response to %d echo-requests", ppp->lcp_echos_pending);
        notice(ppp, "Serial link appears to be disconnected.");
        lcp_close(ppp, "Peer not responding");
	//ppp->status = EXIT_PEER_DEAD;
        ppp->status = PPP_ERR_PEERDEAD;
    }
}

/* -----------------------------------------------------------------------------
* Timer expired for the LCP echo requests from this process.
----------------------------------------------------------------------------- */
static void LcpEchoCheck (fsm *f)
{
    struct ppp 		*ppp = f->ppp;

    LcpSendEchoRequest (f);

    if (f->state != OPENED)
	return;

    /*
     * Start the timer for the next interval.
     */
    ppp->lcp_echoTORef = AddTimerToRunLoop(LcpEchoTimeout, ppp, ppp->lcp_echo_interval);
    ppp->lcp_echo_timer_running = 1;
}

/* -----------------------------------------------------------------------------
* LcpEchoTimeout - Timer expired on the LCP echo
----------------------------------------------------------------------------- */
static void LcpEchoTimeout (CFRunLoopTimerRef timer, void *arg)
{
    struct ppp 		*ppp = (struct ppp *)arg;
    
    if (ppp->lcp_echo_timer_running != 0) {
        ppp->lcp_echo_timer_running = 0;
        LcpEchoCheck (&ppp->lcp_fsm);
    }
}

/* -----------------------------------------------------------------------------
* LcpEchoReply - LCP has received a reply to the echo
----------------------------------------------------------------------------- */
static void lcp_received_echo_reply (fsm *f, int id, u_char *inp, int len)
{
    u_int32_t magic;
    struct ppp 		*ppp = f->ppp;

    /* Check the magic number - don't count replies from ourselves. */
    if (len < 4) {
        dbglog(ppp, "lcp: received short Echo-Reply, length %d", len);
	return;
    }
    GETLONG(magic, inp);
    if (ppp->lcp_gotoptions.neg_magicnumber
	&& magic == ppp->lcp_gotoptions.magicnumber) {
        warn(ppp, "appear to have received our own echo-reply!");
	return;
    }

    /* Reset the number of outstanding echo frames */
    ppp->lcp_echos_pending = 0;
}

/* -----------------------------------------------------------------------------
* LcpSendEchoRequest - Send an echo request frame to the peer
----------------------------------------------------------------------------- */
static void LcpSendEchoRequest (fsm *f)
{
    struct ppp 		*ppp = f->ppp;
    u_int32_t lcp_magic;
    u_char pkt[4], *pktp;

    /*
     * Detect the failure of the peer at this point.
     */
    if (ppp->lcp_echo_fails != 0) {
        if (ppp->lcp_echos_pending >= ppp->lcp_echo_fails) {
            LcpLinkFailure(f);
            ppp->lcp_echos_pending = 0;
	}
    }

    /*
     * Make and send the echo request frame.
     */
    if (f->state == OPENED) {
        lcp_magic = ppp->lcp_gotoptions.magicnumber;
	pktp = pkt;
	PUTLONG(lcp_magic, pktp);
        fsm_sdata(f, ECHOREQ, ppp->lcp_echo_number++ & 0xFF, pkt, pktp - pkt);
        ++ppp->lcp_echos_pending;
    }
}

/* -----------------------------------------------------------------------------
* lcp_echo_lowerup - Start the timer for the LCP frame
----------------------------------------------------------------------------- */
static void lcp_echo_lowerup (struct ppp *ppp)
{
    fsm *f = &ppp->lcp_fsm;

    /* Clear the parameters for generating echo frames */
    ppp->lcp_echos_pending      = 0;
    ppp->lcp_echo_number        = 0;
    ppp->lcp_echo_timer_running = 0;
  
    /* If a timeout interval is specified then start the timer */
    if (ppp->lcp_echo_interval != 0)
        LcpEchoCheck (f);
}

/* -----------------------------------------------------------------------------
* lcp_echo_lowerdown - Stop the timer for the LCP frame
----------------------------------------------------------------------------- */
static void lcp_echo_lowerdown (struct ppp *ppp)
{

    if (ppp->lcp_echo_timer_running != 0) {
        DelTimerFromRunLoop(&ppp->lcp_echoTORef);
        ppp->lcp_echo_timer_running = 0;
    }
}
