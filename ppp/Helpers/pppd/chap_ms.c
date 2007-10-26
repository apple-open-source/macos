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
 * chap_ms.c - Microsoft MS-CHAP compatible implementation.
 *
 * Copyright (c) 1995 Eric Rosenquist.  All rights reserved.
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
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Modifications by Lauri Pesonen / lpesonen@clinet.fi, april 1997
 *
 *   Implemented LANManager type password response to MS-CHAP challenges.
 *   Now pppd provides both NT style and LANMan style blocks, and the
 *   prefered is set by option "ms-lanman". Default is to use NT.
 *   The hash text (StdText) was taken from Win95 RASAPI32.DLL.
 *
 *   You should also use DOMAIN\\USERNAME as described in README.MSCHAP80
 */

/*
 * Modifications by Frank Cusack, frank@google.com, March 2002.
 *
 *   Implemented MS-CHAPv2 functionality, heavily based on sample
 *   implementation in RFC 2759.  Implemented MPPE functionality,
 *   heavily based on sample implementation in RFC 3079.
 *
 * Copyright (c) 2002 Google, Inc.  All rights reserved.
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
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#define RCSID	"$Id: chap_ms.c,v 1.9 2005/08/30 22:53:50 lindak Exp $"

#ifdef CHAPMS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>

#include "pppd.h"
#include "chap-new.h"
#include "chap_ms.h"
#ifdef __APPLE__
#define SHA1_CTX SHA_CTX
#define SHA1_SIGNATURE_SIZE SHA_DIGEST_LENGTH
#include "CommonCrypto/CommonDigest.h"
#else
#include "sha1.h"
#include "md4.h"
#endif
#include "pppcrypt.h"
#include "magic.h"

#ifndef lint
static const char rcsid[] = RCSID;
#endif


static void	ChallengeHash __P((u_char[16], u_char *, char *, u_char[8]));
static void	ascii2unicode __P((char[], int, u_char[]));
static void	NTPasswordHash __P((char *, int, u_char[MD4_SIGNATURE_SIZE]));
static void	ChallengeResponse __P((u_char *, u_char *, u_char[24]));
static void	ChapMS_NT __P((u_char *, char *, int, u_char[24]));
static void	ChapMS2_NT __P((char *, u_char[16], char *, char *, int,
				u_char[24]));
static void	GenerateAuthenticatorResponse __P((char*, int, u_char[24],
						   u_char[16], u_char *,
						   char *, u_char[41]));
#ifdef MSLANMAN
static void	ChapMS_LANMan __P((u_char *, char *, int, MS_ChapResponse *));
#endif

#ifdef MPPE
static void	Set_Start_Key __P((u_char *, char *, int));
static void	SetMasterKeys __P((char *, int, u_char[24], int));
#endif

#ifdef MSLANMAN
bool	ms_lanman = 0;    	/* Use LanMan password instead of NT */
			  	/* Has meaning only with MS-CHAP challenges */
#endif

#ifdef MPPE
u_char mppe_send_key[MPPE_MAX_KEY_LEN];
u_char mppe_recv_key[MPPE_MAX_KEY_LEN];
int mppe_keys_set = 0;		/* Have the MPPE keys been set? */

/* For MPPE debug */
/* Use "[]|}{?/><,`!2&&(" (sans quotes) for RFC 3079 MS-CHAPv2 test value */
static char *mschap_challenge = NULL;
/* Use "!@\#$%^&*()_+:3|~" (sans quotes, backslash is to escape #) for ... */
static char *mschap2_peer_challenge = NULL;

#ifdef __APPLE__
static u_char last_challenge_id = 0;
static char last_challenge_response[16] = "";
#endif

#include "fsm.h"		/* Need to poke MPPE options */
#include "ccp.h"
//#include <net/ppp-comp.h>
#include <ppp_comp.h>
#endif

/*
 * Command-line options.
 */
static option_t chapms_option_list[] = {
#ifdef MSLANMAN
	{ "ms-lanman", o_bool, &ms_lanman,
	  "Use LanMan passwd when using MS-CHAP", 1 },
#endif
#ifdef DEBUGMPPEKEY
	{ "mschap-challenge", o_string, &mschap_challenge,
	  "specify CHAP challenge" },
	{ "mschap2-peer-challenge", o_string, &mschap2_peer_challenge,
	  "specify CHAP peer challenge" },
#endif
	{ NULL }
};

/*
 * chapms_generate_challenge - generate a challenge for MS-CHAP.
 * For MS-CHAP the challenge length is fixed at 8 bytes.
 * The length goes in challenge[0] and the actual challenge starts
 * at challenge[1].
 */
static void
chapms_generate_challenge(unsigned char *challenge)
{
	*challenge++ = 8;
	if (mschap_challenge && strlen(mschap_challenge) == 8)
		memcpy(challenge, mschap_challenge, 8);
	else
		random_bytes(challenge, 8);
}

static void
chapms2_generate_challenge(unsigned char *challenge)
{
	*challenge++ = 16;
	if (mschap_challenge && strlen(mschap_challenge) == 16)
		memcpy(challenge, mschap_challenge, 16);
	else
		random_bytes(challenge, 16);
}

