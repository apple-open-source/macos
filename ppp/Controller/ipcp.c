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
 * ipcp.c - PPP IP Control Protocol.
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

#include "pathnames.h"

/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
forward declarations
----------------------------------------------------------------------------- */

void ipcp_clear_addrs(struct ppp *ppp, u_int32_t ouraddr, u_int32_t hisaddr);


/*
 * Callbacks for fsm code.  (CI = Configuration Information)
 */
static void ipcp_resetci __P((fsm *));	/* Reset our CI */
static int  ipcp_cilen __P((fsm *));	        /* Return length of our CI */
static void ipcp_addci __P((fsm *, u_char *, int *)); /* Add our CI */
static int  ipcp_ackci __P((fsm *, u_char *, int));	/* Peer ack'd our CI */
static int  ipcp_nakci __P((fsm *, u_char *, int));	/* Peer nak'd our CI */
static int  ipcp_rejci __P((fsm *, u_char *, int));	/* Peer rej'd our CI */
static int  ipcp_reqci __P((fsm *, u_char *, int *, int)); /* Rcv CI */
static void ipcp_up __P((fsm *));		/* We're UP */
static void ipcp_down __P((fsm *));		/* We're DOWN */
static void ipcp_finished __P((fsm *));	/* Don't need lower layer */


static fsm_callbacks ipcp_callbacks = { /* IPCP callback routines */
    ipcp_resetci,		/* Reset our Configuration Information */
    ipcp_cilen,			/* Length of our Configuration Information */
    ipcp_addci,			/* Add our Configuration Information */
    ipcp_ackci,			/* ACK our Configuration Information */
    ipcp_nakci,			/* NAK our Configuration Information */
    ipcp_rejci,			/* Reject our Configuration Information */
    ipcp_reqci,			/* Request peer's Configuration Information */
    ipcp_up,			/* Called when fsm reaches OPENED state */
    ipcp_down,			/* Called when fsm leaves OPENED state */
    NULL,			/* Called when we want the lower layer up */
    ipcp_finished,		/* Called when we want the lower layer down */
    NULL,			/* Called when Protocol-Reject received */
    NULL,			/* Retransmission is necessary */
    NULL,			/* Called to handle protocol-specific codes */
    "IPCP"			/* String name of protocol */
};


/*
 * Protocol entry points from main code.
 */
static void ipcp_init __P((struct ppp *));
static void ipcp_open __P((struct ppp *));
static void ipcp_close __P((struct ppp *, char *));
static void ipcp_lowerup __P((struct ppp *));
static void ipcp_lowerdown __P((struct ppp *));
static void ipcp_input __P((struct ppp *, u_char *, int));
static void ipcp_protrej __P((struct ppp *));
static int  ipcp_printpkt __P((struct ppp *, u_char *, int,
			       void (*) __P((struct ppp *, void *, char *, ...)), void *));
int  ip_demand_conf __P((struct ppp *));
//static int  ip_demand_conf __P((struct ppp *));
static int  ip_active_pkt __P((u_char *, int));

struct protent ipcp_protent = {
    PPP_IPCP,
    ipcp_init,
    ipcp_input,
    ipcp_protrej,
    ipcp_lowerup,
    ipcp_lowerdown,
    ipcp_open,
    ipcp_close,
    ipcp_printpkt,
    NULL,
    "IPCP",
    "IP",
    0, //ipcp_option_list,
    0, //ip_check_options,
    ip_demand_conf,
    ip_active_pkt
};

/*
 * Lengths of configuration options.
 */
#define CILEN_VOID	2
#define CILEN_COMPRESS	4	/* min length for compression protocol opt. */
#define CILEN_VJ	6	/* length for RFC1332 Van-Jacobson opt. */
#define CILEN_ADDR	6	/* new-style single address option */
#define CILEN_ADDRS	10	/* old-style dual address option */


#define CODENAME(x)	((x) == CONFACK ? "ACK" : \
			 (x) == CONFNAK ? "NAK" : "REJ")


/* -----------------------------------------------------------------------------
* ipcp_init - Initialize IPCP.
----------------------------------------------------------------------------- */
void ipcp_init(struct ppp *ppp)
{
    fsm *f = &ppp->ipcp_fsm;
    ipcp_options *wo = &ppp->ipcp_wantoptions;
    ipcp_options *ao = &ppp->ipcp_allowoptions;
    ipcp_options *ho = &ppp->ipcp_hisoptions;
    ipcp_options *go = &ppp->ipcp_gotoptions;

    f->ppp = ppp;
    f->unit = ppp->unit;
    f->protocol = PPP_IPCP;
    f->callbacks = &ipcp_callbacks;
    fsm_init(f);

    memset(wo, 0, sizeof(*wo));
    memset(ao, 0, sizeof(*ao));
    memset(go, 0, sizeof(*go));
    memset(ho, 0, sizeof(*ho));

    wo->neg_addr = 1;
    wo->neg_vj = 0;
    wo->vj_protocol = IPCP_VJ_COMP;
    wo->maxslotindex = MAX_STATES - 1; /* really max index */
    wo->cflag = 1;


    /* max slots and slot-id compression are currently hardwired in */
    /* ppp_if.c to 16 and 1, this needs to be changed (among other */
    /* things) gmc */

    ao->neg_addr = 1;
    ao->neg_vj = 0;
    ao->maxslotindex = MAX_STATES - 1;
    ao->cflag = 1;

    /*
     * XXX These control whether the user may use the proxyarp
     */
    ao->proxy_arp = 1;


    ppp->ipcp_disable_defaultip = 0;
    ppp->ipcp_usepeerdns = 0;
}

/* -----------------------------------------------------------------------------
* ipcp_open - IPCP is allowed to come up.
----------------------------------------------------------------------------- */
void ipcp_open(struct ppp *ppp)
{
    fsm_open(&ppp->ipcp_fsm);
}

/* -----------------------------------------------------------------------------
* ipcp_close - Take IPCP down.
----------------------------------------------------------------------------- */
void ipcp_close(struct ppp *ppp, char *reason)
{
    fsm_close(&ppp->ipcp_fsm, reason);
}

