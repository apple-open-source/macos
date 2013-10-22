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
 * chap-new.c - New CHAP implementation.
 *
 * Copyright (c) 2003 Paul Mackerras. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Paul Mackerras
 *     <paulus@samba.org>".
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "pppd.h"
#include "chap-new.h"
#include "chap-md5.h"

#ifdef CHAPMS
#include "chap_ms.h"
#endif

/* Hook for a plugin to validate CHAP challenge */
int (*chap_verify_hook)(u_char *name, u_char *ourname, int id,
			struct chap_digest_type *digest,
			unsigned char *challenge, unsigned char *response,
			unsigned char *message, int message_space) = NULL;

#ifdef __APPLE__
/* Hook for a plugin to validate unknown CHAP packets */
int (*chap_unknown_hook)(char *name, char *ourname, int code, int id,
			struct chap_digest_type *digest,
			unsigned char *challenge, unsigned char *pkt, int pkt_len,
			unsigned char *message, int message_space) = NULL;
#endif

/*
 * Option variables.
 */
int chap_timeout_time = 3;
int chap_max_transmits = 10;
int chap_rechallenge_time = 0;

/*
 * Command-line options.
 */
static option_t chap_option_list[] = {
	{ "chap-restart", o_int, &chap_timeout_time,
	  "Set timeout for CHAP", OPT_PRIO },
	{ "chap-max-challenge", o_int, &chap_max_transmits,
	  "Set max #xmits for challenge", OPT_PRIO },
	{ "chap-interval", o_int, &chap_rechallenge_time,
	  "Set interval for rechallenge", OPT_PRIO },
	{ NULL }
};

/*
 * These limits apply to challenge and response packets we send.
 * The +4 is the +1 that we actually need rounded up.
 */
#define CHAL_MAX_PKTLEN	(PPP_HDRLEN + CHAP_HDRLEN + 4 + MAX_CHALLENGE_LEN + MAXNAMELEN)
#define RESP_MAX_PKTLEN	(PPP_HDRLEN + CHAP_HDRLEN + 4 + MAX_RESPONSE_LEN + MAXNAMELEN)
#define CHGPWD_MAX_PKTLEN	(PPP_MRU)

/*
 * Internal state.
 */
static struct chap_client_state {
	int flags;
	char *name;
	struct chap_digest_type *digest;
	unsigned char priv[64];		/* private area for digest's use */
	int response_xmits;
	int response_pktlen;
	unsigned char response[RESP_MAX_PKTLEN];
} client;

static struct chap_server_state {
	int flags;
	int id;
	char *name;
	struct chap_digest_type *digest;
	int challenge_xmits;
	int challenge_pktlen;
	unsigned char challenge[CHAL_MAX_PKTLEN];
    unsigned char message[256];
} server;

/* Values for flags in chap_client_state and chap_server_state */
#define LOWERUP			1
#define AUTH_STARTED		2
#define AUTH_DONE		4
#define AUTH_FAILED		8
#define TIMEOUT_PENDING		0x10
#define CHALLENGE_VALID		0x20
#define RESPONSE_VALID      0x40

/*
 * Prototypes.
 */
static void chap_init(int unit);
static void chap_lowerup(int unit);
static void chap_lowerdown(int unit);
static void chap_timeout(void *arg);
static void chap_generate_challenge(struct chap_server_state *ss);
static void chap_handle_response(struct chap_server_state *ss, int code,
		unsigned char *pkt, int len);
static int chap_verify_response(u_char *name, u_char *ourname, int id,
		struct chap_digest_type *digest,
		unsigned char *challenge, unsigned char *response,
		unsigned char *message, int message_space);
static void chap_respond(struct chap_client_state *cs, int id,
		unsigned char *pkt, int len);
static void chap_handle_status(struct chap_client_state *cs, int code, int id,
		unsigned char *pkt, int len);