static int
chapms_verify_response(int id, char *name,
		       unsigned char *secret, int secret_len,
		       unsigned char *challenge, unsigned char *response,
		       char *message, int message_space)
{
	MS_ChapResponse *rmd;
	MS_ChapResponse md;
	int diff;
	int challenge_len, response_len;

	challenge_len = *challenge++;	/* skip length, is 8 */
	response_len = *response++;
	if (response_len != MS_CHAP_RESPONSE_LEN)
		goto bad;

	rmd = (MS_ChapResponse *) response;

#ifndef MSLANMAN
	if (!rmd->UseNT[0]) {
		/* Should really propagate this into the error packet. */
		notice("Peer request for LANMAN auth not supported");
		goto bad;
	}
#endif

	/* Generate the expected response. */
	ChapMS(challenge, secret, secret_len, &md);

#ifdef MSLANMAN
	/* Determine which part of response to verify against */
	if (!rmd->UseNT[0])
		diff = memcmp(&rmd->LANManResp, &md.LANManResp,
			      sizeof(md.LANManResp));
	else
#endif
		diff = memcmp(&rmd->NTResp, &md.NTResp, sizeof(md.NTResp));

	if (diff == 0) {
		slprintf(message, message_space, "Access granted");
		return 1;
	}

 bad:
	/* See comments below for MS-CHAP V2 */
	slprintf(message, message_space, "E=691 R=1 C=%0.*B V=0",
		 challenge_len, challenge);
	return 0;
}

static int
chapms2_verify_response(int id, char *name,
			unsigned char *secret, int secret_len,
			unsigned char *challenge, unsigned char *response,
			char *message, int message_space)
{
	MS_Chap2Response *rmd;
	MS_Chap2Response md;
	char saresponse[MS_AUTH_RESPONSE_LENGTH+1];
	int challenge_len, response_len;

	challenge_len = *challenge++;	/* skip length, is 16 */
	response_len = *response++;
	if (response_len != MS_CHAP2_RESPONSE_LEN)
		goto bad;	/* not even the right length */

	rmd = (MS_Chap2Response *) response;

	/* Generate the expected response and our mutual auth. */
	ChapMS2(challenge, rmd->PeerChallenge, name,
		secret, secret_len, &md,
		saresponse, MS_CHAP2_AUTHENTICATOR);

	/* compare MDs and send the appropriate status */
	/*
	 * Per RFC 2759, success message must be formatted as
	 *     "S=<auth_string> M=<message>"
	 * where
	 *     <auth_string> is the Authenticator Response (mutual auth)
	 *     <message> is a text message
	 *
	 * However, some versions of Windows (win98 tested) do not know
	 * about the M=<message> part (required per RFC 2759) and flag
	 * it as an error (reported incorrectly as an encryption error
	 * to the user).  Since the RFC requires it, and it can be
	 * useful information, we supply it if the peer is a conforming
	 * system.  Luckily (?), win98 sets the Flags field to 0x04
	 * (contrary to RFC requirements) so we can use that to
	 * distinguish between conforming and non-conforming systems.
	 *
	 * Special thanks to Alex Swiridov <say@real.kharkov.ua> for
	 * help debugging this.
	 */
	if (memcmp(md.NTResp, rmd->NTResp, sizeof(md.NTResp)) == 0) {
		if (rmd->Flags[0])
			slprintf(message, message_space, "S=%s", saresponse);
		else
			slprintf(message, message_space, "S=%s M=%s",
				 saresponse, "Access granted");
		return 1;
	}

 bad:
	/*
	 * Failure message must be formatted as
	 *     "E=e R=r C=c V=v M=m"
	 * where
	 *     e = error code (we use 691, ERROR_AUTHENTICATION_FAILURE)
	 *     r = retry (we use 1, ok to retry)
	 *     c = challenge to use for next response, we reuse previous
	 *     v = Change Password version supported, we use 0
	 *     m = text message
	 *
	 * The M=m part is only for MS-CHAPv2.  Neither win2k nor
	 * win98 (others untested) display the message to the user anyway.
	 * They also both ignore the E=e code.
	 *
	 * Note that it's safe to reuse the same challenge as we don't
	 * actually accept another response based on the error message
	 * (and no clients try to resend a response anyway).
	 *
	 * Basically, this whole bit is useless code, even the small
	 * implementation here is only because of overspecification.
	 */
	slprintf(message, message_space, "E=691 R=1 C=%0.*B V=0 M=%s",
		 challenge_len, challenge, "Access denied");
	return 0;
}

static void
chapms_make_response(unsigned char *response, int id, char *our_name,
		     unsigned char *challenge, char *secret, int secret_len,
		     unsigned char *private)
{
	challenge++;	/* skip length, should be 8 */
	*response++ = MS_CHAP_RESPONSE_LEN;
	ChapMS(challenge, secret, secret_len, (MS_ChapResponse *) response);
}

static void
chapms2_make_response(unsigned char *response, int id, char *our_name,
		      unsigned char *challenge, char *secret, int secret_len,
		      unsigned char *private)
{
	challenge++;	/* skip length, should be 16 */
	*response++ = MS_CHAP2_RESPONSE_LEN;
	ChapMS2(challenge,  mschap2_peer_challenge  
#ifdef __APPLE__
			? mschap2_peer_challenge :
				(id == last_challenge_id && last_challenge_response[0] ? last_challenge_response : NULL), 
#endif		
		our_name,
		secret, secret_len,
		(MS_Chap2Response *) response, private,
		MS_CHAP2_AUTHENTICATEE);
#ifdef __APPLE__
	last_challenge_id = id;
#endif
}

struct rc4_state {
        u_char  perm[256];
        u_char  index1;
        u_char  index2;
};

static __inline void
swap_bytes(u_char *a, u_char *b)
{
        u_char temp;

        temp = *a;
        *a = *b;
        *b = temp;
}

/*
 * Initialize an RC4 state buffer using the supplied key,
 * which can have arbitrary length.
 */
