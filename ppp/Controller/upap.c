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
 * upap.c - User/Password Authentication Protocol.
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
Includes
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
Definitions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Forward Declarations
----------------------------------------------------------------------------- */

static void upap_init __P((struct ppp *));
static void upap_lowerup __P((struct ppp *));
static void upap_lowerdown __P((struct ppp *));
static void upap_input __P((struct ppp *, u_char *, int));
static void upap_protrej __P((struct ppp *));
static int  upap_printpkt __P((struct ppp *ppp, u_char *, int,
			       void (*) __P((struct ppp *, void *, char *, ...)), void *));

static void upap_timeout __P((CFRunLoopTimerRef, void *));
static void upap_reqtimeout __P((CFRunLoopTimerRef, void *));
static void upap_rauthreq __P((struct ppp *, u_char *, int, int));
static void upap_rauthack __P((struct ppp *, u_char *, int, int));
static void upap_rauthnak __P((struct ppp *, u_char *, int, int));
static void upap_sauthreq __P((struct ppp *));
static void upap_sresp __P((struct ppp *, u_char, u_char, char *, int));

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

struct protent pap_protent = {
    PPP_PAP,
    upap_init,
    upap_input,
    upap_protrej,
    upap_lowerup,
    upap_lowerdown,
    NULL,
    NULL,
    upap_printpkt,
    NULL,
    "PAP",
    NULL,
    0, //pap_option_list,
    NULL,
    NULL,
    NULL
};


/* -----------------------------------------------------------------------------
* upap_init - Initialize a UPAP unit.
----------------------------------------------------------------------------- */
static void upap_init(struct ppp *ppp)
{
    upap_state *u = &ppp->upap;

    u->us_unit = ppp->unit;
    u->us_user = NULL;
    u->us_userlen = 0;
    u->us_passwd = NULL;
    u->us_passwdlen = 0;
    u->us_clientstate = UPAPCS_INITIAL;
    u->us_serverstate = UPAPSS_INITIAL;
    u->us_id = 0;
    u->us_timeouttime = UPAP_DEFTIMEOUT;
    u->us_maxtransmits = 10;
    u->us_reqtimeout = UPAP_DEFREQTIME;

    ppp->upap_attempts = 0;
    ppp->upap_reqTORef = 0;
    ppp->upap_TORef = 0;
        
}

/* -----------------------------------------------------------------------------
* upap_authwithpeer - Authenticate us with our peer (start client).
*
* Set new state and send authenticate's.
----------------------------------------------------------------------------- */
void upap_authwithpeer(struct ppp *ppp, char *user, char *password)
{
    upap_state *u = &ppp->upap;

    /* Save the username and password we're given */
    u->us_user = user;
    u->us_userlen = strlen(user);
    u->us_passwd = password;
    u->us_passwdlen = strlen(password);
    u->us_transmits = 0;

    /* Lower layer up yet? */
    if (u->us_clientstate == UPAPCS_INITIAL ||
	u->us_clientstate == UPAPCS_PENDING) {
	u->us_clientstate = UPAPCS_PENDING;
	return;
    }

    upap_sauthreq(ppp);			/* Start protocol */
}

/* -----------------------------------------------------------------------------
* upap_authpeer - Authenticate our peer (start server).
*
* Set new state.
----------------------------------------------------------------------------- */
void upap_authpeer(struct ppp *ppp)
{
    upap_state *u = &ppp->upap;

    /* Lower layer up yet? */
    if (u->us_serverstate == UPAPSS_INITIAL ||
	u->us_serverstate == UPAPSS_PENDING) {
	u->us_serverstate = UPAPSS_PENDING;
	return;
    }

    u->us_serverstate = UPAPSS_LISTEN;
    if (u->us_reqtimeout > 0)
        ppp->upap_reqTORef = AddTimerToRunLoop(upap_reqtimeout, ppp, u->us_reqtimeout);
}

/* -----------------------------------------------------------------------------
* upap_timeout - Retransmission timer for sending auth-reqs expired.
----------------------------------------------------------------------------- */
static void upap_timeout(CFRunLoopTimerRef timer, void *arg)
{
    struct ppp *ppp = (struct ppp *) arg;
    upap_state *u = &ppp->upap;

    if (u->us_clientstate != UPAPCS_AUTHREQ)
	return;

    if (u->us_transmits >= u->us_maxtransmits) {
	/* give up in disgust */
	error(ppp, "No response to PAP authenticate-requests");
	u->us_clientstate = UPAPCS_BADAUTH;
	auth_withpeer_fail(ppp, PPP_PAP);
	return;
    }

    upap_sauthreq(ppp);		/* Send Authenticate-Request */
}

