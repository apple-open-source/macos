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
 * chap.c - Challenge Handshake Authentication Protocol.
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
 * Copyright (c) 1991 Gregory M. Christy.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Gregory M. Christy.  The name of the author may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission.
 *
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

#include "md5.h"
#include "chap_ms.h"
#include "ppp_utils.h"

/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
Forward Declarations
----------------------------------------------------------------------------- */

static void ChapInit __P((struct ppp *));
static void ChapLowerUp __P((struct ppp *));
static void ChapLowerDown __P((struct ppp *));
static void ChapInput __P((struct ppp *, u_char *, int));
static void ChapProtocolReject __P((struct ppp *));
static int  ChapPrintPkt __P((struct ppp *, u_char *, int,
			      void (*) __P((struct ppp *, void *, char *, ...)), void *));

static void ChapChallengeTimeout __P((CFRunLoopTimerRef, void *));
static void ChapResponseTimeout __P((CFRunLoopTimerRef, void *));
static void ChapReceiveChallenge __P((struct ppp *, u_char *, int, int));
static void ChapRechallenge __P((CFRunLoopTimerRef, void *));
static void ChapReceiveResponse __P((struct ppp *, u_char *, int, int));
static void ChapReceiveSuccess __P((struct ppp *, u_char *, int, int));
static void ChapReceiveFailure __P((struct ppp *, u_char *, int, int));
static void ChapSendStatus __P((struct ppp *, int));
static void ChapSendChallenge __P((struct ppp *));
static void ChapSendResponse __P((struct ppp *));
static void ChapGenChallenge __P((struct ppp *));

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

struct protent chap_protent = {
    PPP_CHAP,
    ChapInit,
    ChapInput,
    ChapProtocolReject,
    ChapLowerUp,
    ChapLowerDown,
    NULL,
    NULL,
    ChapPrintPkt,
    NULL,
    "CHAP",
    NULL,
    0, //chap_option_list,
    NULL,
    NULL,
    NULL
};


/* -----------------------------------------------------------------------------
* ChapInit - Initialize a CHAP unit.
----------------------------------------------------------------------------- */
static void ChapInit(struct ppp *ppp)
{
    chap_state *cstate = &ppp->chap;

    BZERO(cstate, sizeof(*cstate));
    cstate->unit = ppp->unit;
    cstate->clientstate = CHAPCS_INITIAL;
    cstate->serverstate = CHAPSS_INITIAL;
    cstate->timeouttime = CHAP_DEFTIMEOUT;
    cstate->max_transmits = CHAP_DEFTRANSMITS;
    ppp->chap_challengeTORef = 0;
    ppp->chap_rechallengeTORef = 0;
    ppp->chap_responseTORef = 0;
}


/* -----------------------------------------------------------------------------
* ChapAuthWithPeer - Authenticate us with our peer (start client).
----------------------------------------------------------------------------- */
void ChapAuthWithPeer(struct ppp *ppp, char *our_name, int digest)
{
    chap_state *cstate = &ppp->chap;

   cstate->resp_name = our_name;
    cstate->resp_type = digest;

    if (cstate->clientstate == CHAPCS_INITIAL ||
	cstate->clientstate == CHAPCS_PENDING) {
	/* lower layer isn't up - wait until later */
	cstate->clientstate = CHAPCS_PENDING;
	return;
    }

    /*
     * We get here as a result of LCP coming up.
     * So even if CHAP was open before, we will 
     * have to re-authenticate ourselves.
     */
    cstate->clientstate = CHAPCS_LISTEN;
}

/* -----------------------------------------------------------------------------
* ChapAuthPeer - Authenticate our peer (start server).
----------------------------------------------------------------------------- */
void ChapAuthPeer(struct ppp *ppp, char *our_name, int digest)
{
    chap_state *cstate = &ppp->chap;

    cstate->chal_name = our_name;
    cstate->chal_type = digest;

    if (cstate->serverstate == CHAPSS_INITIAL ||
	cstate->serverstate == CHAPSS_PENDING) {
	/* lower layer isn't up - wait until later */
	cstate->serverstate = CHAPSS_PENDING;
	return;
    }

    ChapGenChallenge(ppp);
    ChapSendChallenge(ppp);		/* crank it up dude! */
    cstate->serverstate = CHAPSS_INITIAL_CHAL;
}

