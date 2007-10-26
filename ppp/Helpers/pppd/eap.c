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

#define RCSID	"$Id: eap.c,v 1.25 2005/12/13 06:30:15 lindak Exp $"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>

#include "pppd.h"
#include "eap.h"
#include "fsm.h"
#include "lcp.h"
#include "chap_ms.h" // for mppe keys

#ifndef lint
static const char rcsid[] = RCSID;
#endif


static int eaploadplugin(char **argv);
int sys_eaploadplugin(char *arg, eap_ext *eap);

/*
 * Command-line options.
 */
static option_t eap_option_list[] = {
    { "eap-restart", o_int, &eap[0].timeouttime,
      "Set timeout for EAP" },
    { "eap-max-challenge", o_int, &eap[0].max_transmits,
      "Set max #xmits for challenge" },
    { "eap-interval", o_int, &eap[0].req_interval,
      "Set interval for rechallenge" },
    { "eapplugin", o_special, (void *)eaploadplugin,
      "Load an eap plug-in module into pppd", OPT_PRIV | OPT_A2LIST },
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
    NULL,
#ifdef __APPLE__
    NULL,
    NULL,
    NULL,
    NULL
#endif
};

eap_state eap[NUM_PPP];		/* EAP state; one for each unit */

eap_ext *eap_extensions = NULL;	/* eap extensions list */

static void EapChallengeTimeout __P((void *));
static void EapReceiveRequest __P((eap_state *, u_char *, int, u_char *, int, int));
static void EapRechallenge __P((void *));
static void EapReceiveResponse __P((eap_state *, u_char *, int, u_char *, int, int));
static void EapReceiveSuccess __P((eap_state *, u_char *, int, u_char *, int, int));
static void EapReceiveFailure __P((eap_state *, u_char *, int, u_char *, int, int));
static void EapSendIdentityRequest __P((eap_state *));
static eap_ext * EapSupportedType(int type);
static int EAPServerProcess(eap_state *, u_int16_t, u_char *, int);
static int EAPClientProcess(eap_state *, u_int16_t, u_char *, int);
static void EAPClientAction(eap_state *);
static void EAPServerAction(eap_state *);
static void EAPInput_fd(void);


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
    cstate->client_ext_ui_fds[0] = -1;
    cstate->client_ext_ui_fds[1] = -1;
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


	if (username[0] == 0) {
		/*
			no identity, get it from a plugin
		*/

		eap_ext 	*eap;

		for (eap = eap_extensions; eap; eap = eap->next) {
		   if (eap->identity
				&& (eap->identity(username, sizeof(username)) == EAP_NO_ERROR))
				break;
		}
	}
	
    cstate->our_identity = username;
    cstate->username = username;
    cstate->password = passwd;

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
      
    cstate->our_identity = user; 
    cstate->username = 0;
    cstate->password = 0;

    cstate->req_type = EAP_TYPE_IDENTITY;
    cstate->req_transmits = 0;
    cstate->req_id = 1;

    if (cstate->serverstate == EAPSS_INITIAL ||
	cstate->serverstate == EAPSS_PENDING) {
	/* lower layer isn't up - wait until later */
	cstate->serverstate = EAPSS_PENDING;
	return;
    }

    EapSendIdentityRequest(cstate);
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

    if (cstate->req_transmits >= cstate->max_transmits) {
	/* give up on peer */
	error("Peer failed to respond to EAP challenge");
	cstate->serverstate = EAPSS_BADAUTH;
	auth_peer_fail(cstate->unit, PPP_EAP);
	return;
    }

    if (cstate->req_type == EAP_TYPE_IDENTITY)
        EapSendIdentityRequest(cstate);		/* Re-send challenge */
    else
        EAPServerProcess(cstate, EAP_NOTIFICATION_TIMEOUT, 0, 0);
}


/*
 * EapRechallenge - Time to challenge the peer again.
 */
static void
EapRechallenge(arg)
    void *arg;
{
    eap_state  	*cstate = (eap_state *) arg;
    
    /* if we aren't sending a response, don't worry. */
    if (cstate->serverstate != EAPSS_OPEN)
	return;

    cstate->req_id++;
    cstate->req_transmits = 0;
    
    EAPServerProcess(cstate, EAP_NOTIFICATION_RESTART, 0, 0);
    
    cstate->serverstate = EAPSS_RECHALLENGE;
}

/*
 * EapLostSuccess - EAP success has been lost.
 *
 * Simulate an EAP success packet .
 */
