/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * acscp.c - PPP Apple Client Server Control Protocol.
 * This code was based on ipcp.c from pppd and falls under
 * the following license:
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

#define RCSID	"$Id: acscp.c,v 1.12 2006/03/01 01:07:44 lindak Exp $"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <net/if_types.h>
#include <unistd.h>
#include "pppd.h"
#include "fsm.h"
#include "pathnames.h"
#include "acsp.h"
#include "acscp.h"
#include "acscp_plugin.h"
#include <net/if.h> 		// required for if_ppp.h
#include "../../Family/if_ppp.h"
#include "../vpnd/RASSchemaDefinitions.h"
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCSchemaDefinitions.h>

#ifndef lint
static const char rcsid[] = RCSID;
#endif
//
// Defines
//

//
// Lengths of configuration options.
//
 
#define CILEN_VOID	2
#define CILEN_ROUTES	6
#define CILEN_DOMAINS	6


#define CODENAME(x)	((x) == CONFACK ? "ACK" : \
			 (x) == CONFNAK ? "NAK" : "REJ")
                         

//
// global vars
//

acscp_options acscp_wantoptions[NUM_PPP];	/* Options that we want to request */
acscp_options acscp_gotoptions[NUM_PPP];	/* Options that peer ack'd */
acscp_options acscp_allowoptions[NUM_PPP]; 	/* Options we allow peer to request */
acscp_options acscp_hisoptions[NUM_PPP];	/* Options that we ack'd */

/* Hook for a plugin to know when acsp has come up */
void (*acsp_up_hook) __P((void)) = NULL;

/* Hook for a plugin to know when acsp has come down */
void (*acsp_down_hook) __P((void)) = NULL;

/* Notifiers for when ACSP goes up and down */
struct notifier *acsp_up_notifier = NULL;
struct notifier *acsp_down_notifier = NULL;

//
// local vars
//
static int acscp_is_open;		/* haven't called np_finished() */

// option vars from command line options - these override plist settings
// these will have to be passed to the plugin
// once the plugin code is separated out
bool	acsp_no_routes;
bool	acsp_no_domains;

bool	acsp_use_dhcp;
bool	acsp_intercept_dhcp;


//
// Callbacks for fsm code.  (CI = Configuration Information)
//
static void acscp_resetci __P((fsm *));				/* Reset our CI */
static int  acscp_cilen __P((fsm *));	        		/* Return length of our CI */
static void acscp_addci __P((fsm *, u_char *, int *));	 	/* Add our CI */
static int  acscp_ackci __P((fsm *, u_char *, int));		/* Peer ack'd our CI */
static int  acscp_nakci __P((fsm *, u_char *, int));		/* Peer nak'd our CI */
static int  acscp_rejci __P((fsm *, u_char *, int));		/* Peer rej'd our CI */
static int  acscp_reqci __P((fsm *, u_char *, int *, int)); 	/* Rcv CI */
static void acscp_up __P((fsm *));				/* We're UP */
static void acscp_down __P((fsm *));				/* We're DOWN */
static void acscp_finished __P((fsm *));			/* Don't need lower layer */

fsm acscp_fsm[NUM_PPP];		/* acscp fsm structure */

static fsm_callbacks acscp_callbacks = { /* ACSCP callback routines */
    acscp_resetci,		/* Reset our Configuration Information */
    acscp_cilen,		/* Length of our Configuration Information */
    acscp_addci,		/* Add our Configuration Information */
    acscp_ackci,		/* ACK our Configuration Information */
    acscp_nakci,		/* NAK our Configuration Information */
    acscp_rejci,		/* Reject our Configuration Information */
    acscp_reqci,		/* Request peer's Configuration Information */
    acscp_up,			/* Called when fsm reaches OPENED state */
    acscp_down,			/* Called when fsm leaves OPENED state */
    NULL,			/* Called when we want the lower layer up */
    acscp_finished,		/* Called when we want the lower layer down */
    NULL,			/* Called when Protocol-Reject received */
    NULL,			/* Retransmission is necessary */
    NULL,			/* Called to handle protocol-specific codes */
    "acscp"			/* String name of protocol */
};

//
// Command-line options.
//