/* -----------------------------------------------------------------------------
* ChapChallengeTimeout - Timeout expired on sending challenge.
----------------------------------------------------------------------------- */
static void ChapChallengeTimeout(CFRunLoopTimerRef timer, void *arg)
{
    struct ppp *ppp = (struct ppp *) arg;
    chap_state *cstate = &ppp->chap;

    /* if we aren't sending challenges, don't worry.  then again we */
    /* probably shouldn't be here either */
    if (cstate->serverstate != CHAPSS_INITIAL_CHAL &&
	cstate->serverstate != CHAPSS_RECHALLENGE)
	return;

    if (cstate->chal_transmits >= cstate->max_transmits) {
	/* give up on peer */
	error(ppp, "Peer failed to respond to CHAP challenge");
	cstate->serverstate = CHAPSS_BADAUTH;
	auth_peer_fail(ppp, PPP_CHAP);
	return;
    }

    ChapSendChallenge(ppp);		/* Re-send challenge */
}

/* -----------------------------------------------------------------------------
* ChapResponseTimeout - Timeout expired on sending response.
----------------------------------------------------------------------------- */
static void ChapResponseTimeout (CFRunLoopTimerRef timer, void *arg)
{
    struct ppp *ppp = (struct ppp *) arg;
    chap_state *cstate = &ppp->chap;

    /* if we aren't sending a response, don't worry. */
    if (cstate->clientstate != CHAPCS_RESPONSE)
	return;

    ChapSendResponse(ppp);		/* re-send response */
}

/* -----------------------------------------------------------------------------
* ChapRechallenge - Time to challenge the peer again.
----------------------------------------------------------------------------- */
static void ChapRechallenge(CFRunLoopTimerRef timer, void *arg)
{
    struct ppp *ppp = (struct ppp *) arg;
    chap_state *cstate = &ppp->chap;

    /* if we aren't sending a response, don't worry. */
    if (cstate->serverstate != CHAPSS_OPEN)
	return;

    ChapGenChallenge(ppp);
    ChapSendChallenge(ppp);
    cstate->serverstate = CHAPSS_RECHALLENGE;
}

/* -----------------------------------------------------------------------------
* ChapLowerUp - The lower layer is up.
*
* Start up if we have pending requests.
----------------------------------------------------------------------------- */
static void ChapLowerUp(struct ppp *ppp)
{
    chap_state *cstate = &ppp->chap;
  
    if (cstate->clientstate == CHAPCS_INITIAL)
	cstate->clientstate = CHAPCS_CLOSED;
    else if (cstate->clientstate == CHAPCS_PENDING)
	cstate->clientstate = CHAPCS_LISTEN;

    if (cstate->serverstate == CHAPSS_INITIAL)
	cstate->serverstate = CHAPSS_CLOSED;
    else if (cstate->serverstate == CHAPSS_PENDING) {
	ChapGenChallenge(ppp);
	ChapSendChallenge(ppp);
	cstate->serverstate = CHAPSS_INITIAL_CHAL;
    }
}

/* -----------------------------------------------------------------------------
* ChapLowerDown - The lower layer is down.
* Cancel all timeouts.
----------------------------------------------------------------------------- */
static void ChapLowerDown(struct ppp *ppp)
{
    chap_state *cstate = &ppp->chap;
  
    /* Timeout(s) pending?  Cancel if so. */
    if (cstate->serverstate == CHAPSS_INITIAL_CHAL ||
	cstate->serverstate == CHAPSS_RECHALLENGE)
        DelTimerFromRunLoop(&(ppp->chap_challengeTORef));
    else if (cstate->serverstate == CHAPSS_OPEN
	     && cstate->chal_interval != 0)
        DelTimerFromRunLoop(&(ppp->chap_rechallengeTORef));
    if (cstate->clientstate == CHAPCS_RESPONSE)
        DelTimerFromRunLoop(&(ppp->chap_responseTORef));

    cstate->clientstate = CHAPCS_INITIAL;
    cstate->serverstate = CHAPSS_INITIAL;
}

/* -----------------------------------------------------------------------------
* ChapProtocolReject - Peer doesn't grok CHAP.
----------------------------------------------------------------------------- */
static void ChapProtocolReject(struct ppp *ppp)
{
    chap_state *cstate = &ppp->chap;

    if (cstate->serverstate != CHAPSS_INITIAL &&
	cstate->serverstate != CHAPSS_CLOSED)
	auth_peer_fail(ppp, PPP_CHAP);
    if (cstate->clientstate != CHAPCS_INITIAL &&
	cstate->clientstate != CHAPCS_CLOSED)
	auth_withpeer_fail(ppp, PPP_CHAP);
    ChapLowerDown(ppp);		/* shutdown chap */
}