void
EapLostSuccess(unit)
    int unit;
{
    eap_state *cstate = &eap[unit];
    u_char inpacket[EAP_HEADERLEN];
	u_char *inp = inpacket;

    PUTCHAR(EAP_SUCCESS, inp);		/* simulate success */
    PUTCHAR(cstate->resp_id, inp);  /* id must match the last response we sent */
    PUTSHORT(EAP_HEADERLEN, inp);

	EapReceiveSuccess(cstate, inpacket, EAP_HEADERLEN, inp, cstate->resp_id, 0);
}

/*
 * EapLostFailure - EAP failure has been lost.
 *
 * Simulate an EAP failure packet .
 */
void
EapLostFailure(unit)
    int unit;
{
    eap_state *cstate = &eap[unit];
    u_char inpacket[EAP_HEADERLEN];
	u_char *inp = inpacket;
	
	MAKEHEADER(inp, PPP_EAP);		/* paste in a EAP header */
    PUTCHAR(EAP_FAILURE, inp);		/* simulate failure */
    PUTCHAR(cstate->resp_id, inp);  /* id must match the last response we sent */
    PUTSHORT(EAP_HEADERLEN, inp);

	EapReceiveFailure(cstate, inpacket, EAP_HEADERLEN, inp, cstate->resp_id, 0);
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
        EapSendIdentityRequest(cstate);
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
	     && cstate->req_interval != 0)
	UNTIMEOUT(EapRechallenge, cstate);

    cstate->clientstate = EAPCS_INITIAL;
    cstate->serverstate = EAPSS_INITIAL;

    if (cstate->client_ext) {
        cstate->client_ext->dispose(cstate->client_ext_ctx);
        free (cstate->client_ext_input);
        free (cstate->client_ext_output);
        cstate->client_ext = 0;
        cstate->client_ext_ctx = 0;
        cstate->client_ext_input = 0;
        cstate->client_ext_output = 0;
    }

    if (cstate->server_ext) {
        cstate->server_ext->dispose(cstate->server_ext_ctx);
        free (cstate->server_ext_input);
        free (cstate->server_ext_output);
        cstate->server_ext = 0;
        cstate->server_ext_ctx = 0;
        cstate->server_ext_input = 0;
        cstate->server_ext_output = 0;
    }
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
	EapReceiveRequest(cstate, inpacket, packet_len, inp, id, len);
	break;
    
    case EAP_RESPONSE:
	EapReceiveResponse(cstate, inpacket, packet_len, inp, id, len);
	break;
    
    case EAP_FAILURE:
	EapReceiveFailure(cstate, inpacket, packet_len, inp, id, len);
	break;

    case EAP_SUCCESS:
	EapReceiveSuccess(cstate, inpacket, packet_len, inp, id, len);
	break;

    default:				/* Need code reject? */
	warning("Unknown EAP code (%d) received.", code);
	break;
    }
}

/*
 * EapSendIdentityRequest - Send an Request for Identity.
 */
static void
EapSendIdentityRequest(cstate)
    eap_state *cstate;
{
    u_char *outp;
    int outlen = 0;

    outlen = EAP_HEADERLEN + sizeof (u_char);
    outp = outpacket_buf;
    MAKEHEADER(outp, PPP_EAP);		/* paste in a EAP header */
    PUTCHAR(EAP_REQUEST, outp);
    PUTCHAR(cstate->req_id, outp);
    PUTSHORT(outlen, outp);
    PUTCHAR(cstate->req_type, outp);

    output(cstate->unit, outpacket_buf, outlen + PPP_HDRLEN);

    TIMEOUT(EapChallengeTimeout, cstate, cstate->timeouttime);
    ++cstate->req_transmits;
}

/*
 * EAPAllowedAddr - check with the plugin if the address is OK.
 */
int
EAPAllowedAddr(unit, addr)
    int unit;
    u_int32_t addr;
{
    // always say OK for now.
    return 1;
}

/*
 * EapExtAdd - add a new eap type handler.
 */
int
EapExtAdd(eap_ext *newext)
{
    eap_ext *eap, *last;

    for (last = eap = eap_extensions; eap; last = eap, eap = eap->next) {
        if (eap->type == newext->type)
            return 1; // already exists
    }

	if (last) 
		last->next = newext;
	else 
		eap_extensions = newext;
	newext->next = NULL;
    return 0;
}

/*
 * EapSupportedType - check if an eap type is supported with the specified flags.
 */
static 
eap_ext * EapSupportedType(int type)
{
    eap_ext 	*eap;
    u_int32_t	flags = 0;
    

    for (eap = eap_extensions; eap; eap = eap->next) {
       if (eap->type == type
            && (eap->flags & flags) == flags)
            return eap;
    }
    
    return 0;
}