static void chap_protrej(int unit);
static void chap_input(int unit, unsigned char *pkt, int pktlen);
static int chap_print_pkt(unsigned char *p, int plen,
		void (*printer) __P((void *, char *, ...)), void *arg);

/* List of digest types that we know about */
static struct chap_digest_type *chap_digests;

#ifdef __APPLE__
static int		inside_UI = 0;
static int chap_ui_fds[2];	/* files descriptors for UI thread */
static   pthread_t chap_ui_thread; /* UI thread */
static unsigned char *chap_saved_packet = 0;
static int chap_ignore_failure = 0; /* ignore failure packet */
static u_int8_t chap_ignore_failure_id = 0; /* ignore failure packet id */
static unsigned char chap_last_message[MAXNAMELEN];
static int		ask_password_mode = 0; /* 0 for retry, 1 for change */
#endif

/*
 * chap_init - reset to initial state.
 */
static void
chap_init(int unit)
{
	memset(&client, 0, sizeof(client));
	memset(&server, 0, sizeof(server));

	chap_md5_init();
#ifdef CHAPMS
	chapms_init();
#endif
}

/*
 * Add a new digest type to the list.
 */
void
chap_register_digest(struct chap_digest_type *dp)
{
	dp->next = chap_digests;
	chap_digests = dp;
}

/*
 * chap_lowerup - we can start doing stuff now.
 */
static void
chap_lowerup(int unit)
{
	struct chap_client_state *cs = &client;
	struct chap_server_state *ss = &server;

#ifdef __APPLE__
	chapms_reinit();
#endif

	cs->flags |= LOWERUP;
	ss->flags |= LOWERUP;
	if (ss->flags & AUTH_STARTED)
		chap_timeout(ss);
}

static void
chap_status_timeout(void *arg)
{
	struct chap_client_state *cs = arg;
    
	cs->flags &= ~TIMEOUT_PENDING;
	if ((cs->flags & RESPONSE_VALID) == 0) {
		cs->response_xmits = 0;
		// resend response
		cs->flags |= RESPONSE_VALID;
	} else if (cs->response_xmits >= chap_max_transmits) {
		cs->flags &= ~RESPONSE_VALID;
		cs->flags |= AUTH_DONE | AUTH_FAILED;
		// give up on authen.
        auth_peer_fail(0, PPP_CHAP);
		return;
	}
    
	output(0, cs->response, cs->response_pktlen);
	++cs->response_xmits;
	cs->flags |= TIMEOUT_PENDING;
	TIMEOUT(chap_status_timeout, arg, chap_timeout_time);
}

static void
chap_lowerdown(int unit)
{
	struct chap_client_state *cs = &client;
	struct chap_server_state *ss = &server;

	if (cs->flags & TIMEOUT_PENDING)
		UNTIMEOUT(chap_status_timeout, cs);
	cs->flags = 0;
	if (ss->flags & TIMEOUT_PENDING)
		UNTIMEOUT(chap_timeout, ss);
	ss->flags = 0;

	if (inside_UI) {
		pthread_cancel(chap_ui_thread);
		inside_UI = 0;
	}
}

/*
 * chap_auth_peer - Start authenticating the peer.
 * If the lower layer is already up, we start sending challenges,
 * otherwise we wait for the lower layer to come up.
 */
void
chap_auth_peer(int unit, char *our_name, int digest_code)
{
	struct chap_server_state *ss = &server;
	struct chap_digest_type *dp;

	if (ss->flags & AUTH_STARTED) {
		error("CHAP: peer authentication already started!");
		return;
	}
	for (dp = chap_digests; dp != NULL; dp = dp->next)
		if (dp->code == digest_code)
			break;
	if (dp == NULL)
		fatal("CHAP digest 0x%x requested but not available",
		      digest_code);

	ss->digest = dp;
	ss->name = our_name;
	/* Start with a random ID value */
	ss->id = (unsigned char)(drand48() * 256);
	ss->flags |= AUTH_STARTED;
	if (ss->flags & LOWERUP)
		chap_timeout(ss);
}

