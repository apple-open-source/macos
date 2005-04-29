/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <openssl/md5.h>
#include <openssl/md4.h>
#include <openssl/sha.h>
#include <openssl/rc4.h>
#include <machine/byte_order.h>
#include <stdbool.h>
#include "mschap.h"

#include "DESSupport.h"

static void
ChallengeResponse(const uint8_t challenge[MSCHAP_NT_CHALLENGE_SIZE], 
		  const uint8_t password_hash[NT_PASSWORD_HASH_SIZE], 
		  uint8_t response[MSCHAP_NT_RESPONSE_SIZE]);

static void
password_to_unicode(const uint8_t * password, uint32_t password_len, 
		    uint8_t * unicode_password)
{
    int i;

    if (password_len > NT_MAXPWLEN) {
	password_len = NT_MAXPWLEN;
    }
    bzero(unicode_password, password_len * 2);
    for (i = 0; i < password_len; i++) {
	unicode_password[i * 2] = password[i];
    }
    return;
}

static void
NTPasswordHash(const uint8_t * password, uint32_t password_len, 
	       uint8_t hash[NT_PASSWORD_HASH_SIZE])
{
    MD4(password, password_len, hash);
    return;
}

static void 
NTPasswordHashHash(const uint8_t * password, uint32_t password_len,
		   uint8_t ret_hash[NT_PASSWORD_HASH_SIZE])
{
    uint8_t	hash[NT_PASSWORD_HASH_SIZE];

    NTPasswordHash(password, password_len, hash);
    NTPasswordHash(hash, NT_PASSWORD_HASH_SIZE, ret_hash);
    return;
}

static void 
NTChallengeResponse(const uint8_t challenge[MSCHAP_NT_CHALLENGE_SIZE], 
		    const uint8_t * password, uint32_t password_len,
		    uint8_t response[MSCHAP_NT_RESPONSE_SIZE])
{
    uint8_t 	hash[NT_PASSWORD_HASH_SIZE];

    NTPasswordHash(password, password_len, hash);
    ChallengeResponse(challenge, hash, response);
    return;
}

static void 
NTMPPEChallengeResponse(const uint8_t challenge[MSCHAP_NT_CHALLENGE_SIZE],
			const uint8_t * password, 
			uint32_t password_len,
			uint8_t response[MSCHAP_NT_CHALLENGE_SIZE])
{
    uint8_t 	hash[NT_PASSWORD_HASH_SIZE];

    NTPasswordHashHash(password, password_len, hash);
    ChallengeResponse(challenge, hash, response);
    return;
}

static void
ChallengeResponse(const uint8_t challenge[MSCHAP_NT_CHALLENGE_SIZE], 
		  const uint8_t password_hash[NT_PASSWORD_HASH_SIZE], 
		  uint8_t response[MSCHAP_NT_RESPONSE_SIZE])
{
    uint8_t	zhash[21];

    /* zero pad the password hash to 21 bytes */
    bzero(zhash, 21);
    bcopy(password_hash, zhash, NT_PASSWORD_HASH_SIZE);

    DesEncrypt(challenge, zhash, response);
    DesEncrypt(challenge, zhash + 7, response + 8);
    DesEncrypt(challenge, zhash + 14, response + 16);
    return;
}

static void
ChallengeHash(const uint8_t peer_challenge[MSCHAP2_CHALLENGE_SIZE],
	      const uint8_t auth_challenge[MSCHAP2_CHALLENGE_SIZE],
	      const uint8_t * username,
	      uint8_t challenge[MSCHAP_NT_CHALLENGE_SIZE])
{
    const uint8_t *	user;
    SHA_CTX		context;
    uint8_t		hash[SHA_DIGEST_LENGTH];

    /* find the last backslash to get the user name to use for the hash */
    user = strrchr(username, '\\');
    if (user == NULL) {
	user = username;
    }
    else {
	user = user + 1;
    }
    SHA1_Init(&context);
    SHA1_Update(&context, peer_challenge, MSCHAP2_CHALLENGE_SIZE);
    SHA1_Update(&context, auth_challenge, MSCHAP2_CHALLENGE_SIZE);
    SHA1_Update(&context, user, strlen(user));
    SHA1_Final(hash, &context);
    bcopy(hash, challenge, MSCHAP_NT_CHALLENGE_SIZE);
    return;
}



