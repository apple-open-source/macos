/*
 * eap.c - Extensible Authentication Protocol.
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

#define RCSID	"$Id: eap.c,v 1.2 2002/03/13 22:44:34 callie Exp $"


#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>

#include "pppd.h"
#include "eap.h"
#include "md5.h"
#include "fsm.h"
#include "lcp.h"

static const char rcsid[] = RCSID;

/*
 * Command-line options.
 */
static option_t eap_option_list[] = {
    { "eap-restart", o_int, &eap[0].timeouttime,
      "Set timeout for EAP" },
    { "eap-max-challenge", o_int, &eap[0].max_transmits,
      "Set max #xmits for challenge" },
    { "eap-interval", o_int, &eap[0].chal_interval,
      "Set interval for rechallenge" },
    { NULL }
};

/*
 * Protocol entry points.
 */
static void EapInit __P((int));
static void EapLowerUp __P((int));
static void EapLowerDown __P((int));
static void EapInput __P((int, u_char *, int));
static void EapProtocolReject __P((int));
static int  EapPrintPkt __P((u_char *, int,
			      void (*) __P((void *, char *, ...)), void *));

struct protent eap_protent = {
    PPP_EAP,
    EapInit,
    EapInput,
    EapProtocolReject,
    EapLowerUp,
    EapLowerDown,
    NULL,
    NULL,
    EapPrintPkt,
    NULL,
    1,
    "EAP",
    NULL,
    eap_option_list,
    NULL,
    NULL,
    NULL
};

eap_state eap[NUM_PPP];		/* EAP state; one for each unit */

static void EapChallengeTimeout __P((void *));
static void EapReceiveRequest __P((eap_state *, u_char *, int, int));
static void EapRechallenge __P((void *));
static void EapReceiveResponse __P((eap_state *, u_char *, int, int));
static void EapReceiveSuccess __P((eap_state *, u_char *, int, int));
static void EapReceiveFailure __P((eap_state *, u_char *, int, int));
static void EapSendStatus __P((eap_state *, int));
static void EapSendChallenge __P((eap_state *));
static void EapSendResponse __P((eap_state *));
void EapGenChallenge __P((eap_state *));

extern double drand48 __P((void));
extern void srand48 __P((long));

/*
 * EapInit - Initialize a EAP unit.
 */
static void
EapInit(unit)
    int unit;
{
    eap_state *cstate = &eap[unit];

    BZERO(cstate, sizeof(*cstate));
    cstate->unit = unit;
    cstate->clientstate = EAPCS_INITIAL;
    cstate->serverstate = EAPSS_INITIAL;
    cstate->timeouttime = EAP_DEFTIMEOUT;
    cstate->max_transmits = EAP_DEFTRANSMITS;
    /* random number generator is initialized in magic_init */
}


/*
 * EapAuthWithPeer - Authenticate us with our peer (start client).
 *
 */
void
EapAuthWithPeer(unit, our_name)
    int unit;
    char *our_name;
{
    eap_state *cstate = &eap[unit];
int digest;
    cstate->resp_name = our_name;
    cstate->resp_type = digest;

    if (cstate->clientstate == EAPCS_INITIAL ||
	cstate->clientstate == EAPCS_PENDING) {
	/* lower layer isn't up - wait until later */
	cstate->clientstate = EAPCS_PENDING;
	return;
    }

    /*
     * We get here as a result of LCP coming up.
     * So even if EAP was open before, we will 
     * have to re-authenticate ourselves.
     */
    cstate->clientstate = EAPCS_LISTEN;
}


/*
 * EapAuthPeer - Authenticate our peer (start server).
 */
void
EapAuthPeer(unit, our_name)
    int unit;
    char *our_name;
{
    eap_state *cstate = &eap[unit];
 int digest;
  
    cstate->chal_name = our_name;
    cstate->chal_type = digest;

    if (cstate->serverstate == EAPSS_INITIAL ||
	cstate->serverstate == EAPSS_PENDING) {
	/* lower layer isn't up - wait until later */
	cstate->serverstate = EAPSS_PENDING;
	return;
    }

    EapGenChallenge(cstate);
    EapSendChallenge(cstate);		/* crank it up dude! */
    cstate->serverstate = EAPSS_INITIAL_CHAL;
}


