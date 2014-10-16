/*
 * Copyright (c) 2001-2009 Apple Inc. All rights reserved.
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
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonCryptor.h>
#include <libkern/OSByteOrder.h>
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
    CC_MD4(password, password_len, hash);
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
    CC_SHA1_CTX		context;
    uint8_t		hash[CC_SHA1_DIGEST_LENGTH];

    /* find the last backslash to get the user name to use for the hash */
    user = (const uint8_t *)strrchr((const char *)username, '\\');
    if (user == NULL) {
	user = username;
    }
    else {
	user = user + 1;
    }
    CC_SHA1_Init(&context);
    CC_SHA1_Update(&context, peer_challenge, MSCHAP2_CHALLENGE_SIZE);
    CC_SHA1_Update(&context, auth_challenge, MSCHAP2_CHALLENGE_SIZE);
    CC_SHA1_Update(&context, user, (int)strlen((const char *)user));
    CC_SHA1_Final(hash, &context);
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
    CC_SHA1_CTX		context;
    int			i;
    uint8_t		hash[CC_SHA1_DIGEST_LENGTH];
    uint8_t		password_hash[NT_PASSWORD_HASH_SIZE];
    uint8_t *		scan;

    NTPasswordHashHash(password, password_len, password_hash);

    CC_SHA1_Init(&context);
    CC_SHA1_Update(&context, password_hash, NT_PASSWORD_HASH_SIZE);
    CC_SHA1_Update(&context, nt_response, MSCHAP_NT_RESPONSE_SIZE);
    CC_SHA1_Update(&context, magic1, 39);
    CC_SHA1_Final(hash, &context);

    ChallengeHash(peer_challenge, auth_challenge, username,
		  challenge);
    
    CC_SHA1_Init(&context);
    CC_SHA1_Update(&context, hash, CC_SHA1_DIGEST_LENGTH);
    CC_SHA1_Update(&context, challenge, MSCHAP_NT_CHALLENGE_SIZE);
    CC_SHA1_Update(&context, magic2, 41);
    CC_SHA1_Final(hash, &context);

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
	 i < CC_SHA1_DIGEST_LENGTH; i++, scan +=2) {
	char	hexstr[3];
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
    NTPasswordHashHash(unicode_password, password_len * 2, input);
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

    CC_MD5(input, HASH_INPUT_SIZE, key);
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
    size_t 		bytes_processed;
    CCCryptorStatus	status;

    /* sizeof(cypher) == clear_length */
    status = CCCrypt(kCCEncrypt, kCCAlgorithmRC4, 0,
		     key, key_length, NULL,
		     clear, clear_length,
		     cypher, clear_length,
		     &bytes_processed);
    if (status != kCCSuccess) {
	fprintf(stderr, "rc4_encrypt: CCCrypt failed with %d\n",
		status);
	return;
    }
    return;
}

static void
EncryptPwBlockWithPasswordHash(const uint8_t * password,
			       uint32_t password_len,
			       const uint8_t pw_hash[NT_PASSWORD_HASH_SIZE],
			       NTPasswordBlockRef pwblock)
{
    NTPasswordBlock	clear_pwblock;
    int			offset;
    uint32_t		password_len_little_endian;

    MSChapFillWithRandom(&clear_pwblock, sizeof(clear_pwblock));
    offset = sizeof(clear_pwblock.password) - password_len;
    bcopy(password, ((void *)&clear_pwblock) + offset, password_len);
    /* convert password length to little endian */
    password_len_little_endian = OSSwapHostToLittleInt32(password_len);
    bcopy(&password_len_little_endian, clear_pwblock.password_length,
	  sizeof(uint32_t));
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
				uint8_t encrypted_hash[NT_PASSWORD_HASH_SIZE])
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
				     encrypted_hash);
    return;
}

void
MSChapFillWithRandom(void * buf, uint32_t len)
{
    int			i;
    int			n;
    u_int32_t * 	p = (u_int32_t *)buf;
    
    n = len / sizeof(*p);
    for (i = 0; i < n; i++, p++) {
	*p = arc4random();
    }
    return;
}

/**
 ** RFC 3079
 ** MPPE functions
 **/

/*
 * Pads used in key derivation
 */