static void
rc4_init(struct rc4_state *const state, const u_char *key, int keylen)
{
        u_char j;
        int i;

        /* Initialize state with identity permutation */
        for (i = 0; i < 256; i++)
                state->perm[i] = (u_char)i; 
        state->index1 = 0;
        state->index2 = 0;
  
        /* Randomize the permutation using key data */
        for (j = i = 0; i < 256; i++) {
                j += state->perm[i] + key[i % keylen]; 
                swap_bytes(&state->perm[i], &state->perm[j]);
        }
}

/*
 * Encrypt some data using the supplied RC4 state buffer.
 * The input and output buffers may be the same buffer.
 * Since RC4 is a stream cypher, this function is used
 * for both encryption and decryption.
 */
static void
rc4_crypt(struct rc4_state *const state,
        const u_char *inbuf, u_char *outbuf, int buflen)
{
        int i;
        u_char j;

        for (i = 0; i < buflen; i++) {

                /* Update modification indicies */
                state->index1++;
                state->index2 += state->perm[state->index1];

                /* Modify permutation */
                swap_bytes(&state->perm[state->index1],
                    &state->perm[state->index2]);

                /* Encrypt/decrypt next byte */
                j = state->perm[state->index1] + state->perm[state->index2];
                outbuf[i] = inbuf[i] ^ state->perm[j];
        }
}


static void
EncryptPwBlockWithPasswordHash(
   u_char	*UnicodePassword,
   int		UnicodePasswordLen,
   u_char	*PasswordHash,
   u_char *EncryptedPwBlock)
{
	struct rc4_state rcs; 
	
	u_char ClearPwBlock[MAX_NT_PASSWORD * 2 + 4];
	int offset = MAX_NT_PASSWORD * 2 - UnicodePasswordLen;
	
	/* Fill ClearPwBlock with random octet values */
	random_bytes(ClearPwBlock, sizeof(ClearPwBlock));
	
	memcpy(ClearPwBlock + offset, UnicodePassword, UnicodePasswordLen);

	// Fix Me: This needs to be little endian
	//*(u_int32_t*)&ClearPwBlock[MAX_NT_PASSWORD * 2] = UnicodePasswordLen;
	ClearPwBlock[MAX_NT_PASSWORD*2 + 0] = UnicodePasswordLen;
	ClearPwBlock[MAX_NT_PASSWORD*2 + 1] = UnicodePasswordLen >> 8;
	ClearPwBlock[MAX_NT_PASSWORD*2 + 2] = UnicodePasswordLen >> 16;
	ClearPwBlock[MAX_NT_PASSWORD*2 + 3] = UnicodePasswordLen >> 24;

	rc4_init(&rcs, PasswordHash, MD4_SIGNATURE_SIZE);
	rc4_crypt(&rcs, ClearPwBlock, EncryptedPwBlock, sizeof(ClearPwBlock));
}

static void
NewPasswordEncryptedWithOldNtPasswordHash(char *NewPassword, int NewPasswordLen,
	char *OldPassword, int OldPasswordLen,
	u_char *EncryptedPwBlock)
{
    u_char	unicodeOldPassword[MAX_NT_PASSWORD * 2];
    u_char	OldPasswordHash[MD4_SIGNATURE_SIZE];
    u_char	unicodeNewPassword[MAX_NT_PASSWORD * 2];

    /* Hash the Unicode version of the old password */
    ascii2unicode(OldPassword, OldPasswordLen, unicodeOldPassword);
    NTPasswordHash(unicodeOldPassword, OldPasswordLen * 2, OldPasswordHash);

    /* Unicode version of the new password */
    ascii2unicode(NewPassword, NewPasswordLen, unicodeNewPassword);

	EncryptPwBlockWithPasswordHash(unicodeNewPassword, NewPasswordLen * 2, 
					OldPasswordHash, EncryptedPwBlock);
}

static void
NtPasswordHashEncryptedWithBlock(u_char *PasswordHash, u_char *Block, u_char *Cypher)
{
    (void) DesSetkey(Block + 0);
    DesEncrypt(PasswordHash, Cypher + 0);
	
    (void) DesSetkey(Block + 7);
    DesEncrypt(PasswordHash + 8, Cypher + 8);
}

static void
OldNtPasswordHashEncryptedWithNewNtPasswordHash(char *NewPassword, int NewPasswordLen,
	char *OldPassword, int OldPasswordLen,
	u_char *EncryptedPasswordHash)
{
    u_char	unicodeOldPassword[MAX_NT_PASSWORD * 2];
    u_char	OldPasswordHash[MD4_SIGNATURE_SIZE];
    u_char	unicodeNewPassword[MAX_NT_PASSWORD * 2];
	u_char	NewPasswordHash[MD4_SIGNATURE_SIZE];

    /* Hash the Unicode version of the old password */
    ascii2unicode(OldPassword, OldPasswordLen, unicodeOldPassword);
    NTPasswordHash(unicodeOldPassword, OldPasswordLen * 2, OldPasswordHash);

    /* Hash the Unicode version of the new password */
    ascii2unicode(NewPassword, NewPasswordLen, unicodeNewPassword);
    NTPasswordHash(unicodeNewPassword, NewPasswordLen * 2, NewPasswordHash);

	NtPasswordHashEncryptedWithBlock(OldPasswordHash, NewPasswordHash,
									EncryptedPasswordHash);
}