/*
 * chap_auth_with_peer - Prepare to authenticate ourselves to the peer.
 * There isn't much to do until we receive a challenge.
 */
void
chap_auth_with_peer(int unit, char *our_name, int digest_code)
{
	struct chap_client_state *cs = &client;
	struct chap_digest_type *dp;

	if (cs->flags & AUTH_STARTED) {
		error("CHAP: authentication with peer already started!");
		return;
	}
	for (dp = chap_digests; dp != NULL; dp = dp->next)
		if (dp->code == digest_code)
			break;
	if (dp == NULL)
		fatal("CHAP digest 0x%x requested but not available",
		      digest_code);

	cs->digest = dp;
	cs->name = our_name;
	cs->flags |= AUTH_STARTED;
}

/*
 * chap_timeout - It's time to send another challenge to the peer.
 * This could be either a retransmission of a previous challenge,
 * or a new challenge to start re-authentication.
 */
static void
chap_timeout(void *arg)
{
	struct chap_server_state *ss = arg;

	ss->flags &= ~TIMEOUT_PENDING;
	if ((ss->flags & CHALLENGE_VALID) == 0) {
		ss->challenge_xmits = 0;
		chap_generate_challenge(ss);
		ss->flags |= CHALLENGE_VALID;
	} else if (ss->challenge_xmits >= chap_max_transmits) {
		ss->flags &= ~CHALLENGE_VALID;
		ss->flags |= AUTH_DONE | AUTH_FAILED;
		auth_peer_fail(0, PPP_CHAP);
		return;
	}

	output(0, ss->challenge, ss->challenge_pktlen);
	++ss->challenge_xmits;
	ss->flags |= TIMEOUT_PENDING;
	TIMEOUT(chap_timeout, arg, chap_timeout_time);
}

/*
 * chap_generate_challenge - generate a challenge string and format
 * the challenge packet in ss->challenge_pkt.
 */
static void
chap_generate_challenge(struct chap_server_state *ss)
{
	int clen = 1, nlen, len;
	unsigned char *p;

	p = ss->challenge;
	MAKEHEADER(p, PPP_CHAP);
	p += CHAP_HDRLEN;
	ss->digest->generate_challenge(p);
	clen = *p;
	nlen = strlen(ss->name);
	memcpy(p + 1 + clen, ss->name, nlen);

	len = CHAP_HDRLEN + 1 + clen + nlen;
	ss->challenge_pktlen = PPP_HDRLEN + len;

	p = ss->challenge + PPP_HDRLEN;
	p[0] = CHAP_CHALLENGE;
	p[1] = ++ss->id;
	p[2] = len >> 8;
	p[3] = len;
}

#ifdef __APPLE__
static char prev_name[MAXNAMELEN+1];
#endif
/*
 * chap_handle_response - check the response to our challenge.
 */