/* -----------------------------------------------------------------------------
* ChapInput - Input CHAP packet.
----------------------------------------------------------------------------- */
static void ChapInput(struct ppp *ppp, u_char *inpacket, int packet_len)
{
    u_char *inp;
    u_char code, id;
    int len;
  
    /*
     * Parse header (code, id and length).
     * If packet too short, drop it.
     */
    inp = inpacket;
    if (packet_len < CHAP_HEADERLEN) {
	CHAPDEBUG(("ChapInput: rcvd short header."));
	return;
    }
    GETCHAR(code, inp);
    GETCHAR(id, inp);
    GETSHORT(len, inp);
    if (len < CHAP_HEADERLEN) {
	CHAPDEBUG(("ChapInput: rcvd illegal length."));
	return;
    }
    if (len > packet_len) {
	CHAPDEBUG(("ChapInput: rcvd short packet."));
	return;
    }
    len -= CHAP_HEADERLEN;
  
    /*
     * Action depends on code (as in fact it usually does :-).
     */
    switch (code) {
    case CHAP_CHALLENGE:
	ChapReceiveChallenge(ppp, inp, id, len);
	break;
    
    case CHAP_RESPONSE:
	ChapReceiveResponse(ppp, inp, id, len);
	break;
    
    case CHAP_FAILURE:
	ChapReceiveFailure(ppp, inp, id, len);
	break;

    case CHAP_SUCCESS:
	ChapReceiveSuccess(ppp, inp, id, len);
	break;

    default:				/* Need code reject? */
        warn(ppp, "Unknown CHAP code (%d) received.", code);
	break;
    }
}

/* -----------------------------------------------------------------------------
* ChapReceiveChallenge - Receive Challenge and send Response.
----------------------------------------------------------------------------- */
static void ChapReceiveChallenge(struct ppp *ppp, u_char *inp, int id, int len)
{
    chap_state *cstate = &ppp->chap;
    int rchallenge_len;
    u_char *rchallenge;
    int secret_len;
    char secret[MAXSECRETLEN];
    char rhostname[256];
    MD5_CTX mdContext;
    u_char hash[MD5_SIGNATURE_SIZE];
 
    if (cstate->clientstate == CHAPCS_CLOSED ||
	cstate->clientstate == CHAPCS_PENDING) {
	CHAPDEBUG(("ChapReceiveChallenge: in state %d", cstate->clientstate));
	return;
    }

    if (len < 2) {
	CHAPDEBUG(("ChapReceiveChallenge: rcvd short packet."));
	return;
    }

    GETCHAR(rchallenge_len, inp);
    len -= sizeof (u_char) + rchallenge_len;	/* now name field length */
    if (len < 0) {
	CHAPDEBUG(("ChapReceiveChallenge: rcvd short packet."));
	return;
    }
    rchallenge = inp;
    INCPTR(rchallenge_len, inp);

    if (len >= sizeof(rhostname))
	len = sizeof(rhostname) - 1;
    BCOPY(inp, rhostname, len);
    rhostname[len] = '\000';

    /* Microsoft doesn't send their name back in the PPP packet */
    if (ppp->explicit_remote || (ppp->remote_name[0] != 0 && rhostname[0] == 0)) {
	strlcpy(rhostname, ppp->remote_name, sizeof(rhostname));
	CHAPDEBUG(("ChapReceiveChallenge: using '%q' as remote name",
		   rhostname));
    }

    /* get secret for authenticating ourselves with the specified host */
    if (!get_secret(ppp, cstate->resp_name, rhostname,
		    secret, &secret_len, 0)) {
	secret_len = 0;		/* assume null secret if can't find one */
        warn(ppp, "No CHAP secret found for authenticating us to %q", rhostname);
    }

    /* cancel response send timeout if necessary */
    if (cstate->clientstate == CHAPCS_RESPONSE)
        DelTimerFromRunLoop(&(ppp->chap_responseTORef));

    cstate->resp_id = id;
    cstate->resp_transmits = 0;

    /*  generate MD based on negotiated type */
    switch (cstate->resp_type) { 

    case CHAP_DIGEST_MD5:
	MD5Init(&mdContext);
	MD5Update(&mdContext, &cstate->resp_id, 1);
	MD5Update(&mdContext, secret, secret_len);
	MD5Update(&mdContext, rchallenge, rchallenge_len);
	MD5Final(hash, &mdContext);
	BCOPY(hash, cstate->response, MD5_SIGNATURE_SIZE);
	cstate->resp_length = MD5_SIGNATURE_SIZE;
	break;

    case CHAP_MICROSOFT:
        ChapMS(cstate, rchallenge, rchallenge_len, secret, secret_len);
	break;

    default:
	CHAPDEBUG(("unknown digest type %d", cstate->resp_type));
	return;
    }

    BZERO(secret, sizeof(secret));
    ChapSendResponse(ppp);
}