static void
ascii2hex(u_char *text, int len, u_char *hex) 
{
	int i;
	for (i = 0; i < len; i++) {
		if (text[0] >= '0' && text[0] <= '9')
			hex[i] = text[0] - '0';
		else if (text[0] >= 'a' && text[0] <= 'f')
			hex[i] = text[0] - 'a' + 10;
		else if (text[0] >= 'A' && text[0] <= 'F')
			hex[i] = text[0] - 'A' + 10;

		hex[i] <<= 4;

		if (text[1] >= '0' && text[1] <= '9')
			hex[i] |= text[1] - '0';
		else if (text[1] >= 'a' && text[1] <= 'f')
			hex[i] |= text[1] - 'a' + 10;
		else if (text[1] >= 'A' && text[1] <= 'F')
			hex[i] |= text[1] - 'A' + 10;

		text += 2;
	}
}

static int
chapms2_change_password(unsigned char *response, char *our_name,
		      unsigned char *status_pkt,
			  char *secret, int secret_len,
			  char *new_secret, int new_secret_len,
		      unsigned char *private)
{
	int pktlen;
	unsigned char *p = response;
	
	/* status_pkt and response point to the beginning of the CHAP payload */
	pktlen = status_pkt[2];
	pktlen <<= 8; 
	pktlen += status_pkt[3];

	p[0] = MS_CHAP2_CHANGE_PASSWORD;
	p[1] = status_pkt[1] + 1; // id of failure + 1
	p[2] = (MS_CHAP2_CHANGE_PASSWORD_LEN + CHAP_HDRLEN) >> 8;
	p[3] = (unsigned char)(MS_CHAP2_CHANGE_PASSWORD_LEN + CHAP_HDRLEN);
	p += CHAP_HDRLEN;

	/* first copy encrypted password */
	NewPasswordEncryptedWithOldNtPasswordHash(new_secret, new_secret_len,
		secret, secret_len, p);
	p += 516;

	/* then copy encrypted hash */
	OldNtPasswordHashEncryptedWithNewNtPasswordHash(new_secret, new_secret_len,
		secret, secret_len, p);
	p += MD4_SIGNATURE_SIZE;
	
	status_pkt += CHAP_HDRLEN;
	pktlen -= CHAP_HDRLEN;
	/* then the response */
	for (; pktlen; pktlen--, status_pkt++) {
		if (!strncmp(status_pkt, " C=", 3)) {

			unsigned char challenge[MAX_CHALLENGE_LEN];
			
			ascii2hex(status_pkt + 3, sizeof(challenge), challenge);

			ChapMS2(challenge,  mschap2_peer_challenge,
				our_name,
				new_secret, new_secret_len,
				(MS_Chap2Response *) p, private,
				MS_CHAP2_AUTHENTICATEE);
			break;
		}
	}

	/* then add one zero value flag, not in regular response */
	p += sizeof(MS_Chap2Response);
	*p = 0;
	
	return 0;
}

static int
chapms2_retry_password(unsigned char *response, char *our_name,
		      unsigned char *status_pkt,
			  char *secret, int secret_len,
		      unsigned char *private)
{
	int pktlen, namelen = strlen(our_name);
	unsigned char *p = response;
	
	/* status_pkt and response point to the beginning of the CHAP payload */
	pktlen = status_pkt[2];
	pktlen <<= 8; 
	pktlen += status_pkt[3];

	p[0] = CHAP_RESPONSE;
	p[1] = status_pkt[1] + 1; // id of failure + 1
	p[2] = (MS_CHAP2_RESPONSE_LEN + 1 + namelen + CHAP_HDRLEN) >> 8;
	p[3] = (unsigned char)(MS_CHAP2_RESPONSE_LEN + 1 + namelen + CHAP_HDRLEN);
	p += CHAP_HDRLEN;
	
	status_pkt += CHAP_HDRLEN;
	pktlen -= CHAP_HDRLEN;
	/* then the response */
	for (; pktlen; pktlen--, status_pkt++) {
		if (!strncmp(status_pkt, " C=", 3)) {
			unsigned char challenge[MAX_CHALLENGE_LEN];
			
			ascii2hex(status_pkt + 3, sizeof(challenge), challenge);

			*p++ = MS_CHAP2_RESPONSE_LEN;
			ChapMS2(challenge,  mschap2_peer_challenge,
				our_name,
				secret, secret_len,
				(MS_Chap2Response *) p, private,
				MS_CHAP2_AUTHENTICATEE);
				
			p += MS_CHAP2_RESPONSE_LEN;
			memcpy(p, our_name, namelen);
			break;
		}
	}
	
	return 0;
}

static int
chapms2_check_success(unsigned char *msg, int len, unsigned char *private)
{
	if ((len < MS_AUTH_RESPONSE_LENGTH + 2) || strncmp(msg, "S=", 2)) {
		/* Packet does not start with "S=" */
		error("MS-CHAPv2 Success packet is badly formed.");
		return 0;
	}
	msg += 2;
	len -= 2;
	if (len < MS_AUTH_RESPONSE_LENGTH
	    || memcmp(msg, private, MS_AUTH_RESPONSE_LENGTH)) {
		/* Authenticator Response did not match expected. */
		error("MS-CHAPv2 mutual authentication failed.");
		return 0;
	}
	/* Authenticator Response matches. */
	msg += MS_AUTH_RESPONSE_LENGTH; /* Eat it */
	len -= MS_AUTH_RESPONSE_LENGTH;
	if ((len >= 3) && !strncmp(msg, " M=", 3)) {
		msg += 3; /* Eat the delimiter */
	} else if (len) {
		/* Packet has extra text which does not begin " M=" */
		error("MS-CHAPv2 Success packet is badly formed.");
		return 0;
	}
	return 1;
}

