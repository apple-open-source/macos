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
 * auth.c - PPP authentication and phase control.
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
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
#include "link.h"
#include "auth.h"

#ifdef CBCP_SUPPORT
#include "cbcp.h"
#endif
#include "pathnames.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

/* Bits in scan_authfile return value */
#define NONWILD_SERVER	1
#define NONWILD_CLIENT	2

#define ISWILD(word)	(word[0] == '*' && word[1] == 0)


/* Bits in auth_pending[] */
#define PAP_WITHPEER	1
#define PAP_PEER	2
#define CHAP_WITHPEER	4
#define CHAP_PEER	8

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

extern char *crypt __P((const char *, const char *));

/* Prototypes for procedures local to this file. */

static void network_phase(struct ppp *ppp);
static void check_idle(CFRunLoopTimerRef timer, void *);
static void connect_time_expired(CFRunLoopTimerRef timer, void *);
//static int  get_pap_passwd(struct ppp *ppp, char *);
//static int  have_pap_secret(int *);
//static int  have_chap_secret(char *, char *, int, int *);
static int  ip_addr_check(u_int32_t, struct permitted_ip *);
static void free_wordlist(struct wordlist *);
static void set_allowed_addrs(struct ppp *ppp, struct wordlist *addrs, struct wordlist *opts);
//static int  some_ip_ok(struct wordlist *);
//static int  setupapfile(char **);
//static int  set_noauth_addr(char **);
static int scan_authfile(struct ppp *ppp, FILE *f, char *client, char *server, char *secret, struct wordlist **addrs, struct wordlist **opts, char *filename);
int  getword __P((FILE *f, char *word, int *newlinep, char *filename));
                                /* Read a word from a file */


/* -----------------------------------------------------------------------------
* An Open on LCP has requested a change from Dead to Establish phase.
* Do what's necessary to bring the physical layer up.
----------------------------------------------------------------------------- */
void auth_link_required(struct ppp *ppp)
{
}

/* -----------------------------------------------------------------------------
* LCP has terminated the link; go to the Dead phase and take the
* physical layer down.
----------------------------------------------------------------------------- */
void auth_link_terminated(struct ppp *ppp)
{
    if (ppp->phase == PPP_IDLE)
	return;

    ppp_new_phase(ppp, PPP_TERMINATE);
    link_disconnect(ppp);
}

/* -----------------------------------------------------------------------------
* LCP has gone down; it will either die or try to re-establish.
----------------------------------------------------------------------------- */
void auth_link_down(struct ppp *ppp)
{
    int i;
    struct protent *protp;

    for (i = 0; (protp = ppp->protocols[i]) != NULL; ++i) {
        if (protp->protocol != PPP_LCP && protp->lowerdown != NULL)
	    (*protp->lowerdown)(ppp);
        if (protp->protocol < 0xC000 && protp->close != NULL)
	    (*protp->close)(ppp, "LCP down");
    }
    ppp->num_np_open = 0;
    ppp->num_np_up = 0;
    if (ppp->phase != PPP_IDLE) {
        ppp_new_phase(ppp, PPP_TERMINATE);
        // don't disconnect, as it could be a lcp renegociation (l2tp)
        //link_disconnect(ppp);
    }
}