/* -----------------------------------------------------------------------------
* ChapReceiveResponse - Receive and process response.
----------------------------------------------------------------------------- */
static void ChapReceiveResponse(struct ppp *ppp, u_char *inp, int id, int len)
{
    chap_state *cstate = &ppp->chap;
    u_char *remmd, remmd_len;
    int secret_len, old_state;
    int code;
    char rhostname[256];
    MD5_CTX mdContext;
    char secret[MAXSECRETLEN];
    u_char hash[MD5_SIGNATURE_SIZE];

    if (cstate->serverstate == CHAPSS_CLOSED ||
	cstate->serverstate == CHAPSS_PENDING) {
	CHAPDEBUG(("ChapReceiveResponse: in state %d", cstate->serverstate));
	return;
    }

    if (id != cstate->chal_id)
	return;			/* doesn't match ID of last challenge */

    /*
     * If we have received a duplicate or bogus Response,
     * we have to send the same answer (Success/Failure)
     * as we did for the first Response we saw.
     */
    if (cstate->serverstate == CHAPSS_OPEN) {
	ChapSendStatus(ppp, CHAP_SUCCESS);
	return;
    }
    if (cstate->serverstate == CHAPSS_BADAUTH) {
	ChapSendStatus(ppp, CHAP_FAILURE);
	return;
    }

    if (len < 2) {
	CHAPDEBUG(("ChapReceiveResponse: rcvd short packet."));
	return;
    }
    GETCHAR(remmd_len, inp);		/* get length of MD */
    remmd = inp;			/* get pointer to MD */
    INCPTR(remmd_len, inp);

    len -= sizeof (u_char) + remmd_len;
    if (len < 0) {
	CHAPDEBUG(("ChapReceiveResponse: rcvd short packet."));
	return;
    }

    DelTimerFromRunLoop(&(ppp->chap_challengeTORef));

    if (len >= sizeof(rhostname))
	len = sizeof(rhostname) - 1;
    BCOPY(inp, rhostname, len);
    rhostname[len] = '\000';

    /*
     * Get secret for authenticating them with us,
     * do the hash ourselves, and compare the result.
     */
    code = CHAP_FAILURE;
    if (!get_secret(ppp, (ppp->explicit_remote? ppp->remote_name: rhostname),
		    cstate->chal_name, secret, &secret_len, 1)) {
        warn(ppp, "No CHAP secret found for authenticating %q", rhostname);
    } else {

	/*  generate MD based on negotiated type */
	switch (cstate->chal_type) { 

	case CHAP_DIGEST_MD5:		/* only MD5 is defined for now */
	    if (remmd_len != MD5_SIGNATURE_SIZE)
		break;			/* it's not even the right length */
	    MD5Init(&mdContext);
	    MD5Update(&mdContext, &cstate->chal_id, 1);
	    MD5Update(&mdContext, secret, secret_len);
	    MD5Update(&mdContext, cstate->challenge, cstate->chal_len);
	    MD5Final(hash, &mdContext); 

	    /* compare local and remote MDs and send the appropriate status */
	    if (memcmp (hash, remmd, MD5_SIGNATURE_SIZE) == 0)
		code = CHAP_SUCCESS;	/* they are the same! */
	    break;

	default:
	    CHAPDEBUG(("unknown digest type %d", cstate->chal_type));
	}
    }

    BZERO(secret, sizeof(secret));
    ChapSendStatus(ppp, code);

    if (code == CHAP_SUCCESS) {
	old_state = cstate->serverstate;
	cstate->serverstate = CHAPSS_OPEN;
	if (old_state == CHAPSS_INITIAL_CHAL) {
	    auth_peer_success(ppp, PPP_CHAP, rhostname, len);
	}
	if (cstate->chal_interval != 0)
            ppp->chap_rechallengeTORef = AddTimerToRunLoop(ChapRechallenge, ppp, cstate->chal_interval);
	notice(ppp, "CHAP peer authentication succeeded for %q", rhostname);

    } else {
	error(ppp, "CHAP peer authentication failed for remote host %q", rhostname);
	cstate->serverstate = CHAPSS_BADAUTH;
	auth_peer_fail(ppp, PPP_CHAP);
    }
}