const static uint8_t 	SHSpad1[40] =
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

const static uint8_t 	SHSpad2[40] =
    { 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
      0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
      0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
      0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2 };

/*
 * "Magic" constants used in key derivations
 */

const static uint8_t 	Magic1[27] =
    { 0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74,
      0x68, 0x65, 0x20, 0x4d, 0x50, 0x50, 0x45, 0x20, 0x4d,
      0x61, 0x73, 0x74, 0x65, 0x72, 0x20, 0x4b, 0x65, 0x79 };

#define MAGIC2_3_SIZE	84
#define MAGIC2_SIZE	MAGIC2_3_SIZE
#define MAGIC3_SIZE	MAGIC2_3_SIZE

const static uint8_t	Magic2[MAGIC2_SIZE] =
    { 0x4f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x63, 0x6c, 0x69,
      0x65, 0x6e, 0x74, 0x20, 0x73, 0x69, 0x64, 0x65, 0x2c, 0x20,
      0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
      0x65, 0x20, 0x73, 0x65, 0x6e, 0x64, 0x20, 0x6b, 0x65, 0x79,
      0x3b, 0x20, 0x6f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x73,
      0x65, 0x72, 0x76, 0x65, 0x72, 0x20, 0x73, 0x69, 0x64, 0x65,
      0x2c, 0x20, 0x69, 0x74, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
      0x65, 0x20, 0x72, 0x65, 0x63, 0x65, 0x69, 0x76, 0x65, 0x20,
      0x6b, 0x65, 0x79, 0x2e };

const static uint8_t	Magic3[MAGIC3_SIZE] =
    { 0x4f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x63, 0x6c, 0x69,
      0x65, 0x6e, 0x74, 0x20, 0x73, 0x69, 0x64, 0x65, 0x2c, 0x20,
      0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
      0x65, 0x20, 0x72, 0x65, 0x63, 0x65, 0x69, 0x76, 0x65, 0x20,
      0x6b, 0x65, 0x79, 0x3b, 0x20, 0x6f, 0x6e, 0x20, 0x74, 0x68,
      0x65, 0x20, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72, 0x20, 0x73,
      0x69, 0x64, 0x65, 0x2c, 0x20, 0x69, 0x74, 0x20, 0x69, 0x73,
      0x20, 0x74, 0x68, 0x65, 0x20, 0x73, 0x65, 0x6e, 0x64, 0x20,
      0x6b, 0x65, 0x79, 0x2e };

static void
GetMasterKey(const uint8_t PasswordHashHash[NT_PASSWORD_HASH_SIZE],
	     const uint8_t NTResponse[MSCHAP_NT_RESPONSE_SIZE],
	     uint8_t MasterKey[NT_MASTER_KEY_SIZE])
{
    CC_SHA1_CTX		context;
    uint8_t		Digest[CC_SHA1_DIGEST_LENGTH];

    memset(Digest, 0, sizeof(Digest));

    CC_SHA1_Init(&context);
    CC_SHA1_Update(&context, PasswordHashHash, NT_PASSWORD_HASH_SIZE);
    CC_SHA1_Update(&context, NTResponse, MSCHAP_NT_RESPONSE_SIZE);
    CC_SHA1_Update(&context, Magic1, sizeof(Magic1));
    CC_SHA1_Final(Digest, &context);
    memcpy(MasterKey, Digest, NT_MASTER_KEY_SIZE);
    return;
}

void
MSChap2_MPPEGetMasterKey(const uint8_t * password, uint32_t password_len,
			 const uint8_t NTResponse[MSCHAP_NT_RESPONSE_SIZE],
			 uint8_t MasterKey[NT_MASTER_KEY_SIZE])
{
    uint8_t 	password_hash[NT_PASSWORD_HASH_SIZE];
    uint8_t	unicode_password[NT_MAXPWLEN * 2];

    password_to_unicode(password, password_len, unicode_password);

    NTPasswordHashHash(unicode_password, password_len * 2, password_hash);
    GetMasterKey(password_hash, NTResponse, MasterKey);
}