/*
 * EapReceiveRequest - Receive Challenge and send Response.
 */
static void
EapReceiveRequest(cstate, inpacket, packet_len, inp, id, len)
    eap_state *cstate;
    u_char *inpacket;
    int packet_len;
    u_char *inp;
    int id;
    int len;
{
    int req, err, outlen;
    u_char *outp;
    struct eap_ext	*eap;

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

    switch (req) {
        case EAP_TYPE_IDENTITY:
            outlen = EAP_HEADERLEN + sizeof(char) + strlen(cstate->our_identity);
            outp = outpacket_buf;
            MAKEHEADER(outp, PPP_EAP);
            PUTCHAR(EAP_RESPONSE, outp);	/* we are a response */
            PUTCHAR(id, outp);	/* copy id from request packet */
            PUTSHORT(outlen, outp);	/* packet length */
            PUTCHAR(req, outp);		/* copy type from request packet */
            BCOPY(cstate->our_identity, outp, outlen - EAP_HEADERLEN - sizeof(char)); /* append the response */

            /* send the packet */
            output(cstate->unit, outpacket_buf, outlen + PPP_HDRLEN);
            break;
            
        case EAP_TYPE_NOTIFICATION:
			
			/* print the remote message */
			PRINTMSG(inp, len);
			
            /* send an empty response */
			outlen = EAP_HEADERLEN;
            outp = outpacket_buf;
            MAKEHEADER(outp, PPP_EAP);
            PUTCHAR(EAP_RESPONSE, outp);	/* we are a response */
            PUTCHAR(id, outp);	/* copy id from request packet */
            PUTSHORT(outlen, outp);	/* packet length */
            /* send the packet */
            output(cstate->unit, outpacket_buf, outlen + PPP_HDRLEN);
            break;

        case EAP_TYPE_NAK:
            break;
            
        default:
            if (cstate->client_ext) {
                /* if a request was already received, check that we get the same type */
                if (cstate->client_ext->type != req) {
                    error("EAP received an unexpected request for type %d", req);
                    break;
                }
            }
            else {
                /* first time, create client eap extension context */
                eap = EapSupportedType(req);
                if (eap == NULL) {
					error("EAP refuse to authenticate using type %d", req);

					/* send a NAK with the type we support */
					if (eap_extensions) {
						error("EAP send NAK requesting type %d", eap_extensions->type);
						outlen = EAP_HEADERLEN + sizeof(char) + sizeof(char);
						outp = outpacket_buf;
						MAKEHEADER(outp, PPP_EAP);
						PUTCHAR(EAP_RESPONSE, outp);	/* we are a response */
						PUTCHAR(id, outp);	/* copy id from request packet */
						PUTSHORT(outlen, outp);	/* packet length */
						PUTCHAR(EAP_TYPE_NAK, outp);		/* type NAK */
						PUTCHAR(eap_extensions->type, outp);		/* copy the type we prefer */

						/* send the packet */
						output(cstate->unit, outpacket_buf, outlen + PPP_HDRLEN);
					}
                    break;
                }

                /* init client context on first request */
                /* we don't necessarily know peer_name at this point */
                cstate->client_ext = eap;

                cstate->client_ext_input = (EAP_Input *)malloc(sizeof(EAP_Input));
                cstate->client_ext_output = (EAP_Output *)malloc(sizeof(EAP_Output));
                if (cstate->client_ext_input == 0 || cstate->client_ext_output == 0)
                    novm("Couldn't allocate memory for EAP Plugin data");
                
                /* this part is initialized only once */
                cstate->client_ext_input->size = sizeof(EAP_Input);
                cstate->client_ext_output->size = sizeof(EAP_Output);
                cstate->client_ext_input->mode = 0; // client mode
                cstate->client_ext_input->initial_id = 0; // no sense in client mode
                cstate->client_ext_input->mtu = netif_get_mtu(cstate->unit);
                cstate->client_ext_input->identity = cstate->our_identity;
                cstate->client_ext_input->username = cstate->username;
                cstate->client_ext_input->password = cstate->password;
                cstate->client_ext_input->log_debug = dbglog;
                cstate->client_ext_input->log_error = error;

                /* this part depends on the message */
            	cstate->client_ext_input->notification = EAP_NOTIFICATION_NONE; // no notification in the init message
                cstate->client_ext_input->data = cstate->client_ext->plugin; // bundle ref
                cstate->client_ext_input->data_len = 0;
                
                err = cstate->client_ext->init(cstate->client_ext_input, &cstate->client_ext_ctx);
                if (err) {
                    error("EAP cannot initialize plugin for %s (request type %d)", eap->name ? eap->name : "???", req);
					auth_withpeer_fail(cstate->unit, PPP_EAP);
                    break;
                }
            }
            
            /* process the request */
            EAPClientProcess(cstate, EAP_NOTIFICATION_PACKET, inpacket, packet_len);
            
    }
    
}