/* -----------------------------------------------------------------------------
* upap_reqtimeout - Give up waiting for the peer to send an auth-req.
----------------------------------------------------------------------------- */
static void upap_reqtimeout(CFRunLoopTimerRef timer, void *arg)
{
    struct ppp *ppp = (struct ppp *) arg;
    upap_state *u = &ppp->upap;

    if (u->us_serverstate != UPAPSS_LISTEN)
	return;			/* huh?? */

    auth_peer_fail(ppp, PPP_PAP);
    u->us_serverstate = UPAPSS_BADAUTH;
}

/* -----------------------------------------------------------------------------
* upap_lowerup - The lower layer is up.
*
* Start authenticating if pending.
----------------------------------------------------------------------------- */
static void upap_lowerup(struct ppp *ppp)
{
    upap_state *u = &ppp->upap;

    if (u->us_clientstate == UPAPCS_INITIAL)
	u->us_clientstate = UPAPCS_CLOSED;
    else if (u->us_clientstate == UPAPCS_PENDING) {
	upap_sauthreq(ppp);	/* send an auth-request */
    }

    if (u->us_serverstate == UPAPSS_INITIAL)
	u->us_serverstate = UPAPSS_CLOSED;
    else if (u->us_serverstate == UPAPSS_PENDING) {
	u->us_serverstate = UPAPSS_LISTEN;
	if (u->us_reqtimeout > 0)
            ppp->upap_reqTORef = AddTimerToRunLoop(upap_reqtimeout, ppp, u->us_reqtimeout);
    }
}

/* -----------------------------------------------------------------------------
* upap_lowerdown - The lower layer is down.
*
* Cancel all timeouts.
----------------------------------------------------------------------------- */
static void upap_lowerdown(struct ppp *ppp)
{
    upap_state *u = &ppp->upap;

    if (u->us_clientstate == UPAPCS_AUTHREQ)	/* Timeout pending? */
        DelTimerFromRunLoop(&ppp->upap_TORef);
    if (u->us_serverstate == UPAPSS_LISTEN && u->us_reqtimeout > 0)
        DelTimerFromRunLoop(&ppp->upap_reqTORef);

    u->us_clientstate = UPAPCS_INITIAL;
    u->us_serverstate = UPAPSS_INITIAL;
}


/* -----------------------------------------------------------------------------
* upap_protrej - Peer doesn't speak this protocol.
*
* This shouldn't happen.  In any case, pretend lower layer went down.
----------------------------------------------------------------------------- */
static void upap_protrej(struct ppp *ppp)
{
    upap_state *u = &ppp->upap;

    if (u->us_clientstate == UPAPCS_AUTHREQ) {
	error(ppp, "PAP authentication failed due to protocol-reject");
	auth_withpeer_fail(ppp, PPP_PAP);
    }
    if (u->us_serverstate == UPAPSS_LISTEN) {
	error(ppp, "PAP authentication of peer failed (protocol-reject)");
	auth_peer_fail(ppp, PPP_PAP);
    }
    upap_lowerdown(ppp);
}

/* -----------------------------------------------------------------------------
* upap_input - Input UPAP packet.
----------------------------------------------------------------------------- */
static void upap_input(struct ppp *ppp, u_char *inpacket, int l)
{
    u_char *inp;
    u_char code, id;
    int len;

    /*
     * Parse header (code, id and length).
     * If packet too short, drop it.
     */
    inp = inpacket;
    if (l < UPAP_HEADERLEN) {
	UPAPDEBUG(("pap_input: rcvd short header."));
	return;
    }
    GETCHAR(code, inp);
    GETCHAR(id, inp);
    GETSHORT(len, inp);
    if (len < UPAP_HEADERLEN) {
	UPAPDEBUG(("pap_input: rcvd illegal length."));
	return;
    }
    if (len > l) {
	UPAPDEBUG(("pap_input: rcvd short packet."));
	return;
    }
    len -= UPAP_HEADERLEN;

    /*
     * Action depends on code.
     */
    switch (code) {
    case UPAP_AUTHREQ:
	upap_rauthreq(ppp, inp, id, len);
	break;

    case UPAP_AUTHACK:
	upap_rauthack(ppp, inp, id, len);
	break;

    case UPAP_AUTHNAK:
	upap_rauthnak(ppp, inp, id, len);
	break;

    default:				/* XXX Need code reject */
	break;
    }
}