/*
 * EapChallengeTimeout - Timeout expired on sending challenge.
 */
static void
EapChallengeTimeout(arg)
    void *arg;
{
    eap_state *cstate = (eap_state *) arg;
  
    /* if we aren't sending challenges, don't worry.  then again we */
    /* probably shouldn't be here either */
    if (cstate->serverstate != EAPSS_INITIAL_CHAL &&
	cstate->serverstate != EAPSS_RECHALLENGE)
	return;

    if (cstate->chal_transmits >= cstate->max_transmits) {
	/* give up on peer */
	error("Peer failed to respond to EAP challenge");
	cstate->serverstate = EAPSS_BADAUTH;
	auth_peer_fail(cstate->unit, PPP_EAP);
	return;
    }

    EapSendChallenge(cstate);		/* Re-send challenge */
}


/*
 * EapRechallenge - Time to challenge the peer again.
 */
static void
EapRechallenge(arg)
    void *arg;
{
    eap_state *cstate = (eap_state *) arg;

    /* if we aren't sending a response, don't worry. */
    if (cstate->serverstate != EAPSS_OPEN)
	return;

    EapGenChallenge(cstate);
    EapSendChallenge(cstate);
    cstate->serverstate = EAPSS_RECHALLENGE;
}


/*
 * EapLowerUp - The lower layer is up.
 *
 * Start up if we have pending requests.
 */
static void
EapLowerUp(unit)
    int unit;
{
    eap_state *cstate = &eap[unit];
  
    if (cstate->clientstate == EAPCS_INITIAL)
	cstate->clientstate = EAPCS_CLOSED;
    else if (cstate->clientstate == EAPCS_PENDING)
	cstate->clientstate = EAPCS_LISTEN;

    if (cstate->serverstate == EAPSS_INITIAL)
	cstate->serverstate = EAPSS_CLOSED;
    else if (cstate->serverstate == EAPSS_PENDING) {
	EapGenChallenge(cstate);
	EapSendChallenge(cstate);
	cstate->serverstate = EAPSS_INITIAL_CHAL;
    }
}


/*
 * EapLowerDown - The lower layer is down.
 *
 * Cancel all timeouts.
 */
static void
EapLowerDown(unit)
    int unit;
{
    eap_state *cstate = &eap[unit];
  
    /* Timeout(s) pending?  Cancel if so. */
    if (cstate->serverstate == EAPSS_INITIAL_CHAL ||
	cstate->serverstate == EAPSS_RECHALLENGE)
	UNTIMEOUT(EapChallengeTimeout, cstate);
    else if (cstate->serverstate == EAPSS_OPEN
	     && cstate->chal_interval != 0)
	UNTIMEOUT(EapRechallenge, cstate);

    cstate->clientstate = EAPCS_INITIAL;
    cstate->serverstate = EAPSS_INITIAL;
}


/*
 * EapProtocolReject - Peer doesn't grok EAP.
 */
static void
EapProtocolReject(unit)
    int unit;
{
    eap_state *cstate = &eap[unit];

    if (cstate->serverstate != EAPSS_INITIAL &&
	cstate->serverstate != EAPSS_CLOSED)
	auth_peer_fail(unit, PPP_EAP);
    if (cstate->clientstate != EAPCS_INITIAL &&
	cstate->clientstate != EAPCS_CLOSED)
	auth_withpeer_fail(unit, PPP_EAP);
    EapLowerDown(unit);		/* shutdown eap */
}


/*
 * EapInput - Input EAP packet.
 */