static void
chap_handle_response(struct chap_server_state *ss, int id,
		     unsigned char *pkt, int len)
{
	int response_len, ok = 0, mlen;
	unsigned char *response, *p;
	unsigned char *name = NULL;	/* initialized to shut gcc up */
	int (*verifier)(u_char *, u_char *, int, struct chap_digest_type *,
		unsigned char *, unsigned char *, unsigned char *, int);
	char rname[MAXNAMELEN+1];

	if ((ss->flags & LOWERUP) == 0)
		return;
	if (id != ss->challenge[PPP_HDRLEN+1] || len < 2)
		return;
    if (ss->flags & CHALLENGE_VALID) {
		response = pkt;
		GETCHAR(response_len, pkt);
		len -= response_len + 1;	/* length of name */
		name = pkt + response_len;
		if (len < 0)
			return;

		if (ss->flags & TIMEOUT_PENDING) {
			ss->flags &= ~TIMEOUT_PENDING;
			UNTIMEOUT(chap_timeout, ss);
		}

		if (explicit_remote) {
			name = (u_char*)remote_name;
		} else {
			/* Null terminate and clean remote name. */
			slprintf(rname, sizeof(rname), "%.*v", len, name);
			name = (u_char*)rname;
		}

		if (chap_verify_hook)
			verifier = chap_verify_hook;
		else
			verifier = chap_verify_response;
		ok = (*verifier)(name, (u_char*)ss->name, id, ss->digest,
				 ss->challenge + PPP_HDRLEN + CHAP_HDRLEN,
				 response, ss->message, sizeof(ss->message));
		if (!ok || !auth_number()) {
			ss->flags |= AUTH_FAILED;
			//warning("Peer %q failed CHAP authentication", name);
		}
	} else if ((ss->flags & AUTH_DONE) == 0)
        return;

	/* send the response */
	p = outpacket_buf;
	MAKEHEADER(p, PPP_CHAP);
	mlen = strlen((char*)ss->message);
	len = CHAP_HDRLEN + mlen;
	p[0] = ((ss->flags & AUTH_FAILED) || (ok == -1)) ? CHAP_FAILURE: CHAP_SUCCESS;
	p[1] = id;
	p[2] = len >> 8;
	p[3] = len;
	if (mlen > 0)
		memcpy(p + CHAP_HDRLEN, ss->message, mlen);
	output(0, outpacket_buf, PPP_HDRLEN + len);

#ifdef __APPLE__
	prev_name[0] = 0;
	if (ok == -1) {
		/* need to set timer to resend the failure message*/
		ss->flags |= CHALLENGE_VALID; // challenge is still valid
		ss->challenge[PPP_HDRLEN+1]++;  // but bump packet id
		
		strlcpy(prev_name, (char*)name, sizeof(prev_name));
		return;		/* just return */
	}
#endif

	if ((ss->flags & AUTH_DONE) == 0) {
		ss->flags &= ~CHALLENGE_VALID;
		ss->flags |= AUTH_DONE;
		if (ss->flags & AUTH_FAILED) {
			notice("CHAP peer authentication failed for %q", name);
			auth_peer_fail(0, PPP_CHAP);
		} else {
			notice("CHAP peer authentication succeeded for %q", name);
			auth_peer_success(0, PPP_CHAP, ss->digest->code,
					  name, strlen((char*)name));
			if (chap_rechallenge_time) {
				ss->flags |= TIMEOUT_PENDING;
				TIMEOUT(chap_timeout, ss,
					chap_rechallenge_time);
			}
		}
	}
}

#ifdef __APPLE__
/*
 * chap_handle_default - check handles for unknown packet.
 */
static void
chap_handle_default(struct chap_server_state *ss, int code, int id,
		     unsigned char *pkt, int len)
{
	int ok = 0, mlen;
	unsigned char *p;
	//unsigned char *name = NULL;	/* initialized to shut gcc up */

	if (!chap_unknown_hook)
		return;

	if ((ss->flags & LOWERUP) == 0)
		return;

    ss->message[0] = 0;

	if ((ss->flags & AUTH_DONE) == 0) {

		ok = (*chap_unknown_hook)(prev_name, ss->name, code, id, ss->digest,
			 ss->challenge + PPP_HDRLEN + CHAP_HDRLEN,
			 pkt, len, ss->message, sizeof(ss->message));
		if (!ok) {
			ss->flags |= AUTH_FAILED;
			//warning("Peer %q failed CHAP authentication", prev_name);
		}
	}

	if (ok == -2)
		return;
		
	/* send the response */
	p = outpacket_buf;
	MAKEHEADER(p, PPP_CHAP);
	mlen = strlen((char*)ss->message);
	len = CHAP_HDRLEN + mlen;
	p[0] = ((ss->flags & AUTH_FAILED) || (ok == -1)) ? CHAP_FAILURE: CHAP_SUCCESS;
	p[1] = id;
	p[2] = len >> 8;
	p[3] = len;
	if (mlen > 0)
		memcpy(p + CHAP_HDRLEN, ss->message, mlen);
	output(0, outpacket_buf, PPP_HDRLEN + len);

	if (ok == -1)
		return;		/* just return */

	if ((ss->flags & AUTH_DONE) == 0) {
		ss->flags |= AUTH_DONE;
		if (ss->flags & AUTH_FAILED) {
			notice("CHAP peer authentication failed for %q", prev_name);
			auth_peer_fail(0, PPP_CHAP);
		} else {
			notice("CHAP peer authentication succeeded for %q", prev_name);
			auth_peer_success(0, PPP_CHAP, ss->digest->code,
					  (u_char*)prev_name, strlen(prev_name));
			if (chap_rechallenge_time) {
				ss->flags |= TIMEOUT_PENDING;
				TIMEOUT(chap_timeout, ss,
					chap_rechallenge_time);
			}
		}
	}
}
#endif