static option_t acscp_option_list[] = {
    { "noacsp", o_bool, &acscp_protent.enabled_flag,
      "Disable ACSP and ACSCP" },

    { "acscp-restart", o_int, &acscp_fsm[0].timeouttime,
      "Set timeout for acscp", OPT_PRIO },
    { "acscp-max-terminate", o_int, &acscp_fsm[0].maxtermtransmits,
      "Set max #xmits for term-reqs", OPT_PRIO },
    { "acscp-max-configure", o_int, &acscp_fsm[0].maxconfreqtransmits,
      "Set max #xmits for conf-reqs", OPT_PRIO },
    { "acscp-max-failure", o_int, &acscp_fsm[0].maxnakloops,
      "Set max #conf-naks for acscp", OPT_PRIO },
      
    { "intercept-dhcp", o_bool, &acsp_intercept_dhcp,
      "Intercept dhcp client requests", 1 },
    { "no-intercept-dhcp", o_bool, &acsp_intercept_dhcp,
      "Intercept dhcp client requests", 0 },
    { "use-dhcp", o_bool, &acsp_use_dhcp,
      "Send dhcp request", 1 },
    { "no-use-dhcp", o_bool, &acsp_use_dhcp,
      "Send dhcp request", 0 },
    { NULL }
};


//
// Protocol entry points from main code.
//
static void acscp_init __P((int));
static void acscp_open __P((int));
static void acscp_close __P((int, char *));
static void acscp_lowerup __P((int));
static void acscp_lowerdown __P((int));
static void acscp_input __P((int, u_char *, int));
static void acscp_protrej __P((int));
static int  acscp_printpkt __P((u_char *, int,  void (*) __P((void *, char *, ...)), void *));
static void acscp_check_options __P((void));
static int acscp_state __P((int));


struct protent acscp_protent = {
    PPP_ACSCP,
    acscp_init,
    acscp_input,
    acscp_protrej,
    acscp_lowerup,
    acscp_lowerdown,
    acscp_open,
    acscp_close,
    acscp_printpkt,
    acsp_data_input,
    1,
    "ACSCP",
    "ACSP",
    acscp_option_list,
    acscp_check_options,
    NULL,
    NULL,
    NULL,
    NULL,
    acscp_state,
    acsp_printpkt
};


/*
 * acscp_init - Initialize acscp.
 */
static void
acscp_init(int unit)
{
    fsm *f = &acscp_fsm[unit];
    acscp_options *wo = &acscp_wantoptions[0];
    acscp_options *ao = &acscp_allowoptions[0];
    
    acsp_no_routes = 0;
    acsp_no_domains = 0;

    f->unit = unit;
    f->protocol = PPP_ACSCP;
    f->callbacks = &acscp_callbacks;
    fsm_init(&acscp_fsm[unit]);

    memset(wo, 0, sizeof(*wo));
    memset(ao, 0, sizeof(*ao));
        
    wo->routes_version = LATEST_ROUTES_VERSION;
    wo->domains_version = LATEST_DOMAINS_VERSION;

    ao->routes_version = LATEST_ROUTES_VERSION;
    ao->domains_version = LATEST_DOMAINS_VERSION;
    
    //plugin_list = 0;
    
   
}

/*
 * acsp_check_options
 */
static void
acscp_check_options(void)
{
    if (acscp_protent.enabled_flag || acsp_use_dhcp || acsp_intercept_dhcp)
        add_notifier(&phasechange, acsp_init_plugins, 0);    // to setup plugins

	if (acsp_intercept_dhcp)
		ip_src_address_filter |= NPAFMODE_DHCP_INTERCEPT_SERVER;
	if (acsp_use_dhcp)
		ip_src_address_filter |= NPAFMODE_DHCP_INTERCEPT_CLIENT;
}



/*
 * acscp_open - acscp is allowed to come up.
 */
static void
acscp_open(int unit)
{
    fsm_open(&acscp_fsm[unit]);
    acscp_is_open = 1;
}


/*
 * acscp_close - Take acscp down.
 */
static void
acscp_close(int unit, char *reason)
{
    fsm_close(&acscp_fsm[unit], reason);
}


/*
 * acscp_lowerup - The lower layer is up.
 */
static void
acscp_lowerup(int unit)
{
    fsm_lowerup(&acscp_fsm[unit]);
}


/*
 * acscp_lowerdown - The lower layer is down.
 */
static void
acscp_lowerdown(int unit)
{
    fsm_lowerdown(&acscp_fsm[unit]);
}


/*
 * acscp_input - Input acscp packet.
 */
static void
acscp_input(int unit, u_char *p, int len)
{
    fsm_input(&acscp_fsm[unit], p, len);
}

/*
 * acscp_protrej - A Protocol-Reject was received for acscp.
 *
 * Pretend the lower layer went down, so we shut up.
 */