/* -----------------------------------------------------------------------------
* ChapReceiveSuccess - Receive Success
----------------------------------------------------------------------------- */
static void ChapReceiveSuccess(struct ppp *ppp, u_char *inp, int id, int len)
{
    chap_state *cstate = &ppp->chap;

    if (cstate->clientstate == CHAPCS_OPEN)
	/* presumably an answer to a duplicate response */
	return;

    if (cstate->clientstate != CHAPCS_RESPONSE) {
	/* don't know what this is */
	CHAPDEBUG(("ChapReceiveSuccess: in state %d\n", cstate->clientstate));
	return;
    }

    DelTimerFromRunLoop(&(ppp->chap_responseTORef));

    /*
     * Print message.
     */
    if (len > 0)
	PRINTMSG(ppp, inp, len);

    cstate->clientstate = CHAPCS_OPEN;

    auth_withpeer_success(ppp, PPP_CHAP);
}

/* -----------------------------------------------------------------------------
* ChapReceiveFailure - Receive failure.
----------------------------------------------------------------------------- */
static void ChapReceiveFailure(struct ppp *ppp, u_char *inp, int id, int len)
{
    chap_state *cstate = &ppp->chap;

    if (cstate->clientstate != CHAPCS_RESPONSE) {
	/* don't know what this is */
	CHAPDEBUG(("ChapReceiveFailure: in state %d\n", cstate->clientstate));
	return;
    }

    DelTimerFromRunLoop(&(ppp->chap_responseTORef));

    /*
     * Print message.
     */
    if (len > 0)
	PRINTMSG(ppp, inp, len);

    error(ppp, "CHAP authentication failed");
    auth_withpeer_fail(ppp, PPP_CHAP);
}

/* -----------------------------------------------------------------------------
* ChapSendChallenge - Send an Authenticate challenge.
----------------------------------------------------------------------------- */
static void ChapSendChallenge(struct ppp *ppp)
{
    chap_state *cstate = &ppp->chap;
    u_char *outp;
    int chal_len, name_len;
    int outlen;

    chal_len = cstate->chal_len;
    name_len = strlen(cstate->chal_name);
    outlen = CHAP_HEADERLEN + sizeof (u_char) + chal_len + name_len;
    outp = ppp->outpacket_buf;

    MAKEHEADER(outp, PPP_CHAP);		/* paste in a CHAP header */

    PUTCHAR(CHAP_CHALLENGE, outp);
    PUTCHAR(cstate->chal_id, outp);
    PUTSHORT(outlen, outp);

    PUTCHAR(chal_len, outp);		/* put length of challenge */
    BCOPY(cstate->challenge, outp, chal_len);
    INCPTR(chal_len, outp);

    BCOPY(cstate->chal_name, outp, name_len);	/* append hostname */

    ppp_output(ppp, ppp->outpacket_buf, outlen + PPP_HDRLEN);

    ppp->chap_challengeTORef = AddTimerToRunLoop(ChapChallengeTimeout, ppp, cstate->timeouttime);
    ++cstate->chal_transmits;
}

/* -----------------------------------------------------------------------------
* ChapSendStatus - Send a status response (ack or nak).
----------------------------------------------------------------------------- */
static void ChapSendStatus(struct ppp *ppp, int code)
{
    chap_state *cstate = &ppp->chap;
   u_char *outp;
    int outlen, msglen;
    char msg[256];

    if (code == CHAP_SUCCESS)
	slprintf(ppp, msg, sizeof(msg), "Welcome to %s.", ppp->hostname);
    else
	slprintf(ppp, msg, sizeof(msg), "I don't like you.  Go 'way.");
    msglen = strlen(msg);

    outlen = CHAP_HEADERLEN + msglen;
    outp = ppp->outpacket_buf;

    MAKEHEADER(outp, PPP_CHAP);	/* paste in a header */
  
    PUTCHAR(code, outp);
    PUTCHAR(cstate->chal_id, outp);
    PUTSHORT(outlen, outp);
    BCOPY(msg, outp, msglen);
    ppp_output(ppp, ppp->outpacket_buf, outlen + PPP_HDRLEN);
}