/* -----------------------------------------------------------------------------
* ipcp_lowerup - The lower layer is up.
----------------------------------------------------------------------------- */
void ipcp_lowerup(struct ppp *ppp)
{
    fsm_lowerup(&ppp->ipcp_fsm);
}

/* -----------------------------------------------------------------------------
* ipcp_lowerdown - The lower layer is down.
----------------------------------------------------------------------------- */
void ipcp_lowerdown(struct ppp *ppp)
{
    fsm_lowerdown(&ppp->ipcp_fsm);
}

/* -----------------------------------------------------------------------------
* ipcp_input - Input IPCP packet.
----------------------------------------------------------------------------- */
void ipcp_input(struct ppp *ppp, u_char *p, int len)
{
    fsm_input(&ppp->ipcp_fsm, p, len);
}

/* -----------------------------------------------------------------------------
* ipcp_protrej - A Protocol-Reject was received for IPCP.
* Pretend the lower layer went down, so we shut up.
----------------------------------------------------------------------------- */
void ipcp_protrej(struct ppp *ppp)
{
    fsm_lowerdown(&ppp->ipcp_fsm);
}

/* -----------------------------------------------------------------------------
* ipcp_resetci - Reset our CI.
* Called by fsm_sconfreq, Send Configure Request.
----------------------------------------------------------------------------- */
void ipcp_resetci(fsm *f)
{
    struct ppp 		*ppp = f->ppp;
    ipcp_options *wo = &ppp->ipcp_wantoptions;
    ipcp_options *go = &ppp->ipcp_gotoptions;


    wo->req_addr = wo->neg_addr && ppp->ipcp_allowoptions.neg_addr;
    if (wo->ouraddr == 0 || ppp->ipcp_disable_defaultip)
	wo->accept_local = 1;
    if (wo->hisaddr == 0)
	wo->accept_remote = 1;
    wo->req_dns1 = ppp->ipcp_usepeerdns;	/* Request DNS addresses from the peer */
    wo->req_dns2 = ppp->ipcp_usepeerdns;
    *go = *wo;
    if (ppp->ipcp_disable_defaultip)
	go->ouraddr = 0;
}

/* -----------------------------------------------------------------------------
* ipcp_cilen - Return length of our CI.
* Called by fsm_sconfreq, Send Configure Request.
----------------------------------------------------------------------------- */
int ipcp_cilen(fsm * f)
{
    struct ppp 		*ppp = f->ppp;
    ipcp_options *go = &ppp->ipcp_gotoptions;
    ipcp_options *wo = &ppp->ipcp_wantoptions;
    ipcp_options *ho = &ppp->ipcp_hisoptions;

#define LENCIVJ(neg, old)	(neg ? (old? CILEN_COMPRESS : CILEN_VJ) : 0)
#define LENCIADDR(neg, old)	(neg ? (old? CILEN_ADDRS : CILEN_ADDR) : 0)
#define LENCIDNS(neg)		(neg ? (CILEN_ADDR) : 0)

    /*
     * First see if we want to change our options to the old
     * forms because we have received old forms from the peer.
     */
    if (wo->neg_addr && !go->neg_addr && !go->old_addrs) {
	/* use the old style of address negotiation */
	go->neg_addr = 1;
	go->old_addrs = 1;
    }
    if (wo->neg_vj && !go->neg_vj && !go->old_vj) {
	/* try an older style of VJ negotiation */
	/* use the old style only if the peer did */
	if (ho->neg_vj && ho->old_vj) {
	    go->neg_vj = 1;
	    go->old_vj = 1;
	    go->vj_protocol = ho->vj_protocol;
	}
    }

    return (LENCIADDR(go->neg_addr, go->old_addrs) +
	    LENCIVJ(go->neg_vj, go->old_vj) +
	    LENCIDNS(go->req_dns1) +
	    LENCIDNS(go->req_dns2)) ;
}

/* -----------------------------------------------------------------------------
* ipcp_addci - Add our desired CIs to a packet.
* Called by fsm_sconfreq, Send Configure Request.
----------------------------------------------------------------------------- */
void ipcp_addci(fsm *f, u_char *ucp, int *lenp)
{
    struct ppp 		*ppp = f->ppp;
    ipcp_options *go = &ppp->ipcp_gotoptions;
    int len = *lenp;

#define ADDCIVJ(opt, neg, val, old, maxslotindex, cflag) \
    if (neg) { \
	int vjlen = old? CILEN_COMPRESS : CILEN_VJ; \
	if (len >= vjlen) { \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(vjlen, ucp); \
	    PUTSHORT(val, ucp); \
	    if (!old) { \
		PUTCHAR(maxslotindex, ucp); \
		PUTCHAR(cflag, ucp); \
	    } \
	    len -= vjlen; \
	} else \
	    neg = 0; \
    }

#define ADDCIADDR(opt, neg, old, val1, val2) \
    if (neg) { \
	int addrlen = (old? CILEN_ADDRS: CILEN_ADDR); \
	if (len >= addrlen) { \
	    u_int32_t l; \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(addrlen, ucp); \
	    l = ntohl(val1); \
	    PUTLONG(l, ucp); \
	    if (old) { \
		l = ntohl(val2); \
		PUTLONG(l, ucp); \
	    } \
	    len -= addrlen; \
	} else \
	    neg = 0; \
    }

#define ADDCIDNS(opt, neg, addr) \
    if (neg) { \
	if (len >= CILEN_ADDR) { \
	    u_int32_t l; \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(CILEN_ADDR, ucp); \
	    l = ntohl(addr); \
	    PUTLONG(l, ucp); \
	    len -= CILEN_ADDR; \
	} else \
	    neg = 0; \
    }

    ADDCIADDR((go->old_addrs? CI_ADDRS: CI_ADDR), go->neg_addr,
	      go->old_addrs, go->ouraddr, go->hisaddr);

    ADDCIVJ(CI_COMPRESSTYPE, go->neg_vj, go->vj_protocol, go->old_vj,
	    go->maxslotindex, go->cflag);

    ADDCIDNS(CI_MS_DNS1, go->req_dns1, go->dnsaddr[0]);

    ADDCIDNS(CI_MS_DNS2, go->req_dns2, go->dnsaddr[1]);

    *lenp -= len;
}