/* -----------------------------------------------------------------------------
* upap_rauth - Receive Authenticate.
----------------------------------------------------------------------------- */
static void upap_rauthreq(struct ppp *ppp, u_char *inp, int id, int len)
{
    upap_state *u = &ppp->upap;
    u_char ruserlen, rpasswdlen;
    char *ruser, *rpasswd;
    int retcode;
    char *msg;
    int msglen;

    if (u->us_serverstate < UPAPSS_LISTEN)
	return;

    /*
     * If we receive a duplicate authenticate-request, we are
     * supposed to return the same status as for the first request.
     */
    if (u->us_serverstate == UPAPSS_OPEN) {
	upap_sresp(ppp, UPAP_AUTHACK, id, "", 0);	/* return auth-ack */
	return;
    }
    if (u->us_serverstate == UPAPSS_BADAUTH) {
        upap_sresp(ppp, UPAP_AUTHNAK, id, "", 0);	/* return auth-nak */
	return;
    }

    /*
     * Parse user/passwd.
     */
    if (len < 1) {
	UPAPDEBUG(("pap_rauth: rcvd short packet."));
	return;
    }
    GETCHAR(ruserlen, inp);
    len -= sizeof (u_char) + ruserlen + sizeof (u_char);
    if (len < 0) {
        UPAPDEBUG(("pap_rauth: rcvd short packet."));
        return;
    }
    ruser = (char *) inp;
    INCPTR(ruserlen, inp);
    GETCHAR(rpasswdlen, inp);
    if (len < rpasswdlen) {
	UPAPDEBUG(("pap_rauth: rcvd short packet."));
	return;
    }
    rpasswd = (char *) inp;

    /*
     * Check the username and password given.
     */
    retcode = check_passwd(ppp, ruser, ruserlen, rpasswd,
			   rpasswdlen, &msg);
    BZERO(rpasswd, rpasswdlen);
    msglen = strlen(msg);
    if (msglen > 255)
	msglen = 255;

    upap_sresp(ppp, retcode, id, msg, msglen);

    if (retcode == UPAP_AUTHACK) {
	u->us_serverstate = UPAPSS_OPEN;
	auth_peer_success(ppp, PPP_PAP, ruser, ruserlen);
    } else {
	u->us_serverstate = UPAPSS_BADAUTH;
	auth_peer_fail(ppp, PPP_PAP);
    }

    if (u->us_reqtimeout > 0)
        DelTimerFromRunLoop(&ppp->upap_reqTORef);
}

/* -----------------------------------------------------------------------------
* upap_rauthack - Receive Authenticate-Ack.
----------------------------------------------------------------------------- */
static void upap_rauthack(struct ppp *ppp, u_char *inp, int id, int len)
{
    upap_state *u = &ppp->upap;
    u_char msglen;
    char *msg;

    if (u->us_clientstate != UPAPCS_AUTHREQ) /* XXX */
	return;

    /*
     * Parse message.
     */
    if (len < 1) {
	UPAPDEBUG(("pap_rauthack: ignoring missing msg-length."));
    } else {
	GETCHAR(msglen, inp);
	if (msglen > 0) {
	    len -= sizeof (u_char);
	    if (len < msglen) {
		UPAPDEBUG(("pap_rauthack: rcvd short packet."));
		return;
	    }
	    msg = (char *) inp;
	    PRINTMSG(ppp, msg, msglen);
	}
    }

    u->us_clientstate = UPAPCS_OPEN;

    auth_withpeer_success(ppp, PPP_PAP);
}

/* -----------------------------------------------------------------------------
* upap_rauthnak - Receive Authenticate-Nakk.
----------------------------------------------------------------------------- */
static void upap_rauthnak(struct ppp *ppp, u_char *inp, int id, int len)
{
    upap_state *u = &ppp->upap;
    u_char msglen;
    char *msg;

    if (u->us_clientstate != UPAPCS_AUTHREQ) /* XXX */
	return;

    /*
     * Parse message.
     */
    if (len < 1) {
	UPAPDEBUG(("pap_rauthnak: ignoring missing msg-length."));
    } else {
	GETCHAR(msglen, inp);
	if (msglen > 0) {
	    len -= sizeof (u_char);
	    if (len < msglen) {
		UPAPDEBUG(("pap_rauthnak: rcvd short packet."));
		return;
	    }
	    msg = (char *) inp;
	    PRINTMSG(ppp, msg, msglen);
	}
    }

    u->us_clientstate = UPAPCS_BADAUTH;

    error(ppp, "PAP authentication failed");
    auth_withpeer_fail(ppp, PPP_PAP);
}