static const uint8_t	magic1[39] = {
    0x4D, 0x61, 0x67, 0x69, 0x63, 0x20, 0x73, 0x65, 0x72, 0x76,
    0x65, 0x72, 0x20, 0x74, 0x6F, 0x20, 0x63, 0x6C, 0x69, 0x65,
    0x6E, 0x74, 0x20, 0x73, 0x69, 0x67, 0x6E, 0x69, 0x6E, 0x67,
    0x20, 0x63, 0x6F, 0x6E, 0x73, 0x74, 0x61, 0x6E, 0x74 };

static const uint8_t	magic2[41] = {
    0x50, 0x61, 0x64, 0x20, 0x74, 0x6F, 0x20, 0x6D, 0x61, 0x6B,
    0x65, 0x20, 0x69, 0x74, 0x20, 0x64, 0x6F, 0x20, 0x6D, 0x6F,
    0x72, 0x65, 0x20, 0x74, 0x68, 0x61, 0x6E, 0x20, 0x6F, 0x6E,
    0x65, 0x20, 0x69, 0x74, 0x65, 0x72, 0x61, 0x74, 0x69, 0x6F,
    0x6E};

static void
GenerateAuthResponse(uint8_t * password, uint32_t password_len,
		     const uint8_t nt_response[MSCHAP_NT_RESPONSE_SIZE],
		     const uint8_t peer_challenge[MSCHAP2_CHALLENGE_SIZE],
		     const uint8_t auth_challenge[MSCHAP2_CHALLENGE_SIZE],
		     const uint8_t * username,
		     uint8_t auth_response[MSCHAP2_AUTH_RESPONSE_SIZE])
{
    uint8_t		challenge[MSCHAP_NT_CHALLENGE_SIZE];
    SHA_CTX		context;
    int			i;
    uint8_t		hash[SHA_DIGEST_LENGTH];
    uint8_t		password_hash[NT_PASSWORD_HASH_SIZE];
    uint8_t *		scan;

    NTPasswordHashHash(password, password_len, password_hash);

    SHA1_Init(&context);
    SHA1_Update(&context, password_hash, NT_PASSWORD_HASH_SIZE);
    SHA1_Update(&context, nt_response, MSCHAP_NT_RESPONSE_SIZE);
    SHA1_Update(&context, magic1, 39);
    SHA1_Final(hash, &context);

    ChallengeHash(peer_challenge, auth_challenge, username,
		  challenge);
    
    SHA1_Init(&context);
    SHA1_Update(&context, hash, SHA_DIGEST_LENGTH);
    SHA1_Update(&context, challenge, MSCHAP_NT_CHALLENGE_SIZE);
    SHA1_Update(&context, magic2, 41);
    SHA1_Final(hash, &context);

    /*
     * Encode the value of 'hash' as "S=" followed by
     * 40 ASCII hexadecimal digits and return it in
     * 'auth_response'.
     * For example,
     *   "S=0123456789ABCDEF0123456789ABCDEF01234567"
     */
    auth_response[0] = 'S';
    auth_response[1] = '=';
    for (i = 0, scan = auth_response + 2; 
	 i < SHA_DIGEST_LENGTH; i++, scan +=2) {
	uint8_t	hexstr[3];
	snprintf(hexstr, 3, "%02X", hash[i]);
	scan[0] = hexstr[0];
	scan[1] = hexstr[1];
    }
    return;
}

/*
 * Exported Functions
 */

#define HASH_INPUT_SIZE	(NT_PASSWORD_HASH_SIZE + (MSCHAP_NT_CHALLENGE_SIZE + MSCHAP_NT_RESPONSE_SIZE) * 2)