static void
EapInput(unit, inpacket, packet_len)
    int unit;
    u_char *inpacket;
    int packet_len;
{
    eap_state *cstate = &eap[unit];
    u_char *inp;
    u_char code, id;
    int len;
  
    /*
     * Parse header (code, id and length).
     * If packet too short, drop it.
     */
    inp = inpacket;
    if (packet_len < EAP_HEADERLEN) {
	EAPDEBUG(("EapInput: rcvd short header."));
	return;
    }
    GETCHAR(code, inp);
    GETCHAR(id, inp);
    GETSHORT(len, inp);
    if (len < EAP_HEADERLEN) {
	EAPDEBUG(("EapInput: rcvd illegal length."));
	return;
    }
    if (len > packet_len) {
	EAPDEBUG(("EapInput: rcvd short packet."));
	return;
    }
    len -= EAP_HEADERLEN;
  
    /*
     * Action depends on code (as in fact it usually does :-).
     */
    switch (code) {
    case EAP_REQUEST:
	EapReceiveRequest(cstate, inp, id, len);
	break;
    
    case EAP_RESPONSE:
	EapReceiveResponse(cstate, inp, id, len);
	break;
    
    case EAP_FAILURE:
	EapReceiveFailure(cstate, inp, id, len);
	break;

    case EAP_SUCCESS:
	EapReceiveSuccess(cstate, inp, id, len);
	break;

    default:				/* Need code reject? */
	warn("Unknown EAP code (%d) received.", code);
	break;
    }
}


/*
 * EapReceiveRequest - Receive Challenge and send Response.
 */
static void
EapReceiveRequest(cstate, inp, id, len)
    eap_state *cstate;
    u_char *inp;
    int id;
    int len;
{
    int req, rchallenge_len, i;
    u_char *rchallenge;
    int secret_len;
    char secret[MAXSECRETLEN];
    char rhostname[256];
    MD5_CTX mdContext;
    u_char hash[MD5_SIGNATURE_SIZE];
 
    if (cstate->clientstate == EAPCS_CLOSED ||
	cstate->clientstate == EAPCS_PENDING) {
	EAPDEBUG(("EapReceiveRequest: in state %d", cstate->clientstate));
	return;
    }

    if (len < 1) {
	EAPDEBUG(("EapReceiveRequest: rcvd short packet."));
	return;
    }

    GETCHAR(req, inp);
    len -= 1;
    cstate->resp_id = id;
    cstate->resp_type = req;
    cstate->resp_length = 0;
    
    switch (req) {
        case EAP_REQ_IDENTITY:
            strcpy(cstate->response, user);
            cstate->resp_length = strlen(user);
            break;
        case EAP_REQ_NOTIFICATION:
            break;
        case EAP_REQ_NAK:
            break;
        case EAP_REQ_MD5CHALLENGE:

            GETCHAR(rchallenge_len, inp);
            len -= sizeof (u_char) + rchallenge_len;	/* now name field length */
            if (len < 0) {
                CHAPDEBUG(("EapReceiveRequest: rcvd short packet."));
                return;
            }
            rchallenge = inp;
            INCPTR(rchallenge_len, inp);

            if (len >= sizeof(rhostname))
                len = sizeof(rhostname) - 1;
            BCOPY(inp, rhostname, len);
            rhostname[len] = '\000';
        
            /* Microsoft doesn't send their name back in the PPP packet */
            if (explicit_remote || (remote_name[0] != 0 && rhostname[0] == 0)) {
                strlcpy(rhostname, remote_name, sizeof(rhostname));
                EAPDEBUG(("EapReceiveRequest: using '%q' as remote name",
                        rhostname));
            }
        
            /* get secret for authenticating ourselves with the specified host */
            if (!get_secret(cstate->unit, cstate->resp_name, rhostname,
                            secret, &secret_len, 0)) {
                secret_len = 0;		/* assume null secret if can't find one */
                warn("No CHAP(EAP) secret found for authenticating us to %q", rhostname);
            }

            MD5Init(&mdContext);
            MD5Update(&mdContext, &cstate->resp_id, 1);
                EAPDEBUG(("id = %d",cstate->resp_id));
                EAPDEBUG(("secret = %s, len = %d", secret, secret_len));
            MD5Update(&mdContext, secret, secret_len);
            MD5Update(&mdContext, rchallenge, rchallenge_len);
            MD5Final(hash, &mdContext);
            BCOPY(hash, cstate->response+1, MD5_SIGNATURE_SIZE);
            cstate->response[0] = MD5_SIGNATURE_SIZE;
            cstate->resp_length = MD5_SIGNATURE_SIZE + 1;
            BCOPY(user, cstate->response + MD5_SIGNATURE_SIZE + 1, strlen(user)); /* append our name */
            cstate->resp_length += strlen(user);
            break;
        default:
            ;
    }
    
    BZERO(secret, sizeof(secret));
    EapSendResponse(cstate);
}