/* -----------------------------------------------------------------------------
* ipcp_ackci - Ack our CIs.
* Called by fsm_rconfack, Receive Configure ACK.
*
* Returns:
*	0 - Ack was bad.
*	1 - Ack was good.
----------------------------------------------------------------------------- */
int ipcp_ackci(fsm *f, u_char *p, int len)
{
    struct ppp 		*ppp = f->ppp;
    ipcp_options *go = &ppp->ipcp_gotoptions;
    u_short cilen, citype, cishort;
    u_int32_t cilong;
    u_char cimaxslotindex, cicflag;

    /*
     * CIs must be in exactly the same order that we sent...
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */

#define ACKCIVJ(opt, neg, val, old, maxslotindex, cflag) \
    if (neg) { \
	int vjlen = old? CILEN_COMPRESS : CILEN_VJ; \
	if ((len -= vjlen) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != vjlen || \
	    citype != opt)  \
	    goto bad; \
	GETSHORT(cishort, p); \
	if (cishort != val) \
	    goto bad; \
	if (!old) { \
	    GETCHAR(cimaxslotindex, p); \
	    if (cimaxslotindex != maxslotindex) \
		goto bad; \
	    GETCHAR(cicflag, p); \
	    if (cicflag != cflag) \
		goto bad; \
	} \
    }

#define ACKCIADDR(opt, neg, old, val1, val2) \
    if (neg) { \
	int addrlen = (old? CILEN_ADDRS: CILEN_ADDR); \
	u_int32_t l; \
	if ((len -= addrlen) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != addrlen || \
	    citype != opt) \
	    goto bad; \
	GETLONG(l, p); \
	cilong = htonl(l); \
	if (val1 != cilong) \
	    goto bad; \
	if (old) { \
	    GETLONG(l, p); \
	    cilong = htonl(l); \
	    if (val2 != cilong) \
		goto bad; \
	} \
    }

#define ACKCIDNS(opt, neg, addr) \
    if (neg) { \
	u_int32_t l; \
	if ((len -= CILEN_ADDR) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_ADDR || citype != opt) \
	    goto bad; \
	GETLONG(l, p); \
	cilong = htonl(l); \
	if (addr != cilong) \
	    goto bad; \
    }

    ACKCIADDR((go->old_addrs? CI_ADDRS: CI_ADDR), go->neg_addr,
	      go->old_addrs, go->ouraddr, go->hisaddr);

    ACKCIVJ(CI_COMPRESSTYPE, go->neg_vj, go->vj_protocol, go->old_vj,
	    go->maxslotindex, go->cflag);

    ACKCIDNS(CI_MS_DNS1, go->req_dns1, go->dnsaddr[0]);

    ACKCIDNS(CI_MS_DNS2, go->req_dns2, go->dnsaddr[1]);

    /*
     * If there are any remaining CIs, then this packet is bad.
     */
    if (len != 0)
	goto bad;
    return (1);

bad:
    IPCPDEBUG(("ipcp_ackci: received bad Ack!"));
    return (0);
}

/* -----------------------------------------------------------------------------
* ipcp_nakci - Peer has sent a NAK for some of our CIs.
* This should not modify any state if the Nak is bad
* or if IPCP is in the OPENED state.
* Calback from fsm_rconfnakrej - Receive Configure-Nak or Configure-Reject.
*
* Returns:
*	0 - Nak was bad.
*	1 - Nak was good.
----------------------------------------------------------------------------- */
int ipcp_nakci(fsm *f, u_char *p, int len)
{
    struct ppp 		*ppp = f->ppp;
    ipcp_options *go = &ppp->ipcp_gotoptions;
    u_char cimaxslotindex, cicflag;
    u_char citype, cilen, *next;
    u_short cishort;
    u_int32_t ciaddr1, ciaddr2, l, cidnsaddr;
    ipcp_options no;		/* options we've seen Naks for */
    ipcp_options try;		/* options to request next time */

    BZERO(&no, sizeof(no));
    try = *go;

    /*
     * Any Nak'd CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */
#define NAKCIADDR(opt, neg, old, code) \
    if (go->neg && \
	len >= (cilen = (old? CILEN_ADDRS: CILEN_ADDR)) && \
	p[1] == cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETLONG(l, p); \
	ciaddr1 = htonl(l); \
	if (old) { \
	    GETLONG(l, p); \
	    ciaddr2 = htonl(l); \
	    no.old_addrs = 1; \
	} else \
	    ciaddr2 = 0; \
	no.neg = 1; \
	code \
    }

#define NAKCIVJ(opt, neg, code) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_COMPRESS || cilen == CILEN_VJ) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	no.neg = 1; \
        code \
    }