static int
chapms_handle_failure(unsigned char *inp, int len, char *message, int message_max_len)
{
	int err, ret = 0;
	char *p, *p1, *msg;

	/* We want a null-terminated string for strxxx(). */
	msg = malloc(len + 1);
	if (!msg) {
		notice("Out of memory in chapms_handle_failure");
		return 0;
	}
	BCOPY(inp, msg, len);
	msg[len] = 0;
	p = msg;

	/*
	 * Deal with MS-CHAP formatted failure messages; just print the
	 * M=<message> part (if any).  For MS-CHAP we're not really supposed
	 * to use M=<message>, but it shouldn't hurt.  See
	 * chapms[2]_verify_response.
	 */
	if (!strncmp(p, "E=", 2))
		err = strtol(p + 2, NULL, 10); /* Remember the error code. */
	else {
#ifdef __APPLE__
		p += len;
#endif
		goto print_msg; /* Message is badly formatted. */
	}

	if (len && ((p1 = strstr(p, " R=")) != NULL)) {
		/* R=x field found. */
		p1 += 3;
		if (*p1 == '1' && retry_password_hook)
			ret = 2;
	}

#ifdef __APPLE__
	if (err == MS_CHAP_ERROR_PASSWD_EXPIRED && change_password_hook)
		ret = 1;
#endif

	if (len && ((p = strstr(p, " M=")) != NULL)) {
		/* M=<message> field found. */
		p += 3;
#ifdef __APPLE__
		strncpy(message, p, message_max_len-1); 
		message[message_max_len] = 0;
#endif
	} else {
		/* No M=<message>; use the error code. */
		switch (err) {
		case MS_CHAP_ERROR_RESTRICTED_LOGON_HOURS:
			p = "E=646 Restricted logon hours";
			break;

		case MS_CHAP_ERROR_ACCT_DISABLED:
			p = "E=647 Account disabled";
			break;

		case MS_CHAP_ERROR_PASSWD_EXPIRED:
			p = "E=648 Password expired";
			break;

		case MS_CHAP_ERROR_NO_DIALIN_PERMISSION:
			p = "E=649 No dialin permission";
			break;

		case MS_CHAP_ERROR_AUTHENTICATION_FAILURE:
			p = "E=691 Authentication failure";
			break;

		case MS_CHAP_ERROR_CHANGING_PASSWORD:
#ifndef __APPLE__
			/* Should never see this, we don't support Change Password. */
#endif
			p = "E=709 Error changing password";
			break;

		default:
			free(msg);
			error("error %d", err);
			error("Unknown MS-CHAP authentication failure: %.*v",
			      len, inp);
			return 0;
		}
	}
print_msg:
	if (p != NULL)
		error("MS-CHAP authentication failed: %v", p);
	free(msg);
	return ret;
}


static void
ChallengeResponse(u_char *challenge,
		  u_char PasswordHash[MD4_SIGNATURE_SIZE],
		  u_char response[24])
{
    u_char    ZPasswordHash[21];

    BZERO(ZPasswordHash, sizeof(ZPasswordHash));
    BCOPY(PasswordHash, ZPasswordHash, MD4_SIGNATURE_SIZE);

#if 0
    dbglog("ChallengeResponse - ZPasswordHash %.*B",
	   sizeof(ZPasswordHash), ZPasswordHash);
#endif

    (void) DesSetkey(ZPasswordHash + 0);
    DesEncrypt(challenge, response + 0);
    (void) DesSetkey(ZPasswordHash + 7);
    DesEncrypt(challenge, response + 8);
    (void) DesSetkey(ZPasswordHash + 14);
    DesEncrypt(challenge, response + 16);

#if 0
    dbglog("ChallengeResponse - response %.24B", response);
#endif
}

static void
ChallengeHash(u_char PeerChallenge[16], u_char *rchallenge,
	      char *username, u_char Challenge[8])
    
{
    SHA1_CTX	sha1Context;
    u_char	sha1Hash[SHA1_SIGNATURE_SIZE];
    char	*user;

    /* remove domain from "domain\username" */
    if ((user = strrchr(username, '\\')) != NULL)
	++user;
    else
	user = username;

    SHA1_Init(&sha1Context);
    SHA1_Update(&sha1Context, PeerChallenge, 16);
    SHA1_Update(&sha1Context, rchallenge, 16);
    SHA1_Update(&sha1Context, user, strlen(user));
    SHA1_Final(sha1Hash, &sha1Context);

    BCOPY(sha1Hash, Challenge, 8);
}

/*
 * Convert the ASCII version of the password to Unicode.
 * This implicitly supports 8-bit ISO8859/1 characters.
 * This gives us the little-endian representation, which
 * is assumed by all M$ CHAP RFCs.  (Unicode byte ordering
 * is machine-dependent.)
 */
static void
ascii2unicode(char ascii[], int ascii_len, u_char unicode[])
{
    int i;

    BZERO(unicode, ascii_len * 2);
    for (i = 0; i < ascii_len; i++)
	unicode[i * 2] = (u_char) ascii[i];
}

static void
NTPasswordHash(char *secret, int secret_len, u_char hash[MD4_SIGNATURE_SIZE])
{
#ifdef __APPLE__
	CC_MD4(secret, secret_len, hash);

#else
#ifdef __NetBSD__
    /* NetBSD uses the libc md4 routines which take bytes instead of bits */
    int			mdlen = secret_len;
#else
    int			mdlen = secret_len * 8;
#endif
    MD4_CTX		md4Context;

    MD4Init(&md4Context);
    /* MD4Update can take at most 64 bytes at a time */
    while (mdlen > 512) {
	MD4Update(&md4Context, secret, 512);
	secret += 64;
	mdlen -= 512;
    }
    MD4Update(&md4Context, secret, mdlen);
    MD4Final(hash, &md4Context);
#endif

}