/*
 * chap_verify_response - check whether the peer's response matches
 * what we think it should be.  Returns 1 if it does (authentication
 * succeeded), or 0 if it doesn't.
 */
static int
chap_verify_response(u_char *name, u_char *ourname, int id,
		     struct chap_digest_type *digest,
		     unsigned char *challenge, unsigned char *response,
		     unsigned char *message, int message_space)
{
	int ok;
	u_char secret[MAXSECRETLEN];
	int secret_len;

	/* Get the secret that the peer is supposed to know */
	if (!get_secret(0, name, ourname, secret, &secret_len, 1)) {
		error("No CHAP secret found for authenticating %q", name);
		return 0;
	}

	ok = digest->verify_response(id, (char*)name, secret, secret_len, challenge,
				     response, (char*)message, message_space);
	memset(secret, 0, sizeof(secret));

	return ok;
}

/*
 * chap_respond - Generate and send a response to a challenge.
 */
static void
chap_respond(struct chap_client_state *cs, int id,
	     unsigned char *pkt, int len)
{
	int clen, nlen;
	int secret_len;
	unsigned char *p;
	unsigned char response[RESP_MAX_PKTLEN];
	u_char rname[MAXNAMELEN+1];
	u_char secret[MAXSECRETLEN+1];

	if ((cs->flags & (LOWERUP | AUTH_STARTED)) != (LOWERUP | AUTH_STARTED))
		return;		/* not ready */
	if (len < 2 || len < pkt[0] + 1)
		return;		/* too short */
	if (cs->flags & TIMEOUT_PENDING) {
	        cs->flags &= ~TIMEOUT_PENDING;
		UNTIMEOUT(chap_status_timeout, cs);
	}

	clen = pkt[0];
	nlen = len - (clen + 1);

	/* Null terminate and clean remote name. */
	slprintf((char*)rname, sizeof(rname), "%.*v", nlen, pkt + clen + 1);

	/* Microsoft doesn't send their name back in the PPP packet */
	if (explicit_remote || (remote_name[0] != 0 && rname[0] == 0))
		strlcpy((char*)rname, remote_name, sizeof(rname));
	
	/* get secret for authenticating ourselves with the specified host */
	if (!get_secret(0, (u_char*)cs->name, rname, secret, &secret_len, 0)) {
		secret_len = 0;	/* assume null secret if can't find one */
		warning("No CHAP secret found for authenticating us to %q", rname);
	}

	chap_ignore_failure = 0;
	
	p = response;
	MAKEHEADER(p, PPP_CHAP);
	p += CHAP_HDRLEN;

	cs->digest->make_response(p, id, cs->name, pkt,
				  secret, secret_len, cs->priv);
	memset(secret, 0, secret_len);

	clen = *p;
	nlen = strlen(cs->name);
	memcpy(p + clen + 1, cs->name, nlen);

	p = response + PPP_HDRLEN;
	len = CHAP_HDRLEN + clen + 1 + nlen;
	p[0] = CHAP_RESPONSE;
	p[1] = id;
	p[2] = len >> 8;
	p[3] = len;

	output(0, response, PPP_HDRLEN + len);
    cs->response_pktlen = PPP_HDRLEN + len;
    memcpy(cs->response, response, cs->response_pktlen);
    cs->flags &= ~AUTH_DONE;
    cs->flags |= TIMEOUT_PENDING;
    TIMEOUT(chap_status_timeout, cs,
            chap_timeout_time);
}