static void
acscp_protrej(int unit)
{
    fsm_protreject(&acscp_fsm[unit]);
}

/*
 * acscp_state - return protocol state for the unit.
 */
static int
acscp_state(int unit)
{
   return acscp_fsm[unit].state;
}

/*
 * acscp_resetci - Reset our CI.
 * Called by fsm_sconfreq, Send Configure Request.
 */
static void
acscp_resetci(fsm *f)
{
    acscp_options *wo = &acscp_wantoptions[0];
    acscp_options *go = &acscp_gotoptions[0];

    *go = *wo;
}


/*
 * acscp_cilen - Return length of our CI.
 * Called by fsm_sconfreq, Send Configure Request.
 */
static int
acscp_cilen(fsm *f)
{
    acscp_options *go = &acscp_gotoptions[0];

#define LENCIROUTES(neg)	(neg ? CILEN_ROUTES : 0)
#define LENCIDOMAINS(neg)	(neg ? CILEN_DOMAINS : 0)

    return (LENCIROUTES(go->neg_routes) +
	    LENCIDOMAINS(go->neg_domains));
}


/*
 * acscp_addci - Add our desired CIs to a packet.
 * Called by fsm_sconfreq, Send Configure Request.
 */
static void
acscp_addci(fsm *f, u_char *ucp, int *lenp)
{
    acscp_options *go = &acscp_gotoptions[0];
    int len = *lenp;

#define ADDCIROUTES(opt, neg, vers) \
    if (neg) { \
	if (len >= CILEN_ROUTES) { \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(CILEN_ROUTES, ucp); \
	    PUTLONG(vers, ucp); \
	    len -= CILEN_ROUTES; \
	} else \
	    neg = 0; \
    }

#define ADDCIDOMAINS(opt, neg, vers) \
    if (neg) { \
	if (len >= CILEN_DOMAINS) { \
	    PUTCHAR(opt, ucp); \
	    PUTCHAR(CILEN_DOMAINS, ucp); \
	    PUTLONG(vers, ucp); \
	    len -= CILEN_DOMAINS; \
	} else \
	    neg = 0; \
    }

    ADDCIROUTES(CI_ROUTES, go->neg_routes, go->routes_version);

    ADDCIDOMAINS(CI_DOMAINS, go->neg_domains, go->domains_version);

    *lenp -= len;
}


/*
 * acscp_ackci - Ack our CIs.
 * Called by fsm_rconfack, Receive Configure ACK.
 *
 * Returns:
 *	0 - Ack was bad.
 *	1 - Ack was good.
 */
static int
acscp_ackci(fsm *f, u_char *p, int len)
{
    acscp_options *go = &acscp_gotoptions[0];
    u_short cilen, citype;
    u_int32_t cilong;

    /*
     * CIs must be in exactly the same order that we sent...
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */


#define ACKCIROUTES(opt, neg, vers) \
    if (neg) { \
	if ((len -= CILEN_ROUTES) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_ROUTES || citype != opt) \
	    goto bad; \
	GETLONG(cilong, p); \
	if (vers != cilong) \
	    goto bad; \
    }

#define ACKCIDOMAINS(opt, neg, vers) \
    if (neg) { \
	if ((len -= CILEN_DOMAINS) < 0) \
	    goto bad; \
	GETCHAR(citype, p); \
	GETCHAR(cilen, p); \
	if (cilen != CILEN_DOMAINS || citype != opt) \
	    goto bad; \
	GETLONG(cilong, p); \
	if (vers != cilong) \
	    goto bad; \
    }


    ACKCIROUTES(CI_ROUTES, go->neg_routes, go->routes_version);

    ACKCIDOMAINS(CI_DOMAINS, go->neg_domains, go->domains_version);

    /*
     * If there are any remaining CIs, then this packet is bad.
     */
    if (len != 0)
	goto bad;
    return (1);

bad:
    ACSCPDEBUG(("acscp_ackci: received bad Ack!"));
    return (0);
}

/*
 * acscp_nakci - Peer has sent a NAK for some of our CIs.
 * This should not modify any state if the Nak is bad
 * or if acscp is in the OPENED state.
 * Calback from fsm_rconfnakrej - Receive Configure-Nak or Configure-Reject.
 *
 * Returns:
 *	0 - Nak was bad.
 *	1 - Nak was good.
 */