/* -----------------------------------------------------------------------------
* The link is established.
* Proceed to the Dead, Authenticate or Network phase as appropriate.
----------------------------------------------------------------------------- */
void auth_link_established(struct ppp *ppp)
{
    int auth;
//    lcp_options *wo = &ppp->lcp_wantoptions;
    lcp_options *go = &ppp->lcp_gotoptions;
    lcp_options *ho = &ppp->lcp_hisoptions;
    int i;
    struct protent *protp;

    /*
     * Tell higher-level protocols that LCP is up.
     */
    for (i = 0; (protp = ppp->protocols[i]) != NULL; ++i)
        if (protp->protocol != PPP_LCP
	    && protp->lowerup != NULL)
	    (*protp->lowerup)(ppp);

    if (ppp->auth_required && !(go->neg_chap || go->neg_upap)) {
	/*
	 * We wanted the peer to authenticate itself, and it refused:
	 * if we have some address(es) it can use without auth, fine,
	 * otherwise treat it as though it authenticated with PAP using
	 * a username * of "" and a password of "".  If that's not OK,
	 * boot it out.
	 */
        warn(ppp, "peer refused to authenticate: terminating link");
        lcp_close(ppp, "peer refused to authenticate");
        ppp->status = PPP_ERR_AUTHFAILED;
        return;
#if 0
        if (ppp->noauth_addrs != NULL) {
	    set_allowed_addrs(ppp, ppp->noauth_addrs, NULL);
	} else if (!wo->neg_upap || !null_login(ppp)) {
            warn(ppp, "peer refused to authenticate: terminating link");
	    lcp_close(ppp, "peer refused to authenticate");
            ppp->status = EXIT_PEER_AUTH_FAILED;
	    return;
	}
#endif       
    }

    ppp_new_phase(ppp, PPP_AUTHENTICATE);
    auth = 0;
    if (go->neg_chap) {
	ChapAuthPeer(ppp, ppp->our_name, go->chap_mdtype);
	auth |= CHAP_PEER;
    } else if (go->neg_upap) {
	upap_authpeer(ppp);
	auth |= PAP_PEER;
    }
    if (ho->neg_chap) {
	ChapAuthWithPeer(ppp, ppp->user, ho->chap_mdtype);
	auth |= CHAP_WITHPEER;
    } else if (ho->neg_upap) {
#if 0
        if (passwd[0] == 0) {
	    passwd_from_file = 1;
	    if (!get_pap_passwd(ppp, ppp->passwd))
		error(ppp, "No secret found for PAP login");
	}
#endif
        upap_authwithpeer(ppp, ppp->user, ppp->passwd);
	auth |= PAP_WITHPEER;
    }
    ppp->auth_pending = auth;

    if (auth)
        ppp_new_event(ppp, PPP_EVT_AUTH_STARTED);

    if (!auth)
	network_phase(ppp);
}