static void
chap_wait_input_fd(void)
{
    char	result;
	int		nlen, err;
	int secret_len;
	unsigned char *p;
	unsigned char response[CHGPWD_MAX_PKTLEN];
	u_char rname[MAXNAMELEN+1];
	u_char secret[MAXSECRETLEN+1];
	struct chap_client_state *cs = &client;
	
    if (chap_ui_fds[0] != -1 && is_ready_fd(chap_ui_fds[0])) {
    
		inside_UI = 0;
        result = 0;
        read(chap_ui_fds[0], &result, 1);
        
        wait_input_hook = 0;
        remove_fd(chap_ui_fds[0]);
        close(chap_ui_fds[0]);
        close(chap_ui_fds[1]);
        chap_ui_fds[0] = -1;
        chap_ui_fds[1] = -1;

        if (result == 0) {
			/* user cancellation */
			free(chap_saved_packet);
			chap_saved_packet = 0;
            if (cs->flags & TIMEOUT_PENDING) {
                cs->flags &= ~TIMEOUT_PENDING;
                UNTIMEOUT(chap_status_timeout, cs);
            }
			cs->flags |= AUTH_DONE;
			auth_withpeer_cancelled(0, PPP_CHAP);
            return;
        }

		/* XXX should memorize name from previous challenge */
		rname[0] = 0;

		/* Microsoft doesn't send their name back in the PPP packet */
		if (explicit_remote || (remote_name[0] != 0 && rname[0] == 0))
			strlcpy((char*)rname, remote_name, sizeof(rname));

		/* get secret for authenticating ourselves with the specified host */
		if (!get_secret(0, (u_char*)cs->name, rname, secret, &secret_len, 0)) {
			secret_len = 0;	/* assume null secret if can't find one */
			warning("No CHAP secret found for authenticating us to %q", rname);
		}

		p = response;
		MAKEHEADER(p, PPP_CHAP);

		if (ask_password_mode == 1) {
			err = cs->digest->make_change_password(p, cs->name, chap_saved_packet, 
					  secret, secret_len, (u_char *)new_passwd, strlen(new_passwd),  cs->priv);
			/* save new password if possible */
			if (!err)
				save_new_password();
		}
		else 
			err = cs->digest->make_retry_password(p, cs->name, chap_saved_packet, 
					  secret, secret_len,  cs->priv);

		memset(secret, 0, secret_len);
		
		chap_ignore_failure = 1;
		chap_ignore_failure_id = chap_saved_packet[1];
		
		free(chap_saved_packet);
		chap_saved_packet = 0;

		if (err) {
			/* error occured, fail authentication */
			return;
		}
				
		nlen = p[2];
		nlen <<= 8;
		nlen += p[3];

		output(0, response, PPP_HDRLEN + nlen);
	}
}

static void
*chap_UIThread(void *arg)
{
    /* int 	unit = (int)arg; */ 
    char	result = -1;
    int 	err;

    if (pthread_detach(pthread_self()) == 0) {
        
		if (ask_password_mode == 1)
			err = (*change_password_hook)(chap_last_message[0] ? chap_last_message : 0);
		else 
			err = (*retry_password_hook)(chap_last_message[0] ? (u_char*)chap_last_message : 0);

		if (err == 0)
			result = 0;
    }

    write(chap_ui_fds[1], &result, 1);
    return 0;
}