static void
ChapMS_NT(u_char *rchallenge, char *secret, int secret_len,
	  u_char NTResponse[24])
{
    u_char	unicodePassword[MAX_NT_PASSWORD * 2];
    u_char	PasswordHash[MD4_SIGNATURE_SIZE];

    /* Hash the Unicode version of the secret (== password). */
    ascii2unicode(secret, secret_len, unicodePassword);
    NTPasswordHash(unicodePassword, secret_len * 2, PasswordHash);

    ChallengeResponse(rchallenge, PasswordHash, NTResponse);
}

static void
ChapMS2_NT(char *rchallenge, u_char PeerChallenge[16], char *username,
	   char *secret, int secret_len, u_char NTResponse[24])
{
    u_char	unicodePassword[MAX_NT_PASSWORD * 2];
    u_char	PasswordHash[MD4_SIGNATURE_SIZE];
    u_char	Challenge[8];

    ChallengeHash(PeerChallenge, rchallenge, username, Challenge);

    /* Hash the Unicode version of the secret (== password). */
    ascii2unicode(secret, secret_len, unicodePassword);
    NTPasswordHash(unicodePassword, secret_len * 2, PasswordHash);

    ChallengeResponse(Challenge, PasswordHash, NTResponse);
}

#ifdef MSLANMAN
static u_char *StdText = (u_char *)"KGS!@#$%"; /* key from rasapi32.dll */

static void
ChapMS_LANMan(u_char *rchallenge, char *secret, int secret_len,
	      MS_ChapResponse *response)
{
    int			i;
    u_char		UcasePassword[MAX_NT_PASSWORD]; /* max is actually 14 */
    u_char		PasswordHash[MD4_SIGNATURE_SIZE];

    /* LANMan password is case insensitive */
    BZERO(UcasePassword, sizeof(UcasePassword));
    for (i = 0; i < secret_len; i++)
       UcasePassword[i] = (u_char)toupper(secret[i]);
    (void) DesSetkey(UcasePassword + 0);
    DesEncrypt( StdText, PasswordHash + 0 );
    (void) DesSetkey(UcasePassword + 7);
    DesEncrypt( StdText, PasswordHash + 8 );
    ChallengeResponse(rchallenge, PasswordHash, response->LANManResp);
}
#endif


static void
GenerateAuthenticatorResponse(char *secret, int secret_len,
			      u_char NTResponse[24], u_char PeerChallenge[16],
			      u_char *rchallenge, char *username,
			      u_char authResponse[MS_AUTH_RESPONSE_LENGTH+1])
{
    /*
     * "Magic" constants used in response generation, from RFC 2759.
     */
    u_char Magic1[39] = /* "Magic server to client signing constant" */
	{ 0x4D, 0x61, 0x67, 0x69, 0x63, 0x20, 0x73, 0x65, 0x72, 0x76,
	  0x65, 0x72, 0x20, 0x74, 0x6F, 0x20, 0x63, 0x6C, 0x69, 0x65,
	  0x6E, 0x74, 0x20, 0x73, 0x69, 0x67, 0x6E, 0x69, 0x6E, 0x67,
	  0x20, 0x63, 0x6F, 0x6E, 0x73, 0x74, 0x61, 0x6E, 0x74 };
    u_char Magic2[41] = /* "Pad to make it do more than one iteration" */
	{ 0x50, 0x61, 0x64, 0x20, 0x74, 0x6F, 0x20, 0x6D, 0x61, 0x6B,
	  0x65, 0x20, 0x69, 0x74, 0x20, 0x64, 0x6F, 0x20, 0x6D, 0x6F,
	  0x72, 0x65, 0x20, 0x74, 0x68, 0x61, 0x6E, 0x20, 0x6F, 0x6E,
	  0x65, 0x20, 0x69, 0x74, 0x65, 0x72, 0x61, 0x74, 0x69, 0x6F,
	  0x6E };

    int		i;
    SHA1_CTX	sha1Context;
    u_char	unicodePassword[MAX_NT_PASSWORD * 2];
    u_char	PasswordHash[MD4_SIGNATURE_SIZE];
    u_char	PasswordHashHash[MD4_SIGNATURE_SIZE];
    u_char	Digest[SHA1_SIGNATURE_SIZE];
    u_char	Challenge[8];

    /* Hash (x2) the Unicode version of the secret (== password). */
    ascii2unicode(secret, secret_len, unicodePassword);
    NTPasswordHash(unicodePassword, secret_len * 2, PasswordHash);
    NTPasswordHash(PasswordHash, sizeof(PasswordHash), PasswordHashHash);

    SHA1_Init(&sha1Context);
    SHA1_Update(&sha1Context, PasswordHashHash, sizeof(PasswordHashHash));
    SHA1_Update(&sha1Context, NTResponse, 24);
    SHA1_Update(&sha1Context, Magic1, sizeof(Magic1));
    SHA1_Final(Digest, &sha1Context);

    ChallengeHash(PeerChallenge, rchallenge, username, Challenge);

    SHA1_Init(&sha1Context);
    SHA1_Update(&sha1Context, Digest, sizeof(Digest));
    SHA1_Update(&sha1Context, Challenge, sizeof(Challenge));
    SHA1_Update(&sha1Context, Magic2, sizeof(Magic2));
    SHA1_Final(Digest, &sha1Context);

    /* Convert to ASCII hex string. */
    for (i = 0; i < MAX((MS_AUTH_RESPONSE_LENGTH / 2), sizeof(Digest)); i++)
	sprintf(&authResponse[i * 2], "%02X", Digest[i]);
}


#ifdef MPPE
/*
 * Set mppe_xxxx_key from the NTPasswordHashHash.
 * RFC 2548 (RADIUS support) requires us to export this function (ugh).
 */