/* -----------------------------------------------------------------------------
* Proceed to the network phase.
----------------------------------------------------------------------------- */
static void network_phase(struct ppp *ppp)
{
 //   lcp_options *go = &ppp->lcp_gotoptions;

#ifdef CBCP_SUPPORT
    /*
     * If we negotiated callback, do it now.
     */
    if (go->neg_cbcp) {
        ppp_new_phase(ppp, PPP_CALLBACK);
	(*cbcp_protent.open)(ppp);
	return;
    }
#endif

    auth_start_networks(ppp);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void auth_start_networks(struct ppp *ppp)
{
    int i;
    struct protent *protp;

    ppp_new_phase(ppp, PPP_NETWORK);

    for (i = 0; (protp = ppp->protocols[i]) != NULL; ++i) {
        if (protp->protocol < 0xC000
	    && protp->open != NULL) {
	    (*protp->open)(ppp);
	    if (protp->protocol != PPP_CCP)
		++ppp->num_np_open;
	}
    }

    if (ppp->num_np_open == 0)
	/* nothing to do */
	lcp_close(ppp, "No network protocols running");
}

/* -----------------------------------------------------------------------------
* The peer has failed to authenticate himself using `protocol'.
----------------------------------------------------------------------------- */
void auth_peer_fail(struct ppp *ppp, int protocol)
{
    /*
     * Authentication failure: take the link down
     */
    lcp_close(ppp, "Authentication failed");
//    ppp->status = EXIT_PEER_AUTH_FAILED;
    ppp->status = PPP_ERR_AUTHFAILED; // ??? maybe need a more specific server authe fail error
}

/* -----------------------------------------------------------------------------
* The peer has been successfully authenticated using `protocol'.
----------------------------------------------------------------------------- */
void auth_peer_success(struct ppp *ppp, int protocol, char *name, int namelen)
{
    int bit;

    switch (protocol) {
    case PPP_CHAP:
	bit = CHAP_PEER;
	break;
    case PPP_PAP:
	bit = PAP_PEER;
	break;
    default:
        warn(ppp, "auth_peer_success: unknown protocol %x", protocol);
	return;
    }

    /*
     * Save the authenticated name of the peer for later.
     */
    if (namelen > sizeof(ppp->peer_authname) - 1)
        namelen = sizeof(ppp->peer_authname) - 1;
    BCOPY(name, ppp->peer_authname, namelen);
    ppp->peer_authname[namelen] = 0;

    /*
     * If there is no more authentication still to be done,
     * proceed to the network (or callback) phase.
     */
    if ((ppp->auth_pending &= ~bit) == 0)
        network_phase(ppp);
}

/* -----------------------------------------------------------------------------
* We have failed to authenticate ourselves to the peer using `protocol'.
----------------------------------------------------------------------------- */
void auth_withpeer_fail(struct ppp *ppp, int protocol)
{

    ppp_new_event(ppp, PPP_EVT_AUTH_FAILED);

    if (ppp->passwd_from_file)
        BZERO(ppp->passwd, MAXSECRETLEN);
    /*
     * We've failed to authenticate ourselves to our peer.
     * Some servers keep sending CHAP challenges, but there
     * is no point in persisting without any way to get updated
     * authentication secrets.
     */
    lcp_close(ppp, "Failed to authenticate ourselves to peer");
    //ppp->status = EXIT_AUTH_TOPEER_FAILED;
    ppp->status = PPP_ERR_AUTHFAILED;
}

/* -----------------------------------------------------------------------------
* We have successfully authenticated ourselves with the peer using `protocol'.
----------------------------------------------------------------------------- */
void auth_withpeer_success(struct ppp *ppp, int protocol)
{
    int bit;

    ppp_new_event(ppp, PPP_EVT_AUTH_SUCCEDED);

    switch (protocol) {
    case PPP_CHAP:
	bit = CHAP_WITHPEER;
	break;
    case PPP_PAP:
        if (ppp->passwd_from_file)
            BZERO(ppp->passwd, MAXSECRETLEN);
	bit = PAP_WITHPEER;
	break;
    default:
        warn(ppp, "auth_withpeer_success: unknown protocol %x", protocol);
	bit = 0;
    }

    /*
     * If there is no more authentication still being done,
     * proceed to the network (or callback) phase.
     */
    if ((ppp->auth_pending &= ~bit) == 0)
	network_phase(ppp);
}

/* -----------------------------------------------------------------------------
* np_up - a network protocol has come up.
----------------------------------------------------------------------------- */
void auth_np_up(struct ppp *ppp, int proto)
{
    int tlim;
    struct timeval 	tval;
    struct timezone 	tzone;

    if (ppp->num_np_up == 0) {
	/*
	 * At this point we consider that the link has come up successfully.
	 */
        //ppp->status = EXIT_OK;
        ppp->unsuccess = 0;
        if (!gettimeofday(&tval, &tzone)) {
            ppp->conntime = tval.tv_sec;
        }
        ppp_new_phase(ppp, PPP_RUNNING);

        tlim = ppp->link_idle_timer;
	if (tlim > 0)
            ppp->auth_idleTORef = AddTimerToRunLoop(check_idle, ppp, tlim); 

	/*
	 * Set a timeout to close the connection once the maximum
	 * connect time has expired.
	 */
        if (ppp->link_session_timer > 0)
            ppp->auth_sessionTORef = AddTimerToRunLoop(connect_time_expired, ppp, ppp->link_session_timer); 
    }
    ++ppp->num_np_up;
}

/* -----------------------------------------------------------------------------
* np_down - a network protocol has gone down.
----------------------------------------------------------------------------- */
void auth_np_down(struct ppp *ppp, int proto)
{
    if (--ppp->num_np_up == 0) {
        DelTimerFromRunLoop(&(ppp->auth_idleTORef)); /* Cancel timeout */
        DelTimerFromRunLoop(&(ppp->auth_sessionTORef)); /* Cancel timeout */
        ppp_new_phase(ppp, PPP_NETWORK);
    }
}

 /* -----------------------------------------------------------------------------
 * np_finished - a network protocol has finished using the link.
 ----------------------------------------------------------------------------- */
void auth_np_finished(struct ppp *ppp, int proto)
{
    if (--ppp->num_np_open <= 0) {
	/* no further use for the link: shut up shop. */
	lcp_close(ppp, "No network protocols running");
    }
}

/* -----------------------------------------------------------------------------
* check_idle - check whether the link has been idle for long
* enough that we can shut it down.
----------------------------------------------------------------------------- */
static void check_idle(CFRunLoopTimerRef timer, void *arg)
{
    struct ppp 		*ppp = (struct ppp *)arg;
    struct ppp_idle idle;
    time_t itime;
    int tlim;

    if (ppp_get_idle_time(ppp, &idle))
	return;
	itime = MIN(idle.xmit_idle, idle.recv_idle);
	tlim = ppp->link_idle_timer - itime;
    if (tlim <= 0) {
	/* link is idle: shut it down. */
	notice(ppp, "Terminating connection due to lack of activity.");
	lcp_close(ppp, "Link inactive");
	//ppp->need_holdoff = 0;
        //ppp->status = EXIT_IDLE_TIMEOUT;
        ppp->status = PPP_ERR_IDLETIMEOUT;
    } else {
        ppp->auth_idleTORef = AddTimerToRunLoop(check_idle, ppp, tlim); 
    }
}

/* -----------------------------------------------------------------------------
* connect_time_expired - log a message and close the connection.
----------------------------------------------------------------------------- */
static void connect_time_expired(CFRunLoopTimerRef timer, void *arg)
{
    struct ppp 		*ppp = (struct ppp *)arg;

    //info("Connect time expired"); // could generate an event ?
    
    lcp_close(ppp, "Connect time expired");	/* Close connection */
    ppp->status = PPP_ERR_SESSIONTIMEOUT;
    //ppp->status = EXIT_CONNECT_TIME;
}

/* -----------------------------------------------------------------------------
* auth_reset - called when LCP is starting negotiations to recheck
* authentication options, i.e. whether we have appropriate secrets
* to use for authenticating ourselves and/or the peer.
----------------------------------------------------------------------------- */
void auth_reset(struct ppp *ppp)
{
    lcp_options *ao = &ppp->lcp_allowoptions;

    ao->neg_upap = 1;
    ao->neg_chap = 1;
}

/* -----------------------------------------------------------------------------
* check_passwd - Check the user name and passwd against the PAP secrets
* file.  If requested, also check against the system password database,
* and login the user if OK.
*
* returns:
*	UPAP_AUTHNAK: Authentication failed.
*	UPAP_AUTHACK: Authentication succeeded.
* In either case, msg points to an appropriate message.
----------------------------------------------------------------------------- */
int check_passwd(struct ppp *ppp, char *auser, int userlen, char *apasswd, int passwdlen, char **msg)
{
    int ret = 0;
    char *filename;
    FILE *f;
    struct wordlist *addrs = NULL, *opts = NULL;
    char passwd[256], user[256];
    char secret[MAXWORDLEN];

    /*
     * Make copies of apasswd and auser, then null-terminate them.
     * If there are unprintable characters in the password, make them visible.
     */
    slprintf(ppp, passwd, sizeof(passwd), "%.*v", passwdlen, apasswd);
    slprintf(ppp, user, sizeof(user), "%.*v", userlen, auser);
    *msg = "";

    /*
     * Open the file of pap secrets and scan for a suitable secret
     * for authenticating this user.
     */
    filename = PATH_SECRETS;
    addrs = opts = NULL;
    ret = UPAP_AUTHNAK;
    f = fopen(filename, "r");
    if (f == NULL) {
	error(ppp, "Can't open PAP password file %s: %m", filename);

    } else {
	if (scan_authfile(ppp, f, user, ppp->our_name, secret, &addrs, &opts, filename) < 0) {
            warn(ppp, "no PAP secret found for %s", user);
	} else if (secret[0] != 0) {
	    /* password given in pap-secrets - must match */
	    if (strcmp(passwd, secret) == 0)
		ret = UPAP_AUTHACK;
	    else
                warn(ppp, "PAP authentication failure for %s", user);
	} else {
	    /* empty password in pap-secrets and login option not used */
	    ret = UPAP_AUTHACK;
	}
	fclose(f);
    }

    if (ret == UPAP_AUTHNAK) {
        if (**msg == 0)
	    *msg = "Login incorrect";
	/*
	 * XXX can we ever get here more than once??
	 * Frustrate passwd stealer programs.
	 * Allow 10 tries, but start backing off after 3 (stolen from login).
	 * On 10'th, drop the connection.
	 */
	if (ppp->upap_attempts++ >= 10) {
            warn(ppp, "%d LOGIN FAILURES ON %s, %s", ppp->upap_attempts, ppp->devnam, user);
            lcp_close(ppp, "login failed");
	}
        //if (ppp->upap_attempts > 3)
        //    sleep((u_int) (ppp->upap_attempts - 3) * 5);
	if (opts != NULL)
	    free_wordlist(opts);

    } else {
        ppp->upap_attempts = 0;			/* Reset count */
	if (**msg == 0)
	    *msg = "Login ok";
	set_allowed_addrs(ppp, addrs, opts);
    }

    if (addrs != NULL)
	free_wordlist(addrs);
    BZERO(passwd, sizeof(passwd));
    BZERO(secret, sizeof(secret));

    return ret;
}
#if 0
/* -----------------------------------------------------------------------------
* null_login - Check if a username of "" and a password of "" are
* acceptable, and iff so, set the list of acceptable IP addresses
* and return 1.
----------------------------------------------------------------------------- */
static int null_login(struct ppp *ppp)
{
    char *filename;
    FILE *f;
    int i, ret;
    struct wordlist *addrs, *opts;
    char secret[MAXWORDLEN];

    /*
     * Open the file of pap secrets and scan for a suitable secret.
     */
    filename = _PATH_UPAPFILE;
    addrs = NULL;
    f = fopen(filename, "r");
    if (f == NULL)
	return 0;

    i = scan_authfile(ppp, f, "", our_name, secret, &addrs, &opts, filename);
    ret = i >= 0 && secret[0] == 0;
    BZERO(secret, sizeof(secret));

    if (ret)
	set_allowed_addrs(ppp, addrs, opts);
    else if (opts != 0)
	free_wordlist(opts);
    if (addrs != 0)
	free_wordlist(addrs);

    fclose(f);
    return ret;
}
#endif


/* -----------------------------------------------------------------------------
* set_allowed_addrs() - set the list of allowed addresses.
* Also looks for `--' indicating options to apply for this peer
* and leaves the following words in extra_options.
----------------------------------------------------------------------------- */
static void set_allowed_addrs(struct ppp *ppp, struct wordlist *addrs, struct wordlist *opts)
{
    int n;
    struct wordlist *ap, **pap;
    struct permitted_ip *ip;
    char *ptr_word, *ptr_mask;
    struct hostent *hp;
    struct netent *np;
    u_int32_t a, mask, ah, offset;
    struct ipcp_options *wo = &ppp->ipcp_wantoptions;
    u_int32_t suggested_ip = 0;

    if (ppp->addresses != NULL)
        free(ppp->addresses);
    ppp->addresses = NULL;
    if (ppp->extra_options != NULL)
	free_wordlist(ppp->extra_options);
    ppp->extra_options = opts;

    /*
     * Count the number of IP addresses given.
     */
    for (n = 0, pap = &addrs; (ap = *pap) != NULL; pap = &ap->next)
	++n;
    if (n == 0)
	return;
    ip = (struct permitted_ip *) malloc((n + 1) * sizeof(struct permitted_ip));
    if (ip == 0)
	return;

    n = 0;
    for (ap = addrs; ap != NULL; ap = ap->next) {
	/* "-" means no addresses authorized, "*" means any address allowed */
	ptr_word = ap->word;
	if (strcmp(ptr_word, "-") == 0)
	    break;
	if (strcmp(ptr_word, "*") == 0) {
	    ip[n].permit = 1;
	    ip[n].base = ip[n].mask = 0;
	    ++n;
	    break;
	}

	ip[n].permit = 1;
	if (*ptr_word == '!') {
	    ip[n].permit = 0;
	    ++ptr_word;
	}

	mask = ~ (u_int32_t) 0;
	offset = 0;
	ptr_mask = strchr (ptr_word, '/');
	if (ptr_mask != NULL) {
	    int bit_count;
	    char *endp;

	    bit_count = (int) strtol (ptr_mask+1, &endp, 10);
	    if (bit_count <= 0 || bit_count > 32) {
                warn(ppp, "invalid address length %v in auth. address list",
		     ptr_mask+1);
		continue;
	    }
	    bit_count = 32 - bit_count;	/* # bits in host part */
	    if (*endp == '+') {
		offset = ppp->unit + 1;
		++endp;
	    }
	    if (*endp != 0) {
                warn(ppp, "invalid address length syntax: %v", ptr_mask+1);
		continue;
	    }
	    *ptr_mask = '\0';
	    mask <<= bit_count;
	}

	hp = gethostbyname(ptr_word);
	if (hp != NULL && hp->h_addrtype == AF_INET) {
	    a = *(u_int32_t *)hp->h_addr;
	} else {
	    np = getnetbyname (ptr_word);
	    if (np != NULL && np->n_addrtype == AF_INET) {
		a = htonl (*(u_int32_t *)np->n_net);
		if (ptr_mask == NULL) {
		    /* calculate appropriate mask for net */
		    ah = ntohl(a);
		    if (IN_CLASSA(ah))
			mask = IN_CLASSA_NET;
		    else if (IN_CLASSB(ah))
			mask = IN_CLASSB_NET;
		    else if (IN_CLASSC(ah))
			mask = IN_CLASSC_NET;
		}
	    } else {
		a = inet_addr (ptr_word);
	    }
	}

	if (ptr_mask != NULL)
	    *ptr_mask = '/';

	if (a == (u_int32_t)-1L) {
            warn(ppp, "unknown host %s in auth. address list", ap->word);
	    continue;
	}
	if (offset != 0) {
	    if (offset >= ~mask) {
                warn(ppp, "interface unit %d too large for subnet %v",
		     ppp->unit, ptr_word);
		continue;
	    }
	    a = htonl((ntohl(a) & mask) + offset);
	    mask = ~(u_int32_t)0;
	}
	ip[n].mask = htonl(mask);
	ip[n].base = a & ip[n].mask;
	++n;
	if (~mask == 0 && suggested_ip == 0)
	    suggested_ip = a;
    }

    ip[n].permit = 0;		/* make the last entry forbid all addresses */
    ip[n].base = 0;		/* to terminate the list */
    ip[n].mask = 0;

    ppp->addresses = ip;

    /*
     * If the address given for the peer isn't authorized, or if
     * the user hasn't given one, AND there is an authorized address
     * which is a single host, then use that if we find one.
     */
    if (suggested_ip != 0
	&& (wo->hisaddr == 0 || !auth_ip_addr(ppp, wo->hisaddr)))
	wo->hisaddr = suggested_ip;
}

/* -----------------------------------------------------------------------------
* auth_ip_addr - check whether the peer is authorized to use
* a given IP address.  Returns 1 if authorized, 0 otherwise.
----------------------------------------------------------------------------- */
int auth_ip_addr(struct ppp *ppp, u_int32_t addr)
{
    int ok;
    /* don't allow loopback or multicast address */
    if (bad_ip_adrs(addr))
	return 0;

   if (ppp->addresses != NULL) {
        ok = ip_addr_check(addr, ppp->addresses);
	if (ok >= 0)
	    return ok;
    }
    if (ppp->auth_required)
	return 0;		/* no addresses authorized */

    return ppp->allow_any_ip || !have_route_to(addr);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int ip_addr_check(u_int32_t addr, struct permitted_ip * addrs)
{
    for (; ; ++addrs)
	if ((addr & addrs->mask) == addrs->base)
	    return addrs->permit;
}

/* -----------------------------------------------------------------------------
* get_secret - open the CHAP secret file and return the secret
* for authenticating the given client on the given server.
* (We could be either client or server).
----------------------------------------------------------------------------- */
int get_secret(struct ppp *ppp, char *client, char *server, char *secret, int *secret_len, int am_server)
{
    FILE *f;
    int ret, len;
    char *filename;
    struct wordlist *addrs, *opts;
    char secbuf[MAXWORDLEN];

    if (!am_server && ppp->passwd[0] != 0) {
        strlcpy(secbuf, ppp->passwd, sizeof(secbuf));
    }
    else {
        filename = PATH_SECRETS;
        addrs = NULL;
        secbuf[0] = 0;

        f = fopen(filename, "r");
        if (f == NULL) {
            error(ppp, "Can't open chap secret file %s: %m", filename);
            return 0;
        }

        ret = scan_authfile(ppp, f, client, server, secbuf, &addrs, &opts, filename);
        fclose(f);
        if (ret < 0)
            return 0;

        if (am_server)
            set_allowed_addrs(ppp, addrs, opts);
        else if (opts != 0)
            free_wordlist(opts);
        if (addrs != 0)
            free_wordlist(addrs);
    }

    len = strlen(secbuf);
    if (len > MAXSECRETLEN) {
        error(ppp, "Secret for %s on %s is too long", client, server);
        len = MAXSECRETLEN;
    }
    BCOPY(secbuf, secret, len);
    BZERO(secbuf, sizeof(secbuf));
    *secret_len = len;

    return 1;
}

/* -----------------------------------------------------------------------------
* bad_ip_adrs - return 1 if the IP address is one we don't want
* to use, such as an address in the loopback net or a multicast address.
* addr is in network byte order.
----------------------------------------------------------------------------- */
int bad_ip_adrs(u_int32_t addr)
{
    addr = ntohl(addr);
    return (addr >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET
	|| IN_MULTICAST(addr) || IN_BADCLASS(addr);
}

#if 0
/* -----------------------------------------------------------------------------
* some_ip_ok - check a wordlist to see if it authorizes any
* IP address(es).
----------------------------------------------------------------------------- */
static int some_ip_ok(struct wordlist *addrs)
{
    for (; addrs != 0; addrs = addrs->next) {
	if (addrs->word[0] == '-')
	    break;
	if (addrs->word[0] != '!')
	    return 1;		/* some IP address is allowed */
    }
    return 0;
}

#endif

/* -----------------------------------------------------------------------------
* free_wordlist - release memory allocated for a wordlist.
----------------------------------------------------------------------------- */
static void free_wordlist(struct wordlist *wp)
{
    struct wordlist *next;

    while (wp != NULL) {
        next = wp->next;
        free(wp);
        wp = next;
    }
}

/* -----------------------------------------------------------------------------
 * scan_authfile - Scan an authorization file for a secret suitable
 * for authenticating `client' on `server'.  The return value is -1
 * if no secret is found, otherwise >= 0.  The return value has
 * NONWILD_CLIENT set if the secret didn't have "*" for the client, and
 * NONWILD_SERVER set if the secret didn't have "*" for the server.
 * Any following words on the line up to a "--" (i.e. address authorization
 * info) are placed in a wordlist and returned in *addrs.  Any
 * following words (extra options) are placed in a wordlist and
 * returned in *opts.
 * We assume secret is NULL or points to MAXWORDLEN bytes of space.
----------------------------------------------------------------------------- */
int scan_authfile(struct ppp *ppp, FILE *f,
                  char *client, char *server, char *secret,
                  struct wordlist **addrs, struct wordlist **opts,
                  char *filename)
{
    int newline, xxx;
    int got_flag, best_flag;
    
    FILE *sf;
    struct wordlist *ap, *addr_list, *alist, **app;
    char word[MAXWORDLEN];
    char atfile[MAXWORDLEN];
    char lsecret[MAXWORDLEN];

    if (addrs != NULL)
	*addrs = NULL;
    if (opts != NULL)
	*opts = NULL;
    addr_list = NULL;
    if (!getword(f, word, &newline, filename))
	return -1;		/* file is empty??? */
    newline = 1;
    best_flag = -1;

    // should change this algo....
    for (;;) {
	/*
	 * Skip until we find a word at the start of a line.
	 */
	while (!newline && getword(f, word, &newline, filename))
	    ;
        if (!newline)
            break;		/* got to end of file */

	/*
	 * Got a client - check if it's a match or a wildcard.
	 */
	got_flag = 0;
	if (client != NULL && strcmp(word, client) != 0 && !ISWILD(word)) {
	    newline = 0;
	    continue;
	}
	if (!ISWILD(word))
	    got_flag = NONWILD_CLIENT;

#if 1
        /*
	 * Now get a server and check if it matches.
	 */
	if (!getword(f, word, &newline, filename))
	    break;
	if (newline)
	    continue;
	if (!ISWILD(word)) {
	    if (server != NULL && strcmp(word, server) != 0)
		continue;
	    got_flag |= NONWILD_SERVER;
	}
#else
        got_flag |= NONWILD_SERVER;
#endif
	/*
	 * Got some sort of a match - see if it's better than what
	 * we have already.
	 */
	if (got_flag <= best_flag)
	    continue;

	/*
	 * Get the secret.
	 */
	if (!getword(f, word, &newline, filename))
	    break;
	if (newline)
	    continue;

	/*
	 * Special syntax: @filename means read secret from file.
	 */
	if (word[0] == '@') {
	    strlcpy(atfile, word+1, sizeof(atfile));
	    if ((sf = fopen(atfile, "r")) == NULL) {
		warn(ppp, "can't open indirect secret file %s", atfile);
		continue;
	    }
	    //check_access(sf, atfile);
	    if (!getword(sf, word, &xxx, atfile)) {
		warn(ppp, "no secret in indirect secret file %s", atfile);
		fclose(sf);
		continue;
	    }
	    fclose(sf);
	}
	if (secret != NULL)
	    strlcpy(lsecret, word, sizeof(lsecret));

	/*
	 * Now read address authorization info and make a wordlist.
	 */
	app = &alist;
	for (;;) {
	    if (!getword(f, word, &newline, filename) || newline)
		break;
	    ap = (struct wordlist *) malloc(sizeof(struct wordlist));
            if (ap == NULL) {
                //novm("authorized addresses");
                // humm...
                return 0;
            }
	    ap->word = strdup(word);
	    if (ap->word == NULL) {
                //novm("authorized addresses");
                // humm...
                return 0;
            }
	    *app = ap;
	    app = &ap->next;
	}
	*app = NULL;

	/*
	 * This is the best so far; remember it.
	 */
	best_flag = got_flag;
	if (addr_list)
	    free_wordlist(addr_list);
	addr_list = alist;
	if (secret != NULL)
	    strlcpy(secret, lsecret, MAXWORDLEN);

	if (!newline)
	    break;
    }
        
    /* scan for a -- word indicating the start of options */
    for (app = &addr_list; (ap = *app) != NULL; app = &ap->next)
	if (strcmp(ap->word, "--") == 0)
	    break;
    /* ap = start of options */
    if (ap != NULL) {
	ap = ap->next;		/* first option */
	free(*app);			/* free the "--" word */
	*app = NULL;		/* terminate addr list */
    }
    if (opts != NULL)
	*opts = ap;
    else if (ap != NULL)
	free_wordlist(ap);
    if (addrs != NULL)
	*addrs = addr_list;
    else if (addr_list != NULL)
	free_wordlist(addr_list);

    return best_flag;
}

/*
 * Read a word from a file.
 * Words are delimited by white-space or by quotes (" or ').
 * Quotes, white-space and \ may be escaped with \.
 * \<newline> is ignored.
 */
int
getword(f, word, newlinep, filename)
    FILE *f;
    char *word;
    int *newlinep;
    char *filename;
{
    int c, len, escape;
    int quoted, comment;
    int value, digit, got, n;

#define isoctal(c) ((c) >= '0' && (c) < '8')

    *newlinep = 0;
    len = 0;
    escape = 0;
    comment = 0;
    /*
     * First skip white-space and comments.
     */
    for (;;) {
        c = getc(f);
        if (c == EOF)
            break;

        /*
         * A newline means the end of a comment; backslash-newline
         * is ignored.  Note that we cannot have escape && comment.
         */
        if (c == '\n') {
            if (!escape) {
                *newlinep = 1;
                comment = 0;
            } else
                escape = 0;
            continue;
        }

        /*
         * Ignore characters other than newline in a comment.
         */
        if (comment)
            continue;

        /*
         * If this character is escaped, we have a word start.
         */
        if (escape)
            break;

        /*
         * If this is the escape character, look at the next character.
         */
        if (c == '\\') {
            escape = 1;
            continue;
        }

        /*
         * If this is the start of a comment, ignore the rest of the line.
         */
        if (c == '#') {
            comment = 1;
            continue;
        }

        /*
         * A non-whitespace character is the start of a word.
         */
        if (!isspace(c))
            break;
    }

    /*
     * Save the delimiter for quoted strings.
     */
    if (!escape && (c == '"' || c == '\'')) {
        quoted = c;
        c = getc(f);
    } else
        quoted = 0;

    /*
     * Process characters until the end of the word.
     */
    while (c != EOF) {
        if (escape) {
            /*
             * This character is escaped: backslash-newline is ignored,
             * various other characters indicate particular values
             * as for C backslash-escapes.
             */
            escape = 0;
            if (c == '\n') {
                c = getc(f);
                continue;
            }

            got = 0;
            switch (c) {
            case 'a':
                value = '\a';
                break;
            case 'b':
                value = '\b';
                break;
            case 'f':
                value = '\f';
                break;
            case 'n':
                value = '\n';
                break;
            case 'r':
                value = '\r';
                break;
            case 's':
                value = ' ';
                break;
            case 't':
                value = '\t';
                break;

            default:
                if (isoctal(c)) {
                    /*
                     * \ddd octal sequence
                     */
                    value = 0;
                    for (n = 0; n < 3 && isoctal(c); ++n) {
                        value = (value << 3) + (c & 07);
                        c = getc(f);
                    }
                    got = 1;
                    break;
                }

                if (c == 'x') {
                    /*
                     * \x<hex_string> sequence
                     */
                    value = 0;
                    c = getc(f);
                    for (n = 0; n < 2 && isxdigit(c); ++n) {
                        digit = toupper(c) - '0';
                        if (digit > 10)
                            digit += '0' + 10 - 'A';
                        value = (value << 4) + digit;
                        c = getc (f);
                    }
                    got = 1;
                    break;
                }

                /*
                 * Otherwise the character stands for itself.
                 */
                value = c;
                break;
            }

            /*
             * Store the resulting character for the escape sequence.
             */
            if (len < MAXWORDLEN-1)
                word[len] = value;
            ++len;

            if (!got)
                c = getc(f);
            continue;

        }

        /*
         * Not escaped: see if we've reached the end of the word.
         */
        if (quoted) {
            if (c == quoted)
                break;
        } else {
            if (isspace(c) || c == '#') {
                ungetc (c, f);
                break;
            }
        }

        /*
         * Backslash starts an escape sequence.
         */
        if (c == '\\') {
            escape = 1;
            c = getc(f);
            continue;
        }

        /*
         * An ordinary character: store it in the word and get another.
         */
        if (len < MAXWORDLEN-1)
            word[len] = c;
        ++len;

        c = getc(f);
    }

    /*
     * End of the word: check for errors.
     */
    if (c == EOF) {
        if (ferror(f)) {
            if (errno == 0)
                errno = EIO;
            //option_error("Error reading %s: %m", filename);
            //die(1);
            return 0;
        }
        /*
         * If len is zero, then we didn't find a word before the
         * end of the file.
         */
        if (len == 0)
            return 0;
    }

    /*
     * Warn if the word was too long, and append a terminating null.
     */
    if (len >= MAXWORDLEN) {
        //option_error("warning: word in file %s too long (%.20s...)",
         //            filename, word);
        len = MAXWORDLEN - 1;
    }
    word[len] = 0;

    return 1;

#undef isoctal

}