/* -----------------------------------------------------------------------------
* upap_sauthreq - Send an Authenticate-Request.
----------------------------------------------------------------------------- */
static void upap_sauthreq(struct ppp *ppp)
{
    upap_state *u = &ppp->upap;
    u_char *outp;
    int outlen;

    outlen = UPAP_HEADERLEN + 2 * sizeof (u_char) +
	u->us_userlen + u->us_passwdlen;
    outp = ppp->outpacket_buf;
    
    MAKEHEADER(outp, PPP_PAP);

    PUTCHAR(UPAP_AUTHREQ, outp);
    PUTCHAR(++u->us_id, outp);
    PUTSHORT(outlen, outp);
    PUTCHAR(u->us_userlen, outp);
    BCOPY(u->us_user, outp, u->us_userlen);
    INCPTR(u->us_userlen, outp);
    PUTCHAR(u->us_passwdlen, outp);
    BCOPY(u->us_passwd, outp, u->us_passwdlen);

    ppp_output(ppp, ppp->outpacket_buf, outlen + PPP_HDRLEN);

    ppp->upap_TORef = AddTimerToRunLoop(upap_timeout, ppp, u->us_timeouttime);
    ++u->us_transmits;
    u->us_clientstate = UPAPCS_AUTHREQ;
}

/* -----------------------------------------------------------------------------
* upap_sresp - Send a response (ack or nak).
----------------------------------------------------------------------------- */
static void upap_sresp(struct ppp *ppp, u_char code, u_char id, char * msg, int msglen)
{
    u_char *outp;
    int outlen;

    outlen = UPAP_HEADERLEN + sizeof (u_char) + msglen;
    outp = ppp->outpacket_buf;
    MAKEHEADER(outp, PPP_PAP);

    PUTCHAR(code, outp);
    PUTCHAR(id, outp);
    PUTSHORT(outlen, outp);
    PUTCHAR(msglen, outp);
    BCOPY(msg, outp, msglen);
    ppp_output(ppp, ppp->outpacket_buf, outlen + PPP_HDRLEN);
}

/* -----------------------------------------------------------------------------
* upap_printpkt - print the contents of a PAP packet.
----------------------------------------------------------------------------- */
static char *upap_codenames[] = {
    "AuthReq", "AuthAck", "AuthNak"
};

static int
upap_printpkt(ppp, p, plen, printer, arg)
struct ppp *ppp;
u_char *p;
    int plen;
    void (*printer) __P((struct ppp *, void *, char *, ...));
    void *arg;
{
    int code, id, len;
    int mlen, ulen, wlen;
    char *user, *pwd, *msg;
    u_char *pstart;

    if (plen < UPAP_HEADERLEN)
	return 0;
    pstart = p;
    GETCHAR(code, p);
    GETCHAR(id, p);
    GETSHORT(len, p);
    if (len < UPAP_HEADERLEN || len > plen)
	return 0;

    if (code >= 1 && code <= sizeof(upap_codenames) / sizeof(char *))
        printer(ppp, arg, " %s", upap_codenames[code-1]);
    else
        printer(ppp, arg, " code=0x%x", code);
    printer(ppp, arg, " id=0x%x", id);
    len -= UPAP_HEADERLEN;
    switch (code) {
    case UPAP_AUTHREQ:
	if (len < 1)
	    break;
	ulen = p[0];
	if (len < ulen + 2)
	    break;
	wlen = p[ulen + 1];
	if (len < ulen + wlen + 2)
	    break;
	user = (char *) (p + 1);
	pwd = (char *) (p + ulen + 2);
	p += ulen + wlen + 2;
	len -= ulen + wlen + 2;
        printer(ppp, arg, " user=");
        print_string(ppp, user, ulen, printer, arg);
        printer(ppp, arg, " password=");
#if 0
        if (!hide_password)
            print_string(ppp, pwd, wlen, printer, arg);
	else
#endif
            printer(ppp, arg, "<hidden>");
	break;
    case UPAP_AUTHACK:
    case UPAP_AUTHNAK:
	if (len < 1)
	    break;
	mlen = p[0];
	if (len < mlen + 1)
	    break;
	msg = (char *) (p + 1);
	p += mlen + 1;
	len -= mlen + 1;
        printer(ppp, arg, " ");
        print_string(ppp, msg, mlen, printer, arg);
	break;
    }

    /* print the rest of the bytes in the packet */
    for (; len > 0; --len) {
	GETCHAR(code, p);
        printer(ppp, arg, " %.2x", code);
    }

    return p - pstart;
}