static int
chap_invoke_ui()
{
    if (pipe(chap_ui_fds) < 0) {
        error("chap failed to create pipe for User Interface...\n");
        return -1;
    }

    if (pthread_create(&chap_ui_thread, NULL, chap_UIThread, 0 /* unit number */)) {
        error("chap failed to create thread for client User Interface...\n");
        close(chap_ui_fds[0]);
        close(chap_ui_fds[1]);
        return -1;
    }
    
    wait_input_hook = chap_wait_input_fd;
    add_fd(chap_ui_fds[0]);
    return 0;
}

/*
 * chap_ask_password - Generate and send a change password packet.
 * change is 0 for retry password mode, 1 for change password mode
 */
static int
chap_ask_password(struct chap_client_state *cs, int id,
	     unsigned char *pkt, int len, int change)
{
	int err;
	
	/* check if already processing request */
	if (inside_UI)
		return 0;

	/* UI is started, save packet */
	if (chap_saved_packet)
		free(chap_saved_packet);
	pkt -= CHAP_HDRLEN;
	chap_saved_packet = malloc(len + CHAP_HDRLEN);
	if (!chap_saved_packet) {
		error("CHAP: cannot allocate memory to save packet");
		return -1;
	}
	memcpy(chap_saved_packet, pkt, len + CHAP_HDRLEN);		
	ask_password_mode = change;
	
	err = chap_invoke_ui(/*change*/);
	if (err != 0) {
		free(chap_saved_packet);
		chap_saved_packet = 0;
		return -1;
	}
	
	inside_UI = 1;

	return 0;
}

static void
chap_handle_status(struct chap_client_state *cs, int code, int id,
		   unsigned char *pkt, int len)
{
	const char *msg = NULL;
	int oldflags = cs->flags;

	if ((cs->flags & (AUTH_DONE|AUTH_STARTED|LOWERUP))
	    != (AUTH_STARTED|LOWERUP))
		return;
	cs->flags |= AUTH_DONE;
    if (cs->flags & TIMEOUT_PENDING) {
        cs->flags &= ~TIMEOUT_PENDING;
        UNTIMEOUT(chap_status_timeout, cs);
    }
	if (code == CHAP_SUCCESS) {
		/* used for MS-CHAP v2 mutual auth, yuck */
		if (cs->digest->check_success != NULL) {
			if (!(*cs->digest->check_success)(pkt, len, cs->priv))
				code = CHAP_FAILURE;
		} else
			msg = "CHAP authentication succeeded";
	} else {
		if (cs->digest->handle_failure != NULL) {
			int ret;
			chap_last_message[0] = 0;
			ret = (*cs->digest->handle_failure)(pkt, len, (char*)chap_last_message, sizeof(chap_last_message));
			if (ret == 1) {
				if (chap_ignore_failure) {
					chap_ignore_failure = 0;
					if (chap_ignore_failure_id == id) {
						cs->flags &= ~AUTH_DONE;
						return;
					}
				}
				/* change password */
				cs->flags = oldflags;
				if (chap_ask_password(cs, id, pkt, len, 1) == 0)
					return;
				else 
					cs->flags |= AUTH_DONE;
			}
			else if (ret == 2) {
				if (chap_ignore_failure) {
					chap_ignore_failure = 0;
					if (chap_ignore_failure_id == id) {
						cs->flags &= ~AUTH_DONE;
						return;
					}
				}
				/* incorrect password, retry allowed */
				cs->flags = oldflags;
				if (chap_ask_password(cs, id, pkt, len, 0) == 0)
					return;
				else 
					cs->flags |= AUTH_DONE;
			}
		}
		else
			msg = "CHAP authentication failed";
	}
	if (msg) {
		if (len > 0)
			info("%s: %.*v", msg, len, pkt);
		else
			info("%s", msg);
	}
	if (code == CHAP_SUCCESS)
		auth_withpeer_success(0, PPP_CHAP, cs->digest->code);
	else {
		cs->flags |= AUTH_FAILED;
		auth_withpeer_fail(0, PPP_CHAP);
	}
}