/*
 * EapReceiveResponse - Receive and process response.
 */
static void
EapReceiveResponse(cstate, inpacket, packet_len,  inp, id, len)
    eap_state *cstate;
    u_char *inpacket;
    int packet_len;
    u_char *inp;
    int id;
    int len;
{
    u_char type, auth_type;
    int  err;
    eap_ext	*eap, *last_eap;

    if (cstate->serverstate == EAPSS_CLOSED ||
	cstate->serverstate == EAPSS_PENDING) {
	EAPDEBUG(("EapReceiveResponse: in state %d", cstate->serverstate));
	return;
    }

    if (id != cstate->req_id)
	return;			/* doesn't match ID of last challenge */

    if (len < 1) {
	EAPDEBUG(("EapReceiveResponse: rcvd short packet."));
	return;
    }

    GETCHAR(type, inp);		/* get type */
    len -= 1;
    if ((type != EAP_TYPE_NAK && cstate->req_type && type != cstate->req_type)
		|| (type == EAP_TYPE_NAK && cstate->server_ext == 0)) {
	EAPDEBUG(("EapReceiveResponse: type doesn't match our request."));
	return;			/* doesn't match type of last challenge */
    }

    UNTIMEOUT(EapChallengeTimeout, cstate);            

    switch (type) { 

        case EAP_TYPE_IDENTITY:
            
            if (len >= MAX_NAME_LENGTH)
                len = MAX_NAME_LENGTH - 1;
        
            BCOPY(inp, cstate->peer_identity, len);
            cstate->peer_identity[len] = 0;

            /* XXX : Lookup to find out the protocol to use based on identity */

			/* use the first plugin available */
			eap = eap_extensions; 

            if (eap == NULL) {
                error("No EAP server protocol available");
				cstate->serverstate = EAPSS_BADAUTH;
				auth_peer_fail(cstate->unit, PPP_EAP);
                break;
            }

			/* no break */
			
        case EAP_TYPE_NAK:

			if (type == EAP_TYPE_NAK) {
								
				GETCHAR(auth_type, inp);		/* get authentication type */
				len -= 1;

				last_eap = cstate->server_ext;

                /* check if we support the type desired by the client */
                eap = EapSupportedType(auth_type);
                if (eap == NULL && last_eap->type == 0) {
					/* if we don't support the specfic type, but are currently doing type 0 (eap radius proxy)
						then just process the NAK */
					EAPServerProcess(cstate, EAP_NOTIFICATION_PACKET, inpacket, packet_len);
					break;
				}
				
				/* free previous selected eap module */ 
				cstate->server_ext->dispose(cstate->server_ext_ctx);
				free (cstate->server_ext_input);
				free (cstate->server_ext_output);
				cstate->server_ext = 0;
				cstate->server_ext_ctx = 0;
				cstate->server_ext_input = 0;
				cstate->server_ext_output = 0;
				
				if (eap == NULL) {
				
					/* if not supported, then use next in list */
					eap = last_eap->next; 
					if (eap == NULL) {
						error("Server and client disagree on EAP type");
						cstate->serverstate = EAPSS_BADAUTH;
						auth_peer_fail(cstate->unit, PPP_EAP);
						break;
					}
                }
			}
                                   
            cstate->server_ext = eap;
            cstate->req_type = eap->type;

            cstate->server_ext_input = (EAP_Input *)malloc(sizeof(EAP_Input));
            cstate->server_ext_output = (EAP_Output *)malloc(sizeof(EAP_Output));
            if (cstate->server_ext_input == 0 || cstate->server_ext_output == 0)
                novm("Couldn't allocate memory for EAP Plugin data");

            /* this part is initialized only once */
            cstate->server_ext_input->size = sizeof(EAP_Input);
            cstate->server_ext_output->size = sizeof(EAP_Output);
            cstate->server_ext_input->mode = 1; // server mode
            cstate->server_ext_input->initial_id = cstate->req_id + 1;
            cstate->server_ext_input->mtu = netif_get_mtu(cstate->unit);
            cstate->server_ext_input->identity = cstate->peer_identity;
            cstate->server_ext_input->username = 0; /* irrelevant in server mode */
            cstate->server_ext_input->password = 0; /* irrelevant in server mode */
            cstate->server_ext_input->log_debug = dbglog;
            cstate->server_ext_input->log_error = error;
 
            /* this part depends on the message */
            cstate->server_ext_input->notification = EAP_NOTIFICATION_NONE; // no notification in the init message
            cstate->server_ext_input->data = cstate->server_ext->plugin;
            cstate->server_ext_input->data_len = 0;
            
            err = cstate->server_ext->init(cstate->server_ext_input, &cstate->server_ext_ctx);
            if (err) {
                error("EAP cannot initialize plugin for %s (request type %d)", eap->name ? eap->name : "???", cstate->req_type);
                break;
            }
            
            /* now, start conversation */
            EAPServerProcess(cstate, EAP_NOTIFICATION_START, 0, 0);
            break;
        
        default:

            EAPServerProcess(cstate, EAP_NOTIFICATION_PACKET, inpacket, packet_len);
    }
}