/*
 * EapReceiveResponse - Receive and process response.
 */
static void
EapReceiveResponse(cstate, inp, id, len)
    eap_state *cstate;
    u_char *inp;
    int id;
    int len;
{
    u_char *remmd, remmd_len;
    int secret_len, old_state;
    int code;
    char rhostname[256];
    MD5_CTX mdContext;
    char secret[MAXSECRETLEN];
    u_char hash[MD5_SIGNATURE_SIZE];

    if (cstate->serverstate == EAPSS_CLOSED ||
	cstate->serverstate == EAPSS_PENDING) {
	EAPDEBUG(("EapReceiveResponse: in state %d", cstate->serverstate));
	return;
    }

    if (id != cstate->chal_id)
	return;			/* doesn't match ID of last challenge */

    /*
     * If we have received a duplicate or bogus Response,
     * we have to send the same answer (Success/Failure)
     * as we did for the first Response we saw.
     */
    if (cstate->serverstate == EAPSS_OPEN) {
	EapSendStatus(cstate, EAP_SUCCESS);
	return;
    }
    if (cstate->serverstate == EAPSS_BADAUTH) {
	EapSendStatus(cstate, EAP_FAILURE);
	return;
    }

    if (len < 2) {
	EAPDEBUG(("EapReceiveResponse: rcvd short packet."));
	return;
    }
    GETCHAR(remmd_len, inp);		/* get length of MD */
    remmd = inp;			/* get pointer to MD */
    INCPTR(remmd_len, inp);

    len -= sizeof (u_char) + remmd_len;
    if (len < 0) {
	EAPDEBUG(("EapReceiveResponse: rcvd short packet."));
	return;
    }

    UNTIMEOUT(EapChallengeTimeout, cstate);

    if (len >= sizeof(rhostname))
	len = sizeof(rhostname) - 1;
    BCOPY(inp, rhostname, len);
    rhostname[len] = '\000';

    /*
     * Get secret for authenticating them with us,
     * do the hash ourselves, and compare the result.
     */
    code = EAP_FAILURE;
    if (!get_secret(cstate->unit, (explicit_remote? remote_name: rhostname),
		    cstate->chal_name, secret, &secret_len, 1)) {
	warn("No EAP secret found for authenticating %q", rhostname);
    } else {

	/*  generate MD based on negotiated type */
	switch (cstate->chal_type) { 

	case EAP_DIGEST_MD5:
	    EAPDEBUG(("EapReceiveResponse: rcvd type EAP-DIGEST-MD5"));
	    if (remmd_len != MD5_SIGNATURE_SIZE)
		break;			/* it's not even the right length */
	    MD5Init(&mdContext);
	    MD5Update(&mdContext, &cstate->chal_id, 1);
	    MD5Update(&mdContext, secret, secret_len);
	    MD5Update(&mdContext, cstate->challenge, cstate->chal_len);
	    MD5Final(hash, &mdContext); 

	    /* compare local and remote MDs and send the appropriate status */
	    if (memcmp (hash, remmd, MD5_SIGNATURE_SIZE) == 0)
		code = EAP_SUCCESS;	/* they are the same! */
	    break;

	default:
	    EAPDEBUG(("unknown digest type %d", cstate->chal_type));
	}
    }

    BZERO(secret, sizeof(secret));
    EapSendStatus(cstate, code);

    if ((code == EAP_SUCCESS) || (code == EAP_SUCCESS_R)) {
	old_state = cstate->serverstate;
	cstate->serverstate = EAPSS_OPEN;
	if (old_state == EAPSS_INITIAL_CHAL) {
	    auth_peer_success(cstate->unit, PPP_EAP, rhostname, len);
	}
	if (cstate->chal_interval != 0)
	    TIMEOUT(EapRechallenge, cstate, cstate->chal_interval);
	switch (cstate->chal_type) { 
	  case EAP_DIGEST_MD5:
	    notice("EAP peer authentication succeeded for %q", rhostname);
	    break;
	  default:
	    notice("EAP (unknown) peer authentication succeeded for %q", 
		   rhostname);
	    break;
	}

    } else {
	switch (cstate->chal_type) { 
	  case EAP_DIGEST_MD5:
	    error("EAP peer authentication failed for remote host %q", 
		  rhostname);
	    break;
	  default:
	    error("EAP (unknown) peer authentication failed for remote host %q", rhostname);
	    break;
	}
	cstate->serverstate = EAPSS_BADAUTH;
	auth_peer_fail(cstate->unit, PPP_EAP);
    }
}