static void
chap_input(int unit, unsigned char *pkt, int pktlen)
{
	struct chap_client_state *cs = &client;
	struct chap_server_state *ss = &server;
	unsigned char code, id;
	int len;

	if (pktlen < CHAP_HDRLEN)
		return;
	GETCHAR(code, pkt);
	GETCHAR(id, pkt);
	GETSHORT(len, pkt);
	if (len < CHAP_HDRLEN || len > pktlen)
		return;
	len -= CHAP_HDRLEN;

	switch (code) {
	case CHAP_CHALLENGE:
		chap_respond(cs, id, pkt, len);
		break;
	case CHAP_RESPONSE:
		chap_handle_response(ss, id, pkt, len);
		break;
	case CHAP_FAILURE:
	case CHAP_SUCCESS:
		chap_handle_status(cs, code, id, pkt, len);
		break;
#ifdef __APPLE__
	default:
		chap_handle_default(ss, code, id, pkt, len);
		break;
#endif
	}
}

static void
chap_protrej(int unit)
{
	struct chap_client_state *cs = &client;
	struct chap_server_state *ss = &server;

	if (ss->flags & TIMEOUT_PENDING) {
		ss->flags &= ~TIMEOUT_PENDING;
		UNTIMEOUT(chap_timeout, ss);
	}
	if (ss->flags & AUTH_STARTED) {
		ss->flags = 0;
		auth_peer_fail(0, PPP_CHAP);
	}
	if (cs->flags & TIMEOUT_PENDING) {
		cs->flags &= ~TIMEOUT_PENDING;
		UNTIMEOUT(chap_status_timeout, cs);
	}
	if ((cs->flags & (AUTH_STARTED|AUTH_DONE)) == AUTH_STARTED) {
		cs->flags &= ~AUTH_STARTED;
		auth_withpeer_fail(0, PPP_CHAP);
	}
}

/*
 * chap_print_pkt - print the contents of a CHAP packet.
 */
static char *chap_code_names[] = {
	"Challenge", "Response", "Success", "Failure"
};

static int
chap_print_pkt(unsigned char *p, int plen,
	       void (*printer) __P((void *, char *, ...)), void *arg)
{
	int code, id, len;
	int clen, nlen;
	unsigned char x;

	if (plen < CHAP_HDRLEN)
		return 0;
	GETCHAR(code, p);
	GETCHAR(id, p);
	GETSHORT(len, p);
	if (len < CHAP_HDRLEN || len > plen)
		return 0;

	if (code >= 1 && code <= sizeof(chap_code_names) / sizeof(char *))
		printer(arg, " %s", chap_code_names[code-1]);
	else
		printer(arg, " code=0x%x", code);
	printer(arg, " id=0x%x", id);
	len -= CHAP_HDRLEN;
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
		printer(arg, " <");
		for (; clen > 0; --clen) {
			GETCHAR(x, p);
			printer(arg, "%.2x", x);
		}
		printer(arg, ">, name = ");
		print_string((char *)p, nlen, printer, arg);
		break;
	case CHAP_FAILURE:
	case CHAP_SUCCESS:
		printer(arg, " ");
		print_string((char *)p, len, printer, arg);
		break;
	default:
		for (clen = len; clen > 0; --clen) {
			GETCHAR(x, p);
			printer(arg, " %.2x", x);
		}
	}

	return len + CHAP_HDRLEN;
}

struct protent chap_protent = {
	PPP_CHAP,
	chap_init,
	chap_input,
	chap_protrej,
	chap_lowerup,
	chap_lowerdown,
	NULL,		/* open */
	NULL,		/* close */
	chap_print_pkt,
	NULL,		/* datainput */
	1,		/* enabled_flag */
	"CHAP",		/* name */
	NULL,		/* data_name */
	chap_option_list,
	NULL,		/* check_options */
    NULL,
    NULL,
#ifdef __APPLE__
    NULL,
    NULL,
    NULL,
    NULL
#endif
};