#define NAKCIDNS(opt, neg, code) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_ADDR) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETLONG(l, p); \
	cidnsaddr = htonl(l); \
	no.neg = 1; \
	code \
    }

    /*
     * Accept the peer's idea of {our,his} address, if different
     * from our idea, only if the accept_{local,remote} flag is set.
     */
    NAKCIADDR((go->old_addrs? CI_ADDRS: CI_ADDR), neg_addr, go->old_addrs,
	      if (go->accept_local && ciaddr1) { /* Do we know our address? */
		  try.ouraddr = ciaddr1;
	      }
	      if (go->accept_remote && ciaddr2) { /* Does he know his? */
		  try.hisaddr = ciaddr2;
	      }
	      );

    /*
     * Accept the peer's value of maxslotindex provided that it
     * is less than what we asked for.  Turn off slot-ID compression
     * if the peer wants.  Send old-style compress-type option if
     * the peer wants.
     */
    NAKCIVJ(CI_COMPRESSTYPE, neg_vj,
	    if (cilen == CILEN_VJ) {
		GETCHAR(cimaxslotindex, p);
		GETCHAR(cicflag, p);
		if (cishort == IPCP_VJ_COMP) {
		    try.old_vj = 0;
		    if (cimaxslotindex < go->maxslotindex)
			try.maxslotindex = cimaxslotindex;
		    if (!cicflag)
			try.cflag = 0;
		} else {
		    try.neg_vj = 0;
		}
	    } else {
		if (cishort == IPCP_VJ_COMP || cishort == IPCP_VJ_COMP_OLD) {
		    try.old_vj = 1;
		    try.vj_protocol = cishort;
		} else {
		    try.neg_vj = 0;
		}
	    }
	    );

    NAKCIDNS(CI_MS_DNS1, req_dns1,
	    try.dnsaddr[0] = cidnsaddr;
	    );

    NAKCIDNS(CI_MS_DNS2, req_dns2,
	    try.dnsaddr[1] = cidnsaddr;
	    );

    /*
     * There may be remaining CIs, if the peer is requesting negotiation
     * on an option that we didn't include in our request packet.
     * If they want to negotiate about IP addresses, we comply.
     * If they want us to ask for compression, we refuse.
     */
    while (len > CILEN_VOID) {
	GETCHAR(citype, p);
	GETCHAR(cilen, p);
	if( (len -= cilen) < 0 )
	    goto bad;
	next = p + cilen - 2;

	switch (citype) {
	case CI_COMPRESSTYPE:
	    if (go->neg_vj || no.neg_vj ||
		(cilen != CILEN_VJ && cilen != CILEN_COMPRESS))
		goto bad;
	    no.neg_vj = 1;
	    break;
	case CI_ADDRS:
	    if ((go->neg_addr && go->old_addrs) || no.old_addrs
		|| cilen != CILEN_ADDRS)
		goto bad;
	    try.neg_addr = 1;
	    try.old_addrs = 1;
	    GETLONG(l, p);
	    ciaddr1 = htonl(l);
	    if (ciaddr1 && go->accept_local)
		try.ouraddr = ciaddr1;
	    GETLONG(l, p);
	    ciaddr2 = htonl(l);
	    if (ciaddr2 && go->accept_remote)
		try.hisaddr = ciaddr2;
	    no.old_addrs = 1;
	    break;
	case CI_ADDR:
	    if (go->neg_addr || no.neg_addr || cilen != CILEN_ADDR)
		goto bad;
	    try.old_addrs = 0;
	    GETLONG(l, p);
	    ciaddr1 = htonl(l);
	    if (ciaddr1 && go->accept_local)
		try.ouraddr = ciaddr1;
	    if (try.ouraddr != 0)
		try.neg_addr = 1;
	    no.neg_addr = 1;
	    break;
	}
	p = next;
    }

    /*
     * OK, the Nak is good.  Now we can update state.
     * If there are any remaining options, we ignore them.
     */
    if (f->state != OPENED)
	*go = try;

    return 1;

bad:
    IPCPDEBUG(("ipcp_nakci: received bad Nak!"));
    return 0;
}

/* -----------------------------------------------------------------------------
* ipcp_rejci - Reject some of our CIs.
* Callback from fsm_rconfnakrej.
----------------------------------------------------------------------------- */
int ipcp_rejci(fsm *f, u_char *p, int len)
{
    struct ppp 		*ppp = f->ppp;
    ipcp_options *go = &ppp->ipcp_gotoptions;
    u_char cimaxslotindex, ciflag, cilen;
    u_short cishort;
    u_int32_t cilong;
    ipcp_options try;		/* options to request next time */

    try = *go;
    /*
     * Any Rejected CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */
#define REJCIADDR(opt, neg, old, val1, val2) \
    if (go->neg && \
	len >= (cilen = old? CILEN_ADDRS: CILEN_ADDR) && \
	p[1] == cilen && \
	p[0] == opt) { \
	u_int32_t l; \
	len -= cilen; \
	INCPTR(2, p); \
	GETLONG(l, p); \
	cilong = htonl(l); \
	/* Check rejected value. */ \
	if (cilong != val1) \
	    goto bad; \
	if (old) { \
	    GETLONG(l, p); \
	    cilong = htonl(l); \
	    /* Check rejected value. */ \
	    if (cilong != val2) \
		goto bad; \
	} \
	try.neg = 0; \
    }

#define REJCIVJ(opt, neg, val, old, maxslot, cflag) \
    if (go->neg && \
	p[1] == (old? CILEN_COMPRESS : CILEN_VJ) && \
	len >= p[1] && \
	p[0] == opt) { \
	len -= p[1]; \
	INCPTR(2, p); \
	GETSHORT(cishort, p); \
	/* Check rejected value. */  \
	if (cishort != val) \
	    goto bad; \
	if (!old) { \
	   GETCHAR(cimaxslotindex, p); \
	   if (cimaxslotindex != maxslot) \
	     goto bad; \
	   GETCHAR(ciflag, p); \
	   if (ciflag != cflag) \
	     goto bad; \
        } \
	try.neg = 0; \
     }

#define REJCIDNS(opt, neg, dnsaddr) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_ADDR) && \
	len >= cilen && \
	p[0] == opt) { \
	u_int32_t l; \
	len -= cilen; \
	INCPTR(2, p); \
	GETLONG(l, p); \
	cilong = htonl(l); \
	/* Check rejected value. */ \
	if (cilong != dnsaddr) \
	    goto bad; \
	try.neg = 0; \
    }


    REJCIADDR((go->old_addrs? CI_ADDRS: CI_ADDR), neg_addr,
	      go->old_addrs, go->ouraddr, go->hisaddr);

    REJCIVJ(CI_COMPRESSTYPE, neg_vj, go->vj_protocol, go->old_vj,
	    go->maxslotindex, go->cflag);

    REJCIDNS(CI_MS_DNS1, req_dns1, go->dnsaddr[0]);

    REJCIDNS(CI_MS_DNS2, req_dns2, go->dnsaddr[1]);

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
    IPCPDEBUG(("ipcp_rejci: received bad Reject!"));
    return 0;
}