/*
 * EapReceiveSuccess - Receive Success
 */
static void
EapReceiveSuccess(cstate, inp, id, len)
    eap_state *cstate;
    u_char *inp;
    u_char id;
    int len;
{

    if (cstate->clientstate == EAPCS_OPEN)
	/* presumably an answer to a duplicate response */
	return;

    if (cstate->clientstate != EAPCS_RESPONSE) {
	/* don't know what this is */
	EAPDEBUG(("EapReceiveSuccess: in state %d\n", cstate->clientstate));
	return;
    }

    /*
     * Print message.
     */
    if (len > 0)
	PRINTMSG(inp, len);

    cstate->clientstate = EAPCS_OPEN;

    auth_withpeer_success(cstate->unit, PPP_EAP);
}


/*
 * EapReceiveFailure - Receive failure.
 */
static void
EapReceiveFailure(cstate, inp, id, len)
    eap_state *cstate;
    u_char *inp;
    u_char id;
    int len;
{
    /*
     * Print message.
     */
    if (len > 0)
	PRINTMSG(inp, len);

    error("EAP authentication failed");
    auth_withpeer_fail(cstate->unit, PPP_EAP);
}


/*
 * EapSendChallenge - Send an Authenticate challenge.
 */
static void
EapSendChallenge(cstate)
    eap_state *cstate;
{
    u_char *outp;
    int chal_len, name_len;
    int outlen;

    chal_len = cstate->chal_len;
    name_len = strlen(cstate->chal_name);
    outlen = EAP_HEADERLEN + sizeof (u_char) + chal_len + name_len;
    outp = outpacket_buf;

    MAKEHEADER(outp, PPP_EAP);		/* paste in a EAP header */

    PUTCHAR(EAP_REQUEST, outp);
    PUTCHAR(cstate->chal_id, outp);
    PUTSHORT(outlen, outp);

    PUTCHAR(chal_len, outp);		/* put length of challenge */
    BCOPY(cstate->challenge, outp, chal_len);
    INCPTR(chal_len, outp);

    BCOPY(cstate->chal_name, outp, name_len);	/* append hostname */

    output(cstate->unit, outpacket_buf, outlen + PPP_HDRLEN);
  
    TIMEOUT(EapChallengeTimeout, cstate, cstate->timeouttime);
    ++cstate->chal_transmits;
}


/*
 * EapSendStatus - Send a status response (ack or nak).
 */
static void
EapSendStatus(cstate, code)
    eap_state *cstate;
    int code;
{
    u_char *outp;
    int outlen, msglen;
    char msg[256];

    if (code == EAP_SUCCESS)
	slprintf(msg, sizeof(msg), "Welcome to %s.", hostname);
    else if(code == EAP_SUCCESS_R)
	strcpy(msg, cstate->response);
    else
	slprintf(msg, sizeof(msg), "I don't like you.  Go 'way.");
    msglen = strlen(msg);

    outlen = EAP_HEADERLEN + msglen;
    outp = outpacket_buf;

    MAKEHEADER(outp, PPP_EAP);	/* paste in a header */
  
    PUTCHAR(code == EAP_SUCCESS_R ? EAP_SUCCESS : code, outp);
    PUTCHAR(cstate->chal_id, outp);
    PUTSHORT(outlen, outp);
    BCOPY(msg, outp, msglen);
    output(cstate->unit, outpacket_buf, outlen + PPP_HDRLEN);
}

/*
 * EapGenChallenge is used to generate a pseudo-random challenge string of
 * a pseudo-random length between min_len and max_len.  The challenge
 * string and its length are stored in *cstate, and various other fields of
 * *cstate are initialized.
 */