void
MSChap2_MPPEGetAsymetricStartKey(const uint8_t MasterKey[NT_MASTER_KEY_SIZE],
				 uint8_t SessionKey[NT_SESSION_KEY_SIZE],
				 int SessionKeyLength,
				 bool IsSend,
				 bool IsServer)
{
    CC_SHA1_CTX		context;
    uint8_t		Digest[CC_SHA1_DIGEST_LENGTH];
    const uint8_t *	s;

    /*
     * The logic in the spec says:
     *  if (IsSend) {
     *	    if (IsServer) {
     *         	s = Magic3;
     *	    }
     *	    else {
     *	    	s = Magic2;
     *	    }
     * 	} 
     *  else {
     *	    if (IsServer) {
     *      	s = Magic2;
     *	    } 
     *	    else {
     *      	s = Magic3;
     *	    }
     *	}
     *
     * The corresponding truth table is:
     * IsSend	IsServer	s
     *   0	  0		Magic3
     *   0	  1		Magic2
     *   1	  0		Magic2
     *   1	  1		Magic3
     * which is simply:
     *	s = (IsSend == IsServer) ? Magic3 : Magic2;
     */
    s = (IsSend == IsServer) ? Magic3 : Magic2;
    memset(Digest, 0, sizeof(Digest));
    CC_SHA1_Init(&context);
    CC_SHA1_Update(&context, MasterKey, NT_MASTER_KEY_SIZE);
    CC_SHA1_Update(&context, SHSpad1, sizeof(SHSpad1));
    CC_SHA1_Update(&context, s, MAGIC2_3_SIZE);
    CC_SHA1_Update(&context, SHSpad2, sizeof(SHSpad2));
    CC_SHA1_Final(Digest, &context);

    if (SessionKeyLength > NT_SESSION_KEY_SIZE) {
	SessionKeyLength = NT_SESSION_KEY_SIZE;
    }
    memcpy(SessionKey, Digest, SessionKeyLength);
    return;
}

#ifdef TEST_MSCHAP

const char * password = "clientPass";
const uint8_t nt_response[MSCHAP_NT_RESPONSE_SIZE] = {
    0x82, 0x30, 0x9E, 0xCD, 0x8D, 0x70, 0x8B, 0x5E,
    0xA0, 0x8F, 0xAA, 0x39, 0x81, 0xCD, 0x83, 0x54,
    0x42, 0x33, 0x11, 0x4A, 0x3D, 0x85, 0xD6, 0xDF
};

const uint8_t PasswordHashHash[NT_PASSWORD_HASH_SIZE] = {
    0x41, 0xC0, 0x0C, 0x58, 0x4B, 0xD2, 0xD9, 0x1C,
    0x40, 0x17, 0xA2, 0xA1, 0x2F, 0xA5, 0x9F, 0x3F
};

const uint8_t MasterKey[NT_MASTER_KEY_SIZE] = {
    0xFD, 0xEC, 0xE3, 0x71, 0x7A, 0x8C, 0x83, 0x8C,
    0xB3, 0x88, 0xE5, 0x27, 0xAE, 0x3C, 0xDD, 0x31
};

const uint8_t SendStartKey128[NT_SESSION_KEY_SIZE] ={
    0x8B, 0x7C, 0xDC, 0x14, 0x9B, 0x99, 0x3A, 0x1B,
    0xA1, 0x18, 0xCB, 0x15, 0x3F, 0x56, 0xDC, 0xCB
};


int
main()
{
    uint8_t master[NT_MASTER_KEY_SIZE];
    uint8_t send_start[NT_SESSION_KEY_SIZE];

    MSChap2_MPPEGetMasterKey((const uint8_t *)password, strlen(password),
			     nt_response, master);
    if (bcmp(master, MasterKey, NT_MASTER_KEY_SIZE) != 0) {
	printf("Master Key generation failed\n");
	exit(1);
    }
    printf("Master Key generation successful\n");

    MSChap2_MPPEGetAsymetricStartKey(master,
				     send_start, sizeof(send_start),
				     1, 1);
    if (bcmp(send_start, SendStartKey128, sizeof(send_start)) != 0) {
	printf("SendStartKey128 generation failed\n");
	exit(1);
    }
    printf("SendStartKey128 generation successful\n");
    exit(0);
    return (0);
}
#endif /* TEST_MSCHAP */