/* -----------------------------------------------------------------------------
* ChapGenChallenge is used to generate a pseudo-random challenge string of
* a pseudo-random length between min_len and max_len.  The challenge
* string and its length are stored in *cstate, and various other fields of
* *cstate are initialized.
----------------------------------------------------------------------------- */
static void ChapGenChallenge(struct ppp *ppp)
{
    chap_state *cstate = &ppp->chap;
    int chal_len;
    u_char *ptr = cstate->challenge;
    int i;

    /* pick a random challenge length between MIN_CHALLENGE_LENGTH and 
       MAX_CHALLENGE_LENGTH */  
    chal_len =  (random() %  (MAX_CHALLENGE_LENGTH - MIN_CHALLENGE_LENGTH + 1)) + MIN_CHALLENGE_LENGTH;

    cstate->chal_len = chal_len;
    cstate->chal_id = ++cstate->id;
    cstate->chal_transmits = 0;

    /* generate a random string */
    for (i = 0; i < chal_len; i++)
	*ptr++ = (char) random();
}

/* -----------------------------------------------------------------------------
* ChapSendResponse - send a response packet with values as specified in *cstate.
----------------------------------------------------------------------------- */
static void ChapSendResponse(struct ppp *ppp)
{
    chap_state *cstate = &ppp->chap;
    u_char *outp;
    int outlen, md_len, name_len;

    md_len = cstate->resp_length;
    name_len = strlen(cstate->resp_name);
    outlen = CHAP_HEADERLEN + sizeof (u_char) + md_len + name_len;
    outp = ppp->outpacket_buf;

    MAKEHEADER(outp, PPP_CHAP);

    PUTCHAR(CHAP_RESPONSE, outp);	/* we are a response */
    PUTCHAR(cstate->resp_id, outp);	/* copy id from challenge packet */
    PUTSHORT(outlen, outp);		/* packet length */

    PUTCHAR(md_len, outp);		/* length of MD */
    BCOPY(cstate->response, outp, md_len);	/* copy MD to buffer */
    INCPTR(md_len, outp);

    BCOPY(cstate->resp_name, outp, name_len); /* append our name */

    /* send the packet */
    ppp_output(ppp, ppp->outpacket_buf, outlen + PPP_HDRLEN);

    cstate->clientstate = CHAPCS_RESPONSE;
    ppp->chap_responseTORef = AddTimerToRunLoop(ChapResponseTimeout, ppp, cstate->timeouttime);
    ++cstate->resp_transmits;
}

/* -----------------------------------------------------------------------------
* ChapPrintPkt - print the contents of a CHAP packet.
----------------------------------------------------------------------------- */
static char *ChapCodenames[] = {
    "Challenge", "Response", "Success", "Failure"
};

static int
ChapPrintPkt(ppp, p, plen, printer, arg)
struct ppp *ppp;
u_char *p;
    int plen;
    void (*printer) __P((struct ppp *, void *, char *, ...));
    void *arg;
{
    int code, id, len;
    int clen, nlen;
    u_char x;

    if (plen < CHAP_HEADERLEN)
	return 0;
    GETCHAR(code, p);
    GETCHAR(id, p);
    GETSHORT(len, p);
    if (len < CHAP_HEADERLEN || len > plen)
	return 0;

    if (code >= 1 && code <= sizeof(ChapCodenames) / sizeof(char *))
        printer(ppp, arg, " %s", ChapCodenames[code-1]);
    else
        printer(ppp, arg, " code=0x%x", code);
    printer(ppp, arg, " id=0x%x", id);
    len -= CHAP_HEADERLEN;
    switch (code) {
    case CHAP_CHALLENGE:
    case CHAP_RESPONSE:
	if (len < 1)
	    break;
	clen = p[0];
	if (len < clen + 1)
	    break;
	++p;
	nlen = len - clen - 1;
        printer(ppp, arg, " <");
	for (; clen > 0; --clen) {
	    GETCHAR(x, p);
            printer(ppp, arg, "%.2x", x);
	}
            printer(ppp, arg, ">, name = ");
        print_string(ppp, (char *)p, nlen, printer, arg);
	break;
    case CHAP_FAILURE:
    case CHAP_SUCCESS:
        printer(ppp, arg, " ");
        print_string(ppp, (char *)p, len, printer, arg);
	break;
    default:
	for (clen = len; clen > 0; --clen) {
	    GETCHAR(x, p);
            printer(ppp, arg, " %.2x", x);
	}
    }

    return len + CHAP_HEADERLEN;
}