void
EapGenChallenge(cstate)
    eap_state *cstate;
{
    int chal_len;
    u_char *ptr = cstate->challenge;
    int i;

    /* pick a random challenge length between MIN_CHALLENGE_LENGTH and 
       MAX_CHALLENGE_LENGTH */  
    chal_len =  (unsigned) ((drand48() *
			     (MAX_CHALLENGE_LENGTH - MIN_CHALLENGE_LENGTH)) +
			    MIN_CHALLENGE_LENGTH);
    cstate->chal_len = chal_len;
    cstate->chal_id = ++cstate->id;
    cstate->chal_transmits = 0;

    /* generate a random string */
    for (i = 0; i < chal_len; i++)
	*ptr++ = (char) (drand48() * 0xff);
}

/*
 * EapSendResponse - send a response packet with values as specified
 * in *cstate.
 */
/* ARGSUSED */
static void
EapSendResponse(cstate)
    eap_state *cstate;
{
    u_char *outp;
    int outlen;

    outlen = EAP_HEADERLEN + sizeof (u_char) + cstate->resp_length;
    outp = outpacket_buf;

    MAKEHEADER(outp, PPP_EAP);

    PUTCHAR(EAP_RESPONSE, outp);	/* we are a response */
    PUTCHAR(cstate->resp_id, outp);	/* copy id from request packet */
    PUTSHORT(outlen, outp);		/* packet length */

    PUTCHAR(cstate->resp_type, outp);		/* copy type from request packet */
    BCOPY(cstate->response, outp, cstate->resp_length); /* append the response */

    /* send the packet */
    output(cstate->unit, outpacket_buf, outlen + PPP_HDRLEN);
}

/*
 * EapPrintPkt - print the contents of a EAP packet.
 */
static char *EapCodenames[] = {
    "Request", "Response", "Success", "Failure"
};
static char *EapRequestnames[] = {
    "Identity", "Notification", "Nak", "MD5-Challenge"
};

static int
EapPrintPkt(p, plen, printer, arg)
    u_char *p;
    int plen;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
    int code, id, len, req;
    int clen, nlen;
    u_char x;

    if (plen < EAP_HEADERLEN)
	return 0;
    GETCHAR(code, p);
    GETCHAR(id, p);
    GETSHORT(len, p);
    if (len < EAP_HEADERLEN || len > plen)
	return 0;

    if (code >= 1 && code <= sizeof(EapCodenames) / sizeof(char *))
	printer(arg, " %s", EapCodenames[code-1]);
    else
	printer(arg, " code=0x%x", code);
    printer(arg, " id=0x%x", id);
    len -= EAP_HEADERLEN;
    switch (code) {
    case EAP_REQUEST:
    case EAP_RESPONSE:
	if (len < 1)
	    break;
        GETCHAR(req, p);
        if (req >= 1 && req <= sizeof(EapRequestnames) / sizeof(char *))
            printer(arg, " %s", EapRequestnames[req-1]);
        else
            printer(arg, " type=0x%x", req);
        len -= 1;
        if (len == 0)
            break;
	printer(arg, " <");
        switch (req) {
            case EAP_REQ_IDENTITY: 
            case EAP_REQ_NOTIFICATION: 
                print_string((char *)p, len, printer, arg);
                break;
            case EAP_REQ_MD5CHALLENGE: 
                GETCHAR(clen, p);
                nlen = clen;
                if ((nlen + 1) > len)
                    break;
                for (; clen > 0; clen--) {
                    GETCHAR(x, p);
                    printer(arg, "%.2x", x);
                }
                clen = len - nlen - 1;
                if (clen) {
                    printer(arg, ">");
                    printer(arg, " <");
                    print_string((char *)p, clen, printer, arg);
                }
                break;
            default :
                clen = len;
                for (; clen > 0; clen--) {
                    GETCHAR(x, p);
                    printer(arg, "%.2x", x);
                }
        }
	printer(arg, ">");
	break;
    case EAP_FAILURE:
    case EAP_SUCCESS:
	printer(arg, " ");
	print_string((char *)p, len, printer, arg);
	break;
    default:
	for (clen = len; clen > 0; --clen) {
	    GETCHAR(x, p);
	    printer(arg, " %.2x", x);
	}
    }

    return len + EAP_HEADERLEN + 1;
}