/*
 * EapReceiveSuccess - Receive Success
 */
static void
EapReceiveSuccess(cstate, inpacket, packet_len, inp, id, len)
    eap_state *cstate;
    u_char *inpacket;
    int packet_len;
    u_char *inp;
    u_char id;
    int len;
{

    if (cstate->clientstate == EAPCS_OPEN)
	/* presumably an answer to a duplicate response */
	return;

    EAPClientProcess(cstate, EAP_NOTIFICATION_PACKET, inpacket, packet_len);
}


/*
 * EapReceiveFailure - Receive failure.
 */
static void
EapReceiveFailure(cstate, inpacket, packet_len, inp, id, len)
    eap_state *cstate;
    u_char *inpacket;
    int packet_len;
    u_char *inp;
    u_char id;
    int len;
{

    EAPClientProcess(cstate, EAP_NOTIFICATION_PACKET, inpacket, packet_len);
}

/*
 * EAPClientProcess - Process a packet in a client context.
 */
static int
EAPClientProcess(cstate, notification, inpacket, packet_len)
    eap_state *cstate;
    u_int16_t notification;
    u_char *inpacket;
    int packet_len;
{
    int err;
    
    if (cstate->client_ext == 0)
        /* ignore the request */
    	return 0;

    /* setup in and out structures */
    cstate->client_ext_input->notification = notification;
    cstate->client_ext_input->data = inpacket;
    cstate->client_ext_input->data_len = packet_len;
    cstate->client_ext_output->action = EAP_ACTION_NONE;
    cstate->client_ext_output->data = 0;
    cstate->client_ext_output->data_len = 0;
    cstate->client_ext_output->username = 0;

    err = cstate->client_ext->process(cstate->client_ext_ctx, cstate->client_ext_input, cstate->client_ext_output);
    if (err) {
        error("EAP error while processing packet for %s (request type %d, error %d)", 
                cstate->client_ext->name ? cstate->client_ext->name : "???", cstate->client_ext->type, err);
        return -1;
    }

    EAPClientAction(cstate);
    return 0;
}

/*
 * EAPClientGetAttributes - get client specific attributes.
 */
static void
EAPClientGetAttributes(cstate)
    eap_state *cstate;
{
    EAP_Attribute attribute;
    int err;
    char *str;
    
    /* let's see it we have mppe keys */
    if (cstate->client_ext->attribute == 0)
        return;

    attribute.type = EAP_ATTRIBUTE_MPPE_SEND_KEY;
    err = cstate->client_ext->attribute(cstate->client_ext_ctx, &attribute);
    if (err) {
        str = "MPPE_SEND_KEY";
        goto bad;
    }

    bcopy(attribute.data, mppe_send_key, MIN(attribute.data_len, MPPE_MAX_KEY_LEN));

    attribute.type = EAP_ATTRIBUTE_MPPE_RECV_KEY;
    err = cstate->client_ext->attribute(cstate->client_ext_ctx, &attribute);
    if (err) {
        str = "MPPE_RECV_KEY";
        goto bad;
    }

    bcopy(attribute.data, mppe_recv_key, MIN(attribute.data_len, MPPE_MAX_KEY_LEN));
    
    return;
    
bad:
    dbglog("EAP plugin %s (type %d) does not have %s attribute", 
        cstate->client_ext->name ? cstate->client_ext->name : "???", cstate->client_ext->type, str);
        
}

/*
 * EAPInput_fd - called when activity occurs on a file descriptor, 
 * so eap has a chance to test its file descriptors.
 */