void 
NTSessionKey16(const uint8_t * password, uint32_t password_len,
	       const uint8_t client_challenge[MSCHAP_NT_CHALLENGE_SIZE],
	       const uint8_t server_response[MSCHAP_NT_RESPONSE_SIZE], 
	       const uint8_t server_challenge[MSCHAP_NT_CHALLENGE_SIZE],
	       uint8_t key[NT_SESSION_KEY_SIZE])
{
    uint8_t 	input[HASH_INPUT_SIZE];
    int		offset;
    uint8_t	unicode_password[NT_MAXPWLEN * 2];

    /* add hash of the hash of the unicode password */
    offset = 0;
    password_to_unicode(password, password_len, unicode_password);
    NTPasswordHashHash(unicode_password, password_len * 2, input + offset);
    offset += NT_PASSWORD_HASH_SIZE;

    /* add the client challenge */
    bcopy(client_challenge, input + offset, MSCHAP_NT_CHALLENGE_SIZE);
    offset += MSCHAP_NT_CHALLENGE_SIZE;

    /* add the server response */
    bcopy(server_response, input + offset, MSCHAP_NT_RESPONSE_SIZE);
    offset += MSCHAP_NT_RESPONSE_SIZE;

    /* add the server challenge */
    bcopy(server_challenge, input + offset, MSCHAP_NT_CHALLENGE_SIZE);
    offset += MSCHAP_NT_CHALLENGE_SIZE;

    /* compute the client response */
    NTChallengeResponse(server_challenge, unicode_password,
			password_len * 2, input + offset);
    MD5(input, HASH_INPUT_SIZE, key);
    return;
}

void
MSChap(const uint8_t challenge[MSCHAP_NT_CHALLENGE_SIZE], 
       const uint8_t * password, uint32_t password_len,
       uint8_t response[MSCHAP_NT_RESPONSE_SIZE])
{
    uint8_t	unicode_password[NT_MAXPWLEN * 2];

    password_to_unicode(password, password_len, unicode_password);
    NTChallengeResponse(challenge, unicode_password, password_len * 2,
			response);
    return;
}    

void
MSChap_MPPE(const uint8_t challenge[MSCHAP_NT_CHALLENGE_SIZE], 
	    const uint8_t * password, uint32_t password_len,
	    uint8_t response[MSCHAP_NT_RESPONSE_SIZE])
{
    uint8_t	unicode_password[NT_MAXPWLEN * 2];

    password_to_unicode(password, password_len, unicode_password);
    NTMPPEChallengeResponse(challenge, unicode_password, 
			    password_len * 2, response);
    return;
}

void
MSChap2(const uint8_t auth_challenge[MSCHAP2_CHALLENGE_SIZE], 
	const uint8_t peer_challenge[MSCHAP2_CHALLENGE_SIZE],
	const uint8_t * username,
	const uint8_t * password, uint32_t password_len,
	uint8_t response[MSCHAP_NT_RESPONSE_SIZE])
{
    uint8_t	unicode_password[NT_MAXPWLEN * 2];
    uint8_t	challenge[MSCHAP_NT_CHALLENGE_SIZE];

    ChallengeHash(peer_challenge, auth_challenge, username,
		  challenge);
    password_to_unicode(password, password_len, unicode_password);
    NTChallengeResponse(challenge, unicode_password, password_len * 2,
			response);
    return;
}    

bool
MSChap2AuthResponseValid(const uint8_t * password, uint32_t password_len,
			 const uint8_t nt_response[MSCHAP_NT_RESPONSE_SIZE],
			 const uint8_t peer_challenge[MSCHAP2_CHALLENGE_SIZE],
			 const uint8_t auth_challenge[MSCHAP2_CHALLENGE_SIZE],
			 const uint8_t * username,
			 const uint8_t response[MSCHAP2_AUTH_RESPONSE_SIZE])
{
    uint8_t	my_response[MSCHAP2_AUTH_RESPONSE_SIZE];
    uint8_t	unicode_password[NT_MAXPWLEN * 2];

    password_to_unicode(password, password_len, unicode_password);

    GenerateAuthResponse(unicode_password, password_len * 2, 
			 nt_response,
			 peer_challenge,
			 auth_challenge, 
			 username,
			 my_response);
    if (bcmp(my_response, response, MSCHAP2_AUTH_RESPONSE_SIZE)) {
	return (false);
    }
    return (true);
}