void
mppe_set_keys(u_char *rchallenge, u_char PasswordHashHash[MD4_SIGNATURE_SIZE])
{
    SHA1_CTX	sha1Context;
    u_char	Digest[SHA1_SIGNATURE_SIZE];	/* >= MPPE_MAX_KEY_LEN */

    SHA1_Init(&sha1Context);
    SHA1_Update(&sha1Context, PasswordHashHash, MD4_SIGNATURE_SIZE);
    SHA1_Update(&sha1Context, PasswordHashHash, MD4_SIGNATURE_SIZE);
    SHA1_Update(&sha1Context, rchallenge, 8);
    SHA1_Final(Digest, &sha1Context);

    /* Same key in both directions. */
    BCOPY(Digest, mppe_send_key, sizeof(mppe_send_key));
    BCOPY(Digest, mppe_recv_key, sizeof(mppe_recv_key));
}

/*
 * Set mppe_xxxx_key from MS-CHAP credentials. (see RFC 3079)
 */
static void
Set_Start_Key(u_char *rchallenge, char *secret, int secret_len)
{
    u_char	unicodePassword[MAX_NT_PASSWORD * 2];
    u_char	PasswordHash[MD4_SIGNATURE_SIZE];
    u_char	PasswordHashHash[MD4_SIGNATURE_SIZE];

    /* Hash (x2) the Unicode version of the secret (== password). */
    ascii2unicode(secret, secret_len, unicodePassword);
    NTPasswordHash(unicodePassword, secret_len * 2, PasswordHash);
    NTPasswordHash(PasswordHash, sizeof(PasswordHash), PasswordHashHash);

    mppe_set_keys(rchallenge, PasswordHashHash);
}

/*
 * Set mppe_xxxx_key from MS-CHAPv2 credentials. (see RFC 3079)
 */
static void
SetMasterKeys(char *secret, int secret_len, u_char NTResponse[24], int IsServer)
{
    SHA1_CTX	sha1Context;
    u_char	unicodePassword[MAX_NT_PASSWORD * 2];
    u_char	PasswordHash[MD4_SIGNATURE_SIZE];
    u_char	PasswordHashHash[MD4_SIGNATURE_SIZE];
    u_char	MasterKey[SHA1_SIGNATURE_SIZE];	/* >= MPPE_MAX_KEY_LEN */
    u_char	Digest[SHA1_SIGNATURE_SIZE];	/* >= MPPE_MAX_KEY_LEN */

    u_char SHApad1[40] =
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    u_char SHApad2[40] =
	{ 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
	  0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
	  0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
	  0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2 };

    /* "This is the MPPE Master Key" */
    u_char Magic1[27] =
	{ 0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74,
	  0x68, 0x65, 0x20, 0x4d, 0x50, 0x50, 0x45, 0x20, 0x4d,
	  0x61, 0x73, 0x74, 0x65, 0x72, 0x20, 0x4b, 0x65, 0x79 };
    /* "On the client side, this is the send key; "
       "on the server side, it is the receive key." */
    u_char Magic2[84] =
	{ 0x4f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x63, 0x6c, 0x69,
	  0x65, 0x6e, 0x74, 0x20, 0x73, 0x69, 0x64, 0x65, 0x2c, 0x20,
	  0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
	  0x65, 0x20, 0x73, 0x65, 0x6e, 0x64, 0x20, 0x6b, 0x65, 0x79,
	  0x3b, 0x20, 0x6f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x73,
	  0x65, 0x72, 0x76, 0x65, 0x72, 0x20, 0x73, 0x69, 0x64, 0x65,
	  0x2c, 0x20, 0x69, 0x74, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
	  0x65, 0x20, 0x72, 0x65, 0x63, 0x65, 0x69, 0x76, 0x65, 0x20,
	  0x6b, 0x65, 0x79, 0x2e };
    /* "On the client side, this is the receive key; "
       "on the server side, it is the send key." */
    u_char Magic3[84] =
	{ 0x4f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x63, 0x6c, 0x69,
	  0x65, 0x6e, 0x74, 0x20, 0x73, 0x69, 0x64, 0x65, 0x2c, 0x20,
	  0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
	  0x65, 0x20, 0x72, 0x65, 0x63, 0x65, 0x69, 0x76, 0x65, 0x20,
	  0x6b, 0x65, 0x79, 0x3b, 0x20, 0x6f, 0x6e, 0x20, 0x74, 0x68,
	  0x65, 0x20, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72, 0x20, 0x73,
	  0x69, 0x64, 0x65, 0x2c, 0x20, 0x69, 0x74, 0x20, 0x69, 0x73,
	  0x20, 0x74, 0x68, 0x65, 0x20, 0x73, 0x65, 0x6e, 0x64, 0x20,
	  0x6b, 0x65, 0x79, 0x2e };
    u_char *s;

    /* Hash (x2) the Unicode version of the secret (== password). */
    ascii2unicode(secret, secret_len, unicodePassword);
    NTPasswordHash(unicodePassword, secret_len * 2, PasswordHash);
    NTPasswordHash(PasswordHash, sizeof(PasswordHash), PasswordHashHash);

    SHA1_Init(&sha1Context);
    SHA1_Update(&sha1Context, PasswordHashHash, sizeof(PasswordHashHash));
    SHA1_Update(&sha1Context, NTResponse, 24);
    SHA1_Update(&sha1Context, Magic1, sizeof(Magic1));
    SHA1_Final(MasterKey, &sha1Context);

    /*
     * generate send key
     */
    if (IsServer)
	s = Magic3;
    else
	s = Magic2;
    SHA1_Init(&sha1Context);
    SHA1_Update(&sha1Context, MasterKey, 16);
    SHA1_Update(&sha1Context, SHApad1, sizeof(SHApad1));
    SHA1_Update(&sha1Context, s, 84);
    SHA1_Update(&sha1Context, SHApad2, sizeof(SHApad2));
    SHA1_Final(Digest, &sha1Context);

    BCOPY(Digest, mppe_send_key, sizeof(mppe_send_key));

    /*
     * generate recv key
     */
    if (IsServer)
	s = Magic2;
    else
	s = Magic3;
    SHA1_Init(&sha1Context);
    SHA1_Update(&sha1Context, MasterKey, 16);
    SHA1_Update(&sha1Context, SHApad1, sizeof(SHApad1));
    SHA1_Update(&sha1Context, s, 84);
    SHA1_Update(&sha1Context, SHApad2, sizeof(SHApad2));
    SHA1_Final(Digest, &sha1Context);

    BCOPY(Digest, mppe_recv_key, sizeof(mppe_recv_key));
}