void EAPInput_fd(void)
{
    int unit = 0;
    eap_state *cstate = &eap[unit];
    char	result;

    if (cstate->client_ext_ui_fds[0] != -1 && is_ready_fd(cstate->client_ext_ui_fds[0])) {
    
        result = 0;
        read(cstate->client_ext_ui_fds[0], &result, 1);
        
        wait_input_hook = 0;
        remove_fd(cstate->client_ext_ui_fds[0]);
        close(cstate->client_ext_ui_fds[0]);
        close(cstate->client_ext_ui_fds[1]);
        cstate->client_ext_ui_fds[0] = -1;
        cstate->client_ext_ui_fds[1] = -1;

        if (result == -1) {
            error("EAP error while requesting user input for %s (request type %d)", 
                    cstate->client_ext->name ? cstate->client_ext->name : "???", cstate->client_ext->type);
            return;
        }

        EAPClientProcess(cstate, EAP_NOTIFICATION_DATA_FROM_UI, cstate->client_ext_ui_data, cstate->client_ext_ui_data_len);
    }
}

/*
 * EAPClientUIThread - XXX User interface thread.
 */
void *EAPClientUIThread(void *arg)
{
    int 	unit = (int)arg;
    eap_state 	*cstate = &eap[unit];
    char	result = -1;
    int 	err;

    if (pthread_detach(pthread_self()) == 0) {
        
        if (cstate->client_ext->interactive_ui) {
            err = cstate->client_ext->interactive_ui(cstate->client_ext_ui_data, cstate->client_ext_ui_data_len, 
                            &cstate->client_ext_ui_data, &cstate->client_ext_ui_data_len);
            if (err == 0)
                result = 0;
        }
    }

    write(eap->client_ext_ui_fds[1], &result, 1);
    return 0;
}

/*
 * EAPClientInvokeUI - Perform the action in the client context.
 */
static int
EAPClientInvokeUI(cstate)
    eap_state *cstate;
{
    if (pipe(cstate->client_ext_ui_fds) < 0) {
        error("EAP failed to create pipe for User Interface...\n");
        return -1;
    }

    if (pthread_create(&cstate->client_ui_thread, NULL, EAPClientUIThread, (void*)cstate->unit)) {
        error("EAP failed to create thread for client User Interface...\n");
        close(cstate->client_ext_ui_fds[0]);
        close(cstate->client_ext_ui_fds[1]);
        return -1;
    }
    
    wait_input_hook = EAPInput_fd;
    add_fd(cstate->client_ext_ui_fds[0]);
    return 0;
}

/*
 * EAPClientAction - Perform the action in the client context.
 */
static void
EAPClientAction(cstate)
    eap_state *cstate;
{
    EAP_Output *eap_out = cstate->client_ext_output;
    u_char *outp;

    switch (eap_out->action) {
        case EAP_ACTION_NONE:
            break;
            
        case EAP_ACTION_SEND_WITH_TIMEOUT:
        case EAP_ACTION_SEND_AND_DONE:
            // irrelevant for client
            break;

        case EAP_ACTION_SEND:
            if (eap_out->data == 0 ||
                eap_out->data_len < EAP_HEADERLEN ||
                eap_out->data_len > PPP_MRU) {
                error("EAP plugin tries to send a packet with with incorrect data");
                break;
            }

            outp = outpacket_buf;
            MAKEHEADER(outp, PPP_EAP);		/* paste in a EAP header */
            BCOPY(eap_out->data, outp, eap_out->data_len);
            
			cstate->resp_id = outp[1]; 	/* let's copy the id for future use */
			
            if (cstate->client_ext->free)
                cstate->client_ext->free(cstate->client_ext_ctx, eap_out);
                
            /* send the packet */
            output(cstate->unit, outpacket_buf, eap_out->data_len + PPP_HDRLEN);
            break;
            
        case EAP_ACTION_INVOKE_UI:
            cstate->client_ext_ui_data = eap_out->data;
            cstate->client_ext_ui_data_len = eap_out->data_len;
            EAPClientInvokeUI(cstate);
            break;
            
        case EAP_ACTION_ACCESS_GRANTED:
            EAPClientGetAttributes(cstate);
            cstate->clientstate = EAPCS_OPEN;
            auth_withpeer_success(cstate->unit, PPP_EAP, 0);
            break;
            
        case EAP_ACTION_ACCESS_DENIED:
            error("EAP authentication failed");
            auth_withpeer_fail(cstate->unit, PPP_EAP);
            break;

        case EAP_ACTION_CANCEL:
            auth_withpeer_cancelled(cstate->unit, PPP_EAP);
            break;
            
    }
}

/*
 * EAPServerProcess - Process a packet in a client context.
 */