/* -----------------------------------------------------------------------------
* ipcp_reqci - Check the peer's requested CIs and send appropriate response.
* Callback from fsm_rconfreq, Receive Configure Request
*
* Returns: CONFACK, CONFNAK or CONFREJ and input packet modified
* appropriately.  If reject_if_disagree is non-zero, doesn't return
* CONFNAK; returns CONFREJ if it can't return CONFACK.
----------------------------------------------------------------------------- */
int ipcp_reqci(fsm *f, u_char *inp, int *len, int reject_if_disagree)
{
    struct ppp 		*ppp = f->ppp;
    ipcp_options *wo = &ppp->ipcp_wantoptions;
    ipcp_options *ho = &ppp->ipcp_hisoptions;
    ipcp_options *ao = &ppp->ipcp_allowoptions;
    ipcp_options *go = &ppp->ipcp_gotoptions;
    u_char *cip, *next;		/* Pointer to current and next CIs */
    u_short cilen, citype;	/* Parsed len, type */
    u_short cishort;		/* Parsed short value */
    u_int32_t tl, ciaddr1, ciaddr2;/* Parsed address values */
    int rc = CONFACK;		/* Final packet return code */
    int orc;			/* Individual option return code */
    u_char *p;			/* Pointer to next char to parse */
    u_char *ucp = inp;		/* Pointer to current output char */
    int l = *len;		/* Length left */
    u_char maxslotindex, cflag;
    int d;

    /*
     * Reset all his options.
     */
    BZERO(ho, sizeof(*ho));
    
    /*
     * Process all his options.
     */
    next = inp;
    while (l) {
	orc = CONFACK;			/* Assume success */
	cip = p = next;			/* Remember begining of CI */
	if (l < 2 ||			/* Not enough data for CI header or */
	    p[1] < 2 ||			/*  CI length too small or */
	    p[1] > l) {			/*  CI length too big? */
	    IPCPDEBUG(("ipcp_reqci: bad CI length!"));
	    orc = CONFREJ;		/* Reject bad CI */
	    cilen = l;			/* Reject till end of packet */
	    l = 0;			/* Don't loop again */
	    goto endswitch;
	}
	GETCHAR(citype, p);		/* Parse CI type */
	GETCHAR(cilen, p);		/* Parse CI length */
	l -= cilen;			/* Adjust remaining length */
	next += cilen;			/* Step to next CI */

	switch (citype) {		/* Check CI type */
	case CI_ADDRS:
	    if (!ao->neg_addr ||
		cilen != CILEN_ADDRS) {	/* Check CI length */
		orc = CONFREJ;		/* Reject CI */
		break;
	    }

	    /*
	     * If he has no address, or if we both have his address but
	     * disagree about it, then NAK it with our idea.
	     * In particular, if we don't know his address, but he does,
	     * then accept it.
	     */
	    GETLONG(tl, p);		/* Parse source address (his) */
	    ciaddr1 = htonl(tl);
	    if (ciaddr1 != wo->hisaddr
		&& (ciaddr1 == 0 || !wo->accept_remote)) {
		orc = CONFNAK;
		if (!reject_if_disagree) {
		    DECPTR(sizeof(u_int32_t), p);
		    tl = ntohl(wo->hisaddr);
		    PUTLONG(tl, p);
		}
	    } else if (ciaddr1 == 0 && wo->hisaddr == 0) {
		/*
		 * If neither we nor he knows his address, reject the option.
		 */
		orc = CONFREJ;
		wo->req_addr = 0;	/* don't NAK with 0.0.0.0 later */
		break;
	    }

	    /*
	     * If he doesn't know our address, or if we both have our address
	     * but disagree about it, then NAK it with our idea.
	     */
	    GETLONG(tl, p);		/* Parse desination address (ours) */
	    ciaddr2 = htonl(tl);
	    if (ciaddr2 != wo->ouraddr) {
		if (ciaddr2 == 0 || !wo->accept_local) {
		    orc = CONFNAK;
		    if (!reject_if_disagree) {
			DECPTR(sizeof(u_int32_t), p);
			tl = ntohl(wo->ouraddr);
			PUTLONG(tl, p);
		    }
		} else {
		    go->ouraddr = ciaddr2;	/* accept peer's idea */
		}
	    }

	    ho->neg_addr = 1;
	    ho->old_addrs = 1;
	    ho->hisaddr = ciaddr1;
	    ho->ouraddr = ciaddr2;
	    break;

	case CI_ADDR:
	    if (!ao->neg_addr ||
		cilen != CILEN_ADDR) {	/* Check CI length */
		orc = CONFREJ;		/* Reject CI */
		break;
	    }

	    /*
	     * If he has no address, or if we both have his address but
	     * disagree about it, then NAK it with our idea.
	     * In particular, if we don't know his address, but he does,
	     * then accept it.
	     */
	    GETLONG(tl, p);	/* Parse source address (his) */
	    ciaddr1 = htonl(tl);
	    if (ciaddr1 != wo->hisaddr
		&& (ciaddr1 == 0 || !wo->accept_remote)) {
		orc = CONFNAK;
		if (!reject_if_disagree) {
		    DECPTR(sizeof(u_int32_t), p);
		    tl = ntohl(wo->hisaddr);
		    PUTLONG(tl, p);
		}
	    } else if (ciaddr1 == 0 && wo->hisaddr == 0) {
		/*
		 * Don't ACK an address of 0.0.0.0 - reject it instead.
		 */
		orc = CONFREJ;
		wo->req_addr = 0;	/* don't NAK with 0.0.0.0 later */
		break;
	    }
	
	    ho->neg_addr = 1;
	    ho->hisaddr = ciaddr1;
	    break;

	case CI_MS_DNS1:
	case CI_MS_DNS2:
	    /* Microsoft primary or secondary DNS request */
	    d = citype == CI_MS_DNS2;

	    /* If we do not have a DNS address then we cannot send it */
	    if (ao->dnsaddr[d] == 0 ||
		cilen != CILEN_ADDR) {	/* Check CI length */
		orc = CONFREJ;		/* Reject CI */
		break;
	    }
	    GETLONG(tl, p);
	    if (htonl(tl) != ao->dnsaddr[d]) {
                DECPTR(sizeof(u_int32_t), p);
		tl = ntohl(ao->dnsaddr[d]);
		PUTLONG(tl, p);
		orc = CONFNAK;
            }
            break;

	case CI_MS_WINS1:
	case CI_MS_WINS2:
	    /* Microsoft primary or secondary WINS request */
	    d = citype == CI_MS_WINS2;

	    /* If we do not have a DNS address then we cannot send it */
	    if (ao->winsaddr[d] == 0 ||
		cilen != CILEN_ADDR) {	/* Check CI length */
		orc = CONFREJ;		/* Reject CI */
		break;
	    }
	    GETLONG(tl, p);
	    if (htonl(tl) != ao->winsaddr[d]) {
                DECPTR(sizeof(u_int32_t), p);
		tl = ntohl(ao->winsaddr[d]);
		PUTLONG(tl, p);
		orc = CONFNAK;
            }
            break;
	
	case CI_COMPRESSTYPE:
	    if (!ao->neg_vj ||
		(cilen != CILEN_VJ && cilen != CILEN_COMPRESS)) {
		orc = CONFREJ;
		break;
	    }
	    GETSHORT(cishort, p);

	    if (!(cishort == IPCP_VJ_COMP ||
		  (cishort == IPCP_VJ_COMP_OLD && cilen == CILEN_COMPRESS))) {
		orc = CONFREJ;
		break;
	    }

	    ho->neg_vj = 1;
	    ho->vj_protocol = cishort;
	    if (cilen == CILEN_VJ) {
		GETCHAR(maxslotindex, p);
		if (maxslotindex > ao->maxslotindex) { 
		    orc = CONFNAK;
		    if (!reject_if_disagree){
			DECPTR(1, p);
			PUTCHAR(ao->maxslotindex, p);
		    }
		}
		GETCHAR(cflag, p);
		if (cflag && !ao->cflag) {
		    orc = CONFNAK;
		    if (!reject_if_disagree){
			DECPTR(1, p);
			PUTCHAR(wo->cflag, p);
		    }
		}
		ho->maxslotindex = maxslotindex;
		ho->cflag = cflag;
	    } else {
		ho->old_vj = 1;
		ho->maxslotindex = MAX_STATES - 1;
		ho->cflag = 1;
	    }
	    break;

	default:
	    orc = CONFREJ;
	    break;
	}
endswitch:
	if (orc == CONFACK &&		/* Good CI */
	    rc != CONFACK)		/*  but prior CI wasnt? */
	    continue;			/* Don't send this one */

	if (orc == CONFNAK) {		/* Nak this CI? */
	    if (reject_if_disagree)	/* Getting fed up with sending NAKs? */
		orc = CONFREJ;		/* Get tough if so */
	    else {
		if (rc == CONFREJ)	/* Rejecting prior CI? */
		    continue;		/* Don't send this one */
		if (rc == CONFACK) {	/* Ack'd all prior CIs? */
		    rc = CONFNAK;	/* Not anymore... */
		    ucp = inp;		/* Backup */
		}
	    }
	}

	if (orc == CONFREJ &&		/* Reject this CI */
	    rc != CONFREJ) {		/*  but no prior ones? */
	    rc = CONFREJ;
	    ucp = inp;			/* Backup */
	}

	/* Need to move CI? */
	if (ucp != cip)
	    BCOPY(cip, ucp, cilen);	/* Move it */

	/* Update output pointer */
	INCPTR(cilen, ucp);
    }

    /*
     * If we aren't rejecting this packet, and we want to negotiate
     * their address, and they didn't send their address, then we
     * send a NAK with a CI_ADDR option appended.  We assume the
     * input buffer is long enough that we can append the extra
     * option safely.
     */
    if (rc != CONFREJ && !ho->neg_addr &&
	wo->req_addr && !reject_if_disagree) {
	if (rc == CONFACK) {
	    rc = CONFNAK;
	    ucp = inp;			/* reset pointer */
	    wo->req_addr = 0;		/* don't ask again */
	}
	PUTCHAR(CI_ADDR, ucp);
	PUTCHAR(CILEN_ADDR, ucp);
	tl = ntohl(wo->hisaddr);
	PUTLONG(tl, ucp);
    }

    *len = ucp - inp;			/* Compute output length */
    IPCPDEBUG(("ipcp: returning Configure-%s", CODENAME(rc)));
    return (rc);			/* Return final code */
}