#endif /* MPPE */


void
ChapMS(u_char *rchallenge, char *secret, int secret_len,
       MS_ChapResponse *response)
{
#if 0
    CHAPDEBUG((LOG_INFO, "ChapMS: secret is '%.*s'", secret_len, secret));
#endif
    BZERO(response, sizeof(*response));

    ChapMS_NT(rchallenge, secret, secret_len, response->NTResp);

#ifdef MSLANMAN
    ChapMS_LANMan(rchallenge, secret, secret_len, response);

    /* preferred method is set by option  */
    response->UseNT[0] = !ms_lanman;
#else
    response->UseNT[0] = 1;
#endif

#ifdef MPPE
    Set_Start_Key(rchallenge, secret, secret_len);
    mppe_keys_set = 1;
#endif
}


/*
 * If PeerChallenge is NULL, one is generated and response->PeerChallenge
 * is filled in.  Call this way when generating a response.
 * If PeerChallenge is supplied, it is copied into response->PeerChallenge.
 * Call this way when verifying a response (or debugging).
 * Do not call with PeerChallenge = response->PeerChallenge.
 *
 * response->PeerChallenge is then used for calculation of the
 * Authenticator Response.
 */
void
ChapMS2(u_char *rchallenge, u_char *PeerChallenge,
	char *user, char *secret, int secret_len, MS_Chap2Response *response,
	u_char authResponse[], int authenticator)
{
    /* ARGSUSED */
    u_char *p = response->PeerChallenge;
    int i;

    BZERO(response, sizeof(*response));

    /* Generate the Peer-Challenge if requested, or copy it if supplied. */
    if (!PeerChallenge)
	for (i = 0; i < sizeof(response->PeerChallenge); i++) {
#ifdef __APPLE__
		last_challenge_response[i] = *p++ = (u_char) (drand48() * 0xff);
#else
	    *p++ = (u_char) (drand48() * 0xff);
#endif
	}
    else
	BCOPY(PeerChallenge, response->PeerChallenge,
	      sizeof(response->PeerChallenge));

    /* Generate the NT-Response */
    ChapMS2_NT(rchallenge, response->PeerChallenge, user,
	       secret, secret_len, response->NTResp);

    /* Generate the Authenticator Response. */
    GenerateAuthenticatorResponse(secret, secret_len, response->NTResp,
				  response->PeerChallenge, rchallenge,
				  user, authResponse);

#ifdef MPPE
    SetMasterKeys(secret, secret_len, response->NTResp, authenticator);
    mppe_keys_set = 1;
#endif
}

#ifdef MPPE
/*
 * Set MPPE options from plugins.
 */
void
set_mppe_enc_types(int policy, int types)
{
    /* Early exit for unknown policies. */
    if (policy != MPPE_ENC_POL_ENC_ALLOWED ||
	policy != MPPE_ENC_POL_ENC_REQUIRED)
	return;

    /* Don't modify MPPE if it's optional and wasn't already configured. */
    if (policy == MPPE_ENC_POL_ENC_ALLOWED && !ccp_wantoptions[0].mppe)
	return;

    /*
     * Disable undesirable encryption types.  Note that we don't ENABLE
     * any encryption types, to avoid overriding manual configuration.
     */
    switch(types) {
	case MPPE_ENC_TYPES_RC4_40:
	    ccp_wantoptions[0].mppe &= ~MPPE_OPT_128;	/* disable 128-bit */
	    break;
	case MPPE_ENC_TYPES_RC4_128:
	    ccp_wantoptions[0].mppe &= ~MPPE_OPT_40;	/* disable 40-bit */
	    break;
	default:
	    break;
    }
}
#endif /* MPPE */

static struct chap_digest_type chapms_digest = {
	CHAP_MICROSOFT,		/* code */
	chapms_generate_challenge,
	chapms_verify_response,
	chapms_make_response,
	NULL,			/* check_success */
	chapms_handle_failure,
#ifdef __APPLE__
	0,
	0
#endif
};

static struct chap_digest_type chapms2_digest = {
	CHAP_MICROSOFT_V2,	/* code */
	chapms2_generate_challenge,
	chapms2_verify_response,
	chapms2_make_response,
	chapms2_check_success,
	chapms_handle_failure,
#ifdef __APPLE__
	chapms2_change_password,
	chapms2_retry_password
#endif
};

void
chapms_init(void)
{
	chap_register_digest(&chapms_digest);
	chap_register_digest(&chapms2_digest);
	add_options(chapms_option_list);
}

#ifdef __APPLE__
void
chapms_reinit(void)
{
	last_challenge_id = 0;
	last_challenge_response[0] = 0;
}
#endif

#endif /* CHAPMS */