static int
EAPServerProcess(cstate, notification, inpacket, packet_len)
    eap_state *cstate;
    u_int16_t notification;
    u_char *inpacket;
    int packet_len;
{
    int err;
    
    if (cstate->server_ext == 0)
        /* ignore the call */
    	return 0;

    /* setup in and out structures */
    cstate->server_ext_input->notification = notification;
    cstate->server_ext_input->data = inpacket;
    cstate->server_ext_input->data_len = packet_len;
    cstate->server_ext_output->action = EAP_ACTION_NONE;
    cstate->server_ext_output->data = 0;
    cstate->server_ext_output->data_len = 0;
    cstate->server_ext_output->username = 0;
    
    err = cstate->server_ext->process(cstate->server_ext_ctx, cstate->server_ext_input, cstate->server_ext_output);
    if (err) {
        error("EAP error while processing packet for %s (request type %d, error %d)", 
                cstate->server_ext->name ? cstate->server_ext->name : "???", cstate->server_ext->type, err);
        return -1;
    }

    EAPServerAction(cstate);
    return 0;
}

/*
 * EAPServerGetAttributes - get server specific attributes.
 */
static void
EAPServerGetAttributes(cstate)
    eap_state *cstate;
{
    EAP_Attribute attribute;
    int err;
    char *str;
    
    /* let's see it we have mppe keys */
    if (cstate->server_ext->attribute == 0)
        return;

    attribute.type = EAP_ATTRIBUTE_MPPE_SEND_KEY;
    err = cstate->server_ext->attribute(cstate->server_ext_ctx, &attribute);
    if (err) {
        str = "MPPE_SEND_KEY";
        goto bad;
    }

    bcopy(attribute.data, mppe_send_key, MIN(attribute.data_len, MPPE_MAX_KEY_LEN));

    attribute.type = EAP_ATTRIBUTE_MPPE_RECV_KEY;
    err = cstate->server_ext->attribute(cstate->server_ext_ctx, &attribute);
    if (err) {
        str = "MPPE_RECV_KEY";
        goto bad;
    }

    bcopy(attribute.data, mppe_recv_key, MIN(attribute.data_len, MPPE_MAX_KEY_LEN));
    
    return;
    
bad:
    dbglog("EAP plugin %s (type %d) does not have %s attribute", 
        cstate->server_ext->name ? cstate->server_ext->name : "???", cstate->server_ext->type, str);
        
}

/*
 * EAPClientAction - Perform the action in the client context.
 */
static void
EAPServerAction(cstate)
    eap_state *cstate;
{

    EAP_Output *eap_out = cstate->server_ext_output;
    u_char code, *outp;
    int old_state;
    char *name;
    
    switch (eap_out->action) {
        case EAP_ACTION_NONE:
            break;
            
        case EAP_ACTION_SEND_WITH_TIMEOUT:
        case EAP_ACTION_SEND_AND_DONE:
        case EAP_ACTION_SEND:
            if (eap_out->data == 0 ||
                eap_out->data_len < EAP_HEADERLEN ||
                eap_out->data_len > PPP_MRU) {
                error("EAP plugin tries to send a packet with with incorrect data");
                break;
            }
            outp = outpacket_buf;
            MAKEHEADER(outp, PPP_EAP);		/* paste in a EAP header */
            BCOPY(eap_out->data, outp, eap_out->data_len);

            code = outp[0];
            cstate->req_transmits = 0;
            cstate->req_id = outp[1]; 	/* let's copy the id for future use */
            
            if (cstate->server_ext->free)
                cstate->server_ext->free(cstate->server_ext_ctx, eap_out);
                
            /* send the packet */
            output(cstate->unit, outpacket_buf, eap_out->data_len + PPP_HDRLEN);
            
            if (eap_out->action == EAP_ACTION_SEND_WITH_TIMEOUT)
                TIMEOUT(EapChallengeTimeout, cstate, cstate->timeouttime);
            ++cstate->req_transmits;

            if (eap_out->action == EAP_ACTION_SEND_AND_DONE) {

                /*
                * If we have received a duplicate or bogus Response,
                * we have to send the same answer (Success/Failure)
                * as we did for the first Response we saw.
                * The packet we are sending is a result of this retransmission,
                * so nothing more to do.
                */
                if (cstate->serverstate == EAPSS_OPEN
                    || cstate->serverstate == EAPSS_BADAUTH)
                    break;

                /*
                * the eap server plugin is done. Let's see what is the result code.
                * the plugin can return a username to override the identity
                */
                name = eap_out->username ? eap_out->username : cstate->peer_identity;

                if (code == EAP_SUCCESS) {
                    UNTIMEOUT(EapChallengeTimeout, cstate);            
                    old_state = cstate->serverstate;
                    cstate->serverstate = EAPSS_OPEN;
                    if (old_state == EAPSS_INITIAL_CHAL) {
                        EAPServerGetAttributes(cstate);
                        auth_peer_success(cstate->unit, PPP_EAP, 0, name, strlen(name));
                    }
                    if (cstate->req_interval != 0)
                        TIMEOUT(EapRechallenge, cstate, cstate->req_interval);
                            
                    notice("EAP peer authentication succeeded for %s", name);
                }
                else 
                {
                    UNTIMEOUT(EapChallengeTimeout, cstate);
                    error("EAP peer authentication failed for remote host %s", name);
                    cstate->serverstate = EAPSS_BADAUTH;
                    auth_peer_fail(cstate->unit, PPP_EAP);        
                }
            }
            break;
            
        case EAP_ACTION_INVOKE_UI:            
        case EAP_ACTION_ACCESS_GRANTED:
        case EAP_ACTION_ACCESS_DENIED:
        case EAP_ACTION_CANCEL:
            // no used for server
            break;
    }
}