/* -----------------------------------------------------------------------------
* ip_demand_conf - configure the interface as though
* IPCP were up, for use with dial-on-demand.
----------------------------------------------------------------------------- */
int ip_demand_conf(struct ppp *ppp)
{
    ipcp_options *wo = &ppp->ipcp_wantoptions;

    if (wo->hisaddr == 0) {
	/* make up an arbitrary address for the peer */
	wo->hisaddr = htonl(0x0a707070 + ppp->unit);
	wo->accept_remote = 1;
    }
    if (wo->ouraddr == 0) {
	/* make up an arbitrary address for us */
	wo->ouraddr = htonl(0x0a404040 + ppp->unit);
	wo->accept_local = 1;
	ppp->ipcp_disable_defaultip = 1;	/* don't tell the peer this address */
    }


    ppp_attachip(ppp);
    if (!ppp_addroute(ppp, wo->ouraddr, wo->hisaddr, ppp->ipcp_usermask, 0, 0, 0))
	return 0;
//    if (!sifup(ppp))
//	return 0;
//    if (!sifnpmode(ppp, PPP_IP, NPMODE_QUEUE))
//	return 0;
    if (wo->proxy_arp)
	if (ppp_setipv4proxyarp(ppp, wo->hisaddr))
	    ppp->ipcp_proxy_arp_set = 1;

    notice(ppp, "local  IP address %I", wo->ouraddr);
    notice(ppp, "remote IP address %I", wo->hisaddr);

    return 1;
}