static int
acscp_nakci(fsm *f, u_char *p, int len)
{
    acscp_options *go = &acscp_gotoptions[0];
    u_char citype, cilen;
    u_int32_t ciroutes_vers, cidomains_vers, l;
    acscp_options no;		/* options we've seen Naks for */
    acscp_options try;		/* options to request next time */

    BZERO(&no, sizeof(no));
    try = *go;

    /*
     * Any Nak'd CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */

#define NAKCIROUTES(opt, neg, code) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_ROUTES) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETLONG(l, p); \
	ciroutes_vers = l; \
	no.neg = 1; \
	code \
    }

#define NAKCIDOMAINS(opt, neg, code) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_DOMAINS) && \
	len >= cilen && \
	p[0] == opt) { \
	len -= cilen; \
	INCPTR(2, p); \
	GETLONG(l, p); \
	cidomains_vers = l; \
	no.neg = 1; \
	code \
    }

    NAKCIROUTES(CI_ROUTES, neg_routes,
            if (ciroutes_vers <= LATEST_ROUTES_VERSION)
                try.routes_version = ciroutes_vers;
            else
                try.routes_version = go->routes_version;
	    );

    NAKCIDOMAINS(CI_DOMAINS, neg_domains,
            if (cidomains_vers <= LATEST_DOMAINS_VERSION)
                try.domains_version = cidomains_vers;
            else
                try.domains_version = go->domains_version;
	    );

    /*
     * There may be remaining CIs, if the peer is requesting negotiation
     * on an option that we didn't include in our request packet.
     * We refuse them all.
     */
    while (len > CILEN_VOID) {
	GETCHAR(citype, p);
	GETCHAR(cilen, p);
	if( (len -= cilen) < 0 )
	    goto bad;
	p = p + cilen - 2;
    }

    /*
     * OK, the Nak is good.  Now we can update state.
     * If there are any remaining options, we ignore them.
     */
    if (f->state != OPENED)
	*go = try;

    return 1;

bad:
    ACSCPDEBUG(("acscp_nakci: received bad Nak!"));
    return 0;
}


/*
 * acscp_rejci - Reject some of our CIs.
 * Callback from fsm_rconfnakrej.
 */
static int
acscp_rejci(fsm *f, u_char *p, int len)
{
    acscp_options *go = &acscp_gotoptions[0];
    u_char cilen;
    u_int32_t cilong;
    acscp_options try;		/* options to request next time */

    try = *go;
    /*
     * Any Rejected CIs must be in exactly the same order that we sent.
     * Check packet length and CI length at each step.
     * If we find any deviations, then this packet is bad.
     */

#define REJCIROUTES(opt, neg, vers) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_ROUTES) && \
	len >= cilen && \
	p[0] == opt) { \
	u_int32_t l; \
	len -= cilen; \
	INCPTR(2, p); \
	GETLONG(l, p); \
	cilong = l; \
	/* Check rejected value. */ \
	if (cilong != vers) \
	    goto bad; \
	try.neg = 0; \
    }

#define REJCIDOMAINS(opt, neg, vers) \
    if (go->neg && \
	((cilen = p[1]) == CILEN_DOMAINS) && \
	len >= cilen && \
	p[0] == opt) { \
	u_int32_t l; \
	len -= cilen; \
	INCPTR(2, p); \
	GETLONG(l, p); \
	cilong = l; \
	/* Check rejected value. */ \
	if (cilong != vers) \
	    goto bad; \
	try.neg = 0; \
    }

    REJCIROUTES(CI_ROUTES, neg_routes, go->routes_version);

    REJCIDOMAINS(CI_DOMAINS, neg_domains, go->domains_version);

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
    ACSCPDEBUG(("acscp_rejci: received bad Reject!"));
    return 0;
}


/*
 * acscp_reqci - Check the peer's requested CIs and send appropriate response.
 * Callback from fsm_rconfreq, Receive Configure Request
 *
 * Returns: CONFACK, CONFNAK or CONFREJ and input packet modified
 * appropriately.  If reject_if_disagree is non-zero, doesn't return
 * CONFNAK; returns CONFREJ if it can't return CONFACK.
 */