static void
rc4_encrypt(const void * clear, uint32_t clear_length,
	    const void * key, uint32_t key_length,
	    void * cypher)
{
    RC4_KEY			rc4_key;

    RC4_set_key(&rc4_key, key_length, key);
    /* sizeof(cypher) == clear_length */
    RC4(&rc4_key, clear_length, clear, cypher);
    return;
}

static void
EncryptPwBlockWithPasswordHash(const uint8_t * password,
			       uint32_t password_len,
			       const uint8_t pw_hash[NT_PASSWORD_HASH_SIZE],
			       NTPasswordBlockRef pwblock)
{
    int			offset;
    NTPasswordBlock	clear_pwblock;

    MSChapFillWithRandom(&clear_pwblock, sizeof(clear_pwblock));
    offset = sizeof(clear_pwblock.password) - password_len;
    bcopy(password, ((void *)&clear_pwblock) + offset, password_len);
    clear_pwblock.password_length 
	= NXSwapHostLongToLittle(password_len); /* little endian? */
    rc4_encrypt(&clear_pwblock, sizeof(clear_pwblock),
		pw_hash, NT_PASSWORD_HASH_SIZE, pwblock);
    return;
}

/* 
 * RFC 2759, Section 8.9
 *   NewPasswordEncryptedWithOldNtPasswordHash
 */
void
NTPasswordBlockEncryptNewPasswordWithOldHash(const uint8_t * new_password,
					     uint32_t new_password_len,
					     const uint8_t * old_password,
					     uint32_t old_password_len,
					     NTPasswordBlockRef pwblock)
{
    uint8_t	hash[NT_PASSWORD_HASH_SIZE];
    uint8_t	new_password_unicode[NT_MAXPWLEN * 2];
    uint8_t	old_password_unicode[NT_MAXPWLEN * 2];

    password_to_unicode(new_password, new_password_len, new_password_unicode);
    password_to_unicode(old_password, old_password_len, old_password_unicode);
    
    NTPasswordHash(old_password_unicode, old_password_len * 2, hash);
    EncryptPwBlockWithPasswordHash(new_password_unicode,
				   new_password_len * 2,
				   hash, pwblock);
    return;
}

/*
 * NtPasswordHashEncryptedWithBlock()
 */
static void
NTPasswordHashEncryptedWithBlock(const uint8_t pw_hash[NT_PASSWORD_HASH_SIZE],
				 const uint8_t block[NT_PASSWORD_HASH_SIZE],
				 uint8_t cypher[NT_PASSWORD_HASH_SIZE])
{
    DesEncrypt(pw_hash, block, cypher);
    DesEncrypt(pw_hash + 8, block + 7, cypher + 8);
    return;
}


/* 
 * RFC 2759, Section 8.12
 * OldNtPasswordHashEncryptedWithNewNtPasswordHash()
 */
void
NTPasswordHashEncryptOldWithNew(const uint8_t * new_password,
				uint32_t new_password_len,
				const uint8_t * old_password,
				uint32_t old_password_len,
				uint8_t password_hash[NT_PASSWORD_HASH_SIZE])
{
    uint8_t	new_password_unicode[NT_MAXPWLEN * 2];
    uint8_t	new_pw_hash[NT_PASSWORD_HASH_SIZE];
    uint8_t	old_password_unicode[NT_MAXPWLEN * 2];
    uint8_t	old_pw_hash[NT_PASSWORD_HASH_SIZE];

    password_to_unicode(new_password, new_password_len, new_password_unicode);
    NTPasswordHash(new_password_unicode, new_password_len * 2, new_pw_hash);

    password_to_unicode(old_password, old_password_len, old_password_unicode);
    NTPasswordHash(old_password_unicode, old_password_len * 2, old_pw_hash);

    NTPasswordHashEncryptedWithBlock(old_pw_hash, new_pw_hash,
				     password_hash);
    return;
}

void
MSChapFillWithRandom(void * buf, uint32_t len)
{
    int		i;
    int		n;
    int32_t * 	p = (int32_t *)buf;
    
    n = len / sizeof(long);
    for (i = 0; i < n; i++, p++) {
	*p = (int32_t)random();
    }
    return;
}