/*
 * EapPrintPkt - print the contents of a EAP packet.
 */
static char *EapCodenames[] = {
    "Request", "Response", "Success", "Failure"
};
static char *EapRequestnames[] = {
    "Identity", "Notification", "Nak"
};

static int
EapPrintPkt(p, plen, printer, arg)
    u_char *p;
    int plen;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
    int code, id, len, req;
    int clen;
    u_char x;
    eap_ext	*eap;

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
            printer(arg, " %s ", EapRequestnames[req-1]);
        else if (eap = EapSupportedType(req))
            printer(arg, " %s ", eap->name ? eap->name : "???");
        else
            printer(arg, " type=0x%x ", req);
        len -= 1;
        if (len == 0)
            break;
        switch (req) {
            case EAP_TYPE_IDENTITY: 
            case EAP_TYPE_NOTIFICATION: 
                printer(arg, "<");
                print_string((char *)p, len, printer, arg);
                printer(arg, ">");
                break;
            default :
                eap = EapSupportedType(req);
                if (eap && eap->print_packet) {
                    eap->print_packet(printer, arg, code, p, len);
                }
                else {
                    printer(arg, "<");
                    clen = len;
                    for (; clen > 0; clen--) {
                        GETCHAR(x, p);
                        printer(arg, "%.2x", x);
                    }
                    printer(arg, ">");
                }
        }
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

/*
 * EapGetClientSecret - get the secret for a given name.
 */
int
EapGetClientSecret(void *cookie, u_char *our_name, u_char *peer_name, u_char *secret, int *secretlen)
{
    eap_state *cstate = (eap_state *)cookie;

    /* get secret for authenticating ourselves with the specified host */
    if (!get_secret(cstate->unit, our_name, peer_name, secret, secretlen, 0)) {
        *secretlen = 0;		/* assume null secret if can't find one */
        warning("No EAP secret found for authenticating us to %s", peer_name);
        return 1;
    }
    return 0;
}

/*
 * EapGetServerSecret - get the secret for a given name.
 */
int
EapGetServerSecret(void *cookie, u_char *our_name, u_char *peer_name, u_char *secret, int *secretlen)
{
    eap_state *cstate = (eap_state *)cookie;
    
    /* get secret for authenticating ourselves with the specified host */
    if (!get_secret(cstate->unit, our_name, peer_name, secret, secretlen, 1)) {
        *secretlen = 0;		/* assume null secret if can't find one */
        warning("No EAP secret found for authenticating %s", peer_name);
        return 1;
    }
    return 0;
}

/*
 * eaploadplugin - load the eap plugin
 */
static int
eaploadplugin(argv)
    char **argv;
{
    char *arg = *argv;
    int err;
    eap_ext *eap;
    
    eap = (eap_ext *)malloc(sizeof(eap_ext));
    if (eap == 0)
        novm("Couldn't allocate memory for EAP plugin");
    
	bzero(eap, sizeof(eap_ext));

    err = sys_eaploadplugin(*argv, eap);
    if (err) {
	option_error("Couldn't load EAP plugin %s", arg);
		// continue without loading plugin
        return 1;
    }

    if (eap->init == 0 || eap->dispose == 0 || eap->process == 0) {
	option_error("EAP plugin %s has no Init() Dispose() or Process() function", arg);
        return 0;
    }
                    
    if (EapSupportedType(eap->type)) {
	option_error("EAP plugin %s is trying to use an already loaded EAP type %d", arg, eap->type);
        return 0;
    }
    
    EapExtAdd(eap);
    //info("Plugin %s loaded.", arg);

    return 1;
}