static int
acscp_reqci(fsm *f, u_char *inp, int *len, int reject_if_disagree)
{
    acscp_options *ho = &acscp_hisoptions[0];
    acscp_options *ao = &acscp_allowoptions[0];
    u_char *cip, *next;		/* Pointer to current and next CIs */
    u_short cilen, citype;	/* Parsed len, type */
    u_int32_t tl;		/* Parsed version values */
    int rc = CONFACK;		/* Final packet return code */
    int orc;			/* Individual option return code */
    u_char *p;			/* Pointer to next char to parse */
    u_char *ucp = inp;		/* Pointer to current output char */
    int l = *len;		/* Length left */
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
	    ACSCPDEBUG(("acscp_reqci: bad CI length!"));
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

	case CI_ROUTES:
	    /* ROUTES request */
	    d = citype == CI_ROUTES;

	    /* check that we support the version requested */
	    if (ao->neg_routes == 0 ||
		cilen != CILEN_ROUTES) {	/* Check CI length */
		orc = CONFREJ;			/* Reject CI */
		break;
	    }
	    GETLONG(tl, p);
	    if (tl > ao->routes_version) {
                DECPTR(sizeof(u_int32_t), p);
		PUTLONG(ao->routes_version, p);
		orc = CONFNAK;
                break;
            }
            ho->neg_routes = 1;
            break;

	case CI_DOMAINS:
	    /* DOMAINS request */
	    d = citype == CI_DOMAINS;

	    /* check that we support the version requested */
	    if (ao->neg_domains == 0 ||
		cilen != CILEN_DOMAINS) {	/* Check CI length */
		orc = CONFREJ;			/* Reject CI */
		break;
	    }
	    GETLONG(tl, p);
	    if (tl > ao->domains_version) {
                DECPTR(sizeof(u_int32_t), p);
		PUTLONG(ao->domains_version, p);
		orc = CONFNAK;
                break;
            }
            ho->neg_domains = 1;
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

    *len = ucp - inp;			/* Compute output length */
    ACSCPDEBUG(("acscp: returning Configure-%s", CODENAME(rc)));
    return (rc);			/* Return final code */
}

/*
 * acscp_up - acscp has come UP.
 *
 */
static void
acscp_up(fsm *f)
{
    int		mtu;
        
    mtu = netif_get_mtu(f->unit);
    ACSCPDEBUG(("acscp: up"));
    notify(acsp_up_notifier, 0);
    if (acsp_up_hook)
        acsp_up_hook();
    acsp_start(mtu);
}


/*
 * acscp_down - acscp has gone DOWN.
 *
 * Take the IP network interface down, clear its addresses
 * and delete routes through it.
 */
static void
acscp_down(fsm *f)
{
    
    ACSCPDEBUG(("acscp: down"));
    notify(acsp_down_notifier, 0);
    acsp_stop();
    if (acsp_down_hook)
        acsp_down_hook();
}


/*
 * acscp_finished - possibly shut down the lower layers.
 */
static void
acscp_finished(f)
    fsm *f;
{
	if (acscp_is_open) {
		acscp_is_open = 0;
        np_finished(f->unit, PPP_ACSP);
    }
}

/*
 * acscp_printpkt - print the contents of an acscp packet.
 */
static char *acscp_codenames[] = {
    "ConfReq", "ConfAck", "ConfNak", "ConfRej",
    "TermReq", "TermAck", "CodeRej"
};

static int
acscp_printpkt(p, plen, printer, arg)
    u_char *p;
    int plen;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
    int code, id, len, olen;
    u_char *pstart, *optend;
    u_int32_t cilong;

    if (plen < HEADERLEN)
	return 0;
    pstart = p;
    GETCHAR(code, p);
    GETCHAR(id, p);
    GETSHORT(len, p);
    if (len < HEADERLEN || len > plen)
	return 0;

    if (code >= 1 && code <= sizeof(acscp_codenames) / sizeof(char *))
	printer(arg, " %s", acscp_codenames[code-1]);
    else
	printer(arg, " code=0x%x", code);
    printer(arg, " id=0x%x", id);
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
	    printer(arg, " <");
	    len -= olen;
	    optend = p + olen;
	    switch (code) {
	    case CI_ROUTES:
	        p += 2;
		GETLONG(cilong, p); // version
		printer(arg, "route vers %d", htonl(cilong));
		break;
	    case CI_DOMAINS:
	        p += 2;
		GETLONG(cilong, p);
		printer(arg, "domain vers %d", htonl(cilong));
		break;
	    }
	    while (p < optend) {
		GETCHAR(code, p);
		printer(arg, " %.2x", code);
	    }
	    printer(arg, ">");
	}
	break;

    case TERMACK:
    case TERMREQ:
	if (len > 0 && *p >= ' ' && *p < 0x7f) {
	    printer(arg, " ");
	    print_string((char *)p, len, printer, arg);
	    p += len;
	    len = 0;
	}
	break;
    }

    /* print the rest of the bytes in the packet */
    for (; len > 0; --len) {
	GETCHAR(code, p);
	printer(arg, " %.2x", code);
    }

    return p - pstart;
}