/* -----------------------------------------------------------------------------
* ipcp_up - IPCP has come UP.
*
* Configure the IP network interface appropriately and bring it up.
----------------------------------------------------------------------------- */
void ipcp_up(fsm * f)
{
    struct ppp *ppp = f->ppp;
    ipcp_options *ho = &ppp->ipcp_hisoptions;
    ipcp_options *go = &ppp->ipcp_gotoptions;
    ipcp_options *wo = &ppp->ipcp_wantoptions;
    
    ppp_new_event(ppp, PPP_EVT_IPCP_UP);

  /*
     * We must have a non-zero IP address for both ends of the link.
     */
    if (!ho->neg_addr)
	ho->hisaddr = wo->hisaddr;

   if (ho->hisaddr == 0) {
	error(ppp, "Could not determine remote IP address");
	ipcp_close(f->ppp, "Could not determine remote IP address");
	return;
    }
   if (go->ouraddr == 0) {
	error(ppp, "Could not determine local IP address");
	ipcp_close(f->ppp, "Could not determine local IP address");
	return;
    }

  /*
     * Check that the peer is allowed to use the IP address it wants.
     */
    if (!auth_ip_addr(ppp, ho->hisaddr)) {
	error(ppp, "Peer is not authorized to use remote address %I", ho->hisaddr);
	ipcp_close(ppp, "Unauthorized remote IP address");
	return;
    }

   ppp_attachip(ppp);

   /* set tcp compression */
   ppp_set_ipvjcomp(ppp, ho->neg_vj, ho->cflag, ho->maxslotindex);

   /*
     * If we are doing dial-on-demand, the interface is already
     * configured, so we put out any saved-up packets, then set the
     * interface to pass IP packets.
     */
    if (ppp->demand) {
//	if (go->ouraddr != wo->ouraddr || ho->hisaddr != wo->hisaddr) {
	if (go->ouraddr != ppp->ipcp_ouraddr || ho->hisaddr != ppp->ipcp_hisaddr) {
            ipcp_clear_addrs(ppp, ppp->ipcp_ouraddr, ppp->ipcp_hisaddr);
            //ipcp_clear_addrs(ppp, wo->ouraddr, wo->hisaddr);
	    //if (go->ouraddr != wo->ouraddr) {
	    if (go->ouraddr != ppp->ipcp_ouraddr) {
                warn(ppp, "Local IP address changed to %I", go->ouraddr);
		wo->ouraddr = go->ouraddr;
	    }
            //if (ho->hisaddr != wo->hisaddr) {
            if (ho->hisaddr != ppp->ipcp_hisaddr) {
                warn(ppp, "Remote IP address changed to %I", ho->hisaddr);
		wo->hisaddr = ho->hisaddr;
	    } 

            /* publish route and dns information */
            ppp->ipcp_hisaddr = ho->hisaddr;
            ppp->ipcp_ouraddr = go->ouraddr;
            if (!ppp_addroute(ppp, go->ouraddr, ho->hisaddr, ppp->ipcp_usermask, go->dnsaddr[0], go->dnsaddr[1], 0)) {
		if (ppp->debug)
                    warn(ppp, "Interface configuration failed 1");
		ipcp_close(ppp, "Interface configuration failed");
		return;
	    }

	    /* Make a proxy ARP entry if requested. */
	    if (ppp->ipcp_wantoptions.proxy_arp)
                if (ppp_setipv4proxyarp(ppp, ho->hisaddr))
                    ppp->ipcp_proxy_arp_set = 1;

	}

        // ATTENTION : NEED TO COME BACK
       // demand_rexmit(PPP_IP);


        //sifnpmode(ppp, PPP_IP, NPMODE_PASS);

    } else {
	/*
	 * Set IP addresses and (if specified) netmask.
	 */
        
        /* publish route and dns information */
            ppp->ipcp_hisaddr = ho->hisaddr;
            ppp->ipcp_ouraddr = go->ouraddr;
        if (!ppp_addroute(ppp, go->ouraddr, ho->hisaddr, ppp->ipcp_usermask, go->dnsaddr[0], go->dnsaddr[1], 0)) {
            if (ppp->debug)
                warn(ppp, "Interface address configuration failed");
            ipcp_close(ppp, "Interface address configuration failed");
            return;
        }

	/* bring the interface up for IP */
	if (!ppp_ifup(ppp)) {
	    if (ppp->debug)
		warn(ppp, "Interface failed to come up");
	    ipcp_close(ppp, "Interface configuration failed");
	    return;
	}

	/* Make a proxy ARP entry if requested. */
	if (ppp->ipcp_wantoptions.proxy_arp)
            if (ppp_setipv4proxyarp(ppp, ho->hisaddr))
                ppp->ipcp_proxy_arp_set = 1;

	ppp->ipcp_wantoptions.ouraddr = go->ouraddr;

	notice(ppp, "local  IP address %I", go->ouraddr);
	notice(ppp, "remote IP address %I", ho->hisaddr);
	if (go->dnsaddr[0])
	    notice(ppp, "primary   DNS address %I", go->dnsaddr[0]);
	if (go->dnsaddr[1])
	    notice(ppp, "secondary DNS address %I", go->dnsaddr[1]);
    }


    auth_np_up(ppp, PPP_IP);
    ppp->ipcp_is_up = 1;

}

/* -----------------------------------------------------------------------------
* ipcp_down - IPCP has gone DOWN.
*
* Take the IP network interface down, clear its addresses
* and delete routes through it.
----------------------------------------------------------------------------- */
void ipcp_down(fsm *f)
{
    struct ppp *ppp = f->ppp;
    IPCPDEBUG(("ipcp: down"));

    ppp_new_event(ppp, PPP_EVT_IPCP_DOWN);

    if (ppp->ipcp_is_up) {
	ppp->ipcp_is_up = 0;
        auth_np_down(ppp, PPP_IP);
    }
    ppp_set_ipvjcomp(ppp, 0, 0, 0);

    /*
     * If we are doing dial-on-demand, set the interface
     * to queue up outgoing packets (for now).
     */
    if (ppp->demand) {
        //ipcp_clear_addrs(ppp, ppp->ipcp_gotoptions.ouraddr,
        //                 ppp->ipcp_hisoptions.hisaddr);

        //ppp_detachip(ppp);
//	sifnpmode(ppp, PPP_IP, NPMODE_QUEUE);
    } else {
//	sifnpmode(ppp, PPP_IP, NPMODE_DROP);
	ppp_ifdown(ppp);
	ipcp_clear_addrs(ppp, ppp->ipcp_gotoptions.ouraddr,
			 ppp->ipcp_hisoptions.hisaddr);

        ppp_detachip(ppp);
   }

}

/* -----------------------------------------------------------------------------
* ipcp_clear_addrs() - clear the interface addresses, routes,
* proxy arp entries, etc.
----------------------------------------------------------------------------- */
void ipcp_clear_addrs(struct ppp *ppp, u_int32_t ouraddr, u_int32_t hisaddr)
{
    if (ppp->ipcp_proxy_arp_set) {
        ppp_clearipv4proxyarp(ppp, hisaddr);
        ppp->ipcp_proxy_arp_set = 0;
    }
    ppp_delroute(ppp, ouraddr, hisaddr);
}

/* -----------------------------------------------------------------------------
* ipcp_finished - possibly shut down the lower layers.
----------------------------------------------------------------------------- */
void ipcp_finished(fsm *f)
{
    auth_np_finished(f->ppp, PPP_IP);
}

/* -----------------------------------------------------------------------------
* setescape - add chars to the set we escape on transmission.
----------------------------------------------------------------------------- */
/*
 * ipcp_printpkt - print the contents of an IPCP packet.
 */
static char *ipcp_codenames[] = {
    "ConfReq", "ConfAck", "ConfNak", "ConfRej",
    "TermReq", "TermAck", "CodeRej"
};

static int
ipcp_printpkt(ppp, p, plen, printer, arg)
struct ppp *ppp;
u_char *p;
    int plen;
    void (*printer) __P((struct ppp *, void *, char *, ...));
    void *arg;
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

    if (code >= 1 && code <= sizeof(ipcp_codenames) / sizeof(char *))
        printer(ppp, arg, " %s", ipcp_codenames[code-1]);
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
	    case CI_ADDRS:
		if (olen == CILEN_ADDRS) {
		    p += 2;
		    GETLONG(cilong, p);
                    printer(ppp, arg, "addrs %I", htonl(cilong));
		    GETLONG(cilong, p);
                    printer(ppp, arg, " %I", htonl(cilong));
		}
		break;
	    case CI_COMPRESSTYPE:
		if (olen >= CILEN_COMPRESS) {
		    p += 2;
		    GETSHORT(cishort, p);
                    printer(ppp, arg, "compress ");
		    switch (cishort) {
		    case IPCP_VJ_COMP:
                        printer(ppp, arg, "VJ");
			break;
		    case IPCP_VJ_COMP_OLD:
                        printer(ppp, arg, "old-VJ");
			break;
		    default:
                        printer(ppp, arg, "0x%x", cishort);
		    }
		}
		break;
	    case CI_ADDR:
		if (olen == CILEN_ADDR) {
		    p += 2;
		    GETLONG(cilong, p);
                    printer(ppp, arg, "addr %I", htonl(cilong));
		}
		break;
	    case CI_MS_DNS1:
	    case CI_MS_DNS2:
	        p += 2;
		GETLONG(cilong, p);
                printer(ppp, arg, "ms-dns%d %I", code - CI_MS_DNS1 + 1,
			htonl(cilong));
		break;
	    case CI_MS_WINS1:
	    case CI_MS_WINS2:
	        p += 2;
		GETLONG(cilong, p);
                printer(ppp, arg, "ms-wins %I", htonl(cilong));
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
    }

    /* print the rest of the bytes in the packet */
    for (; len > 0; --len) {
	GETCHAR(code, p);
        printer(ppp, arg, " %.2x", code);
    }

    return p - pstart;
}

/*
 * ip_active_pkt - see if this IP packet is worth bringing the link up for.
 * We don't bring the link up for IP fragments or for TCP FIN packets
 * with no data.
 */
#define IP_HDRLEN	20	/* bytes */
#define IP_OFFMASK	0x1fff
#define IPPROTO_TCP	6
#define TCP_HDRLEN	20
#define TH_FIN		0x01

/*
 * We use these macros because the IP header may be at an odd address,
 * and some compilers might use word loads to get th_off or ip_hl.
 */

#define net_short(x)	(((x)[0] << 8) + (x)[1])
#define get_iphl(x)	(((unsigned char *)(x))[0] & 0xF)
#define get_ipoff(x)	net_short((unsigned char *)(x) + 6)
#define get_ipproto(x)	(((unsigned char *)(x))[9])
#define get_tcpoff(x)	(((unsigned char *)(x))[12] >> 4)
#define get_tcpflags(x)	(((unsigned char *)(x))[13])

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int
ip_active_pkt(pkt, len)
    u_char *pkt;
    int len;
{
    u_char *tcp;
    int hlen;

    len -= PPP_HDRLEN;
    pkt += PPP_HDRLEN;
    if (len < IP_HDRLEN)
	return 0;
    if ((get_ipoff(pkt) & IP_OFFMASK) != 0)
	return 0;
    if (get_ipproto(pkt) != IPPROTO_TCP)
	return 1;
    hlen = get_iphl(pkt) * 4;
    if (len < hlen + TCP_HDRLEN)
	return 0;
    tcp = pkt + hlen;
    if ((get_tcpflags(tcp) & TH_FIN) != 0 && len == hlen + get_tcpoff(tcp) * 4)
	return 0;
    return 1;
}
