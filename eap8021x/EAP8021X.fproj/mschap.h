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

#ifndef _EAP8021X_MSCHAP_H
#define _EAP8021X_MSCHAP_H

#include <stdint.h>
#include <stdbool.h>

enum {
    kMSCHAP2_ERROR_RESTRICTED_LOGON_HOURS = 646,
    kMSCHAP2_ERROR_ACCT_DISABLED = 647,
    kMSCHAP2_ERROR_PASSWD_EXPIRED= 648,
    kMSCHAP2_ERROR_NO_DIALIN_PERMISSION = 649,
    kMSCHAP2_ERROR_AUTHENTICATION_FAILURE = 691,
    kMSCHAP2_ERROR_CHANGING_PASSWORD = 709
};

#define NT_MAXPWLEN			256 	/* unicode chars */
#define NT_MASTER_KEY_SIZE		16
#define NT_SESSION_KEY_SIZE		16
#define NT_PASSWORD_HASH_SIZE		16

#define MSCHAP_NT_CHALLENGE_SIZE	8
#define MSCHAP_NT_RESPONSE_SIZE		24
#define MSCHAP_LM_RESPONSE_SIZE		24
#define MSCHAP_FLAGS_SIZE		1
#define MSCHAP_IDENT_SIZE		1

#define MSCHAP2_CHALLENGE_SIZE		16
#define MSCHAP2_RESERVED_SIZE		8
#define MSCHAP2_AUTH_RESPONSE_SIZE	42

typedef struct {
    uint8_t		peer_challenge[MSCHAP2_CHALLENGE_SIZE];
    uint8_t		reserved[MSCHAP2_RESERVED_SIZE];
    uint8_t		nt_response[MSCHAP_NT_RESPONSE_SIZE];
    uint8_t		flags[MSCHAP_FLAGS_SIZE];
} MSCHAP2Response, * MSCHAP2ResponseRef;

#define MSCHAP2_RESPONSE_LENGTH		(MSCHAP2_CHALLENGE_SIZE \
			 		 + MSCHAP2_RESERVED_SIZE \
				 	 + MSCHAP_NT_RESPONSE_SIZE \
					 + MSCHAP_FLAGS_SIZE)
typedef struct NTPasswordBlock_s {
    uint8_t		password[NT_MAXPWLEN * 2];
    uint32_t		password_length;
} NTPasswordBlock, * NTPasswordBlockRef;

#define NT_PASSWORD_BLOCK_SIZE		516 /* sizeof(NTPasswordBlock) */

void
MSChap(const uint8_t challenge[MSCHAP_NT_CHALLENGE_SIZE], 
       const uint8_t * password, uint32_t password_len,
       uint8_t response[MSCHAP_NT_RESPONSE_SIZE]);

void
MSChap_MPPE(const uint8_t challenge[MSCHAP_NT_CHALLENGE_SIZE], 
	    const uint8_t * password, uint32_t password_len,
	    uint8_t response[MSCHAP_NT_RESPONSE_SIZE]);

void 
NTSessionKey16(const uint8_t * password, uint32_t password_len,
	       const uint8_t client_challenge[MSCHAP_NT_CHALLENGE_SIZE],
	       const uint8_t server_response[MSCHAP_NT_RESPONSE_SIZE], 
	       const uint8_t server_challenge[MSCHAP_NT_CHALLENGE_SIZE],
	       uint8_t key[NT_SESSION_KEY_SIZE]);

void
MSChap2(const uint8_t authenticator_challenge[MSCHAP2_CHALLENGE_SIZE], 
	const uint8_t peer_challenge[MSCHAP2_CHALLENGE_SIZE],
	const uint8_t * username,
	const uint8_t * password, uint32_t password_len,
	uint8_t response[MSCHAP_NT_RESPONSE_SIZE]);

bool
MSChap2AuthResponseValid(const uint8_t * password, uint32_t password_len,
			 const uint8_t nt_response[MSCHAP_NT_RESPONSE_SIZE],
			 const uint8_t peer_challenge[MSCHAP2_CHALLENGE_SIZE],
			 const uint8_t auth_challenge[MSCHAP2_CHALLENGE_SIZE],
			 const uint8_t * username,
			 const uint8_t response[MSCHAP2_AUTH_RESPONSE_SIZE]);

void
NTPasswordBlockEncryptNewPasswordWithOldHash(const uint8_t * new_password,
					     uint32_t new_password_len,
					     const uint8_t * old_password,
					     uint32_t old_password_len,
					     NTPasswordBlockRef pwblock);
void
NTPasswordHashEncryptOldWithNew(const uint8_t * new_password,
				uint32_t new_password_len,
				const uint8_t * old_password,
				uint32_t old_password_len,
				uint8_t password_hash[NT_PASSWORD_HASH_SIZE]);
void
MSChapFillWithRandom(void * buf, uint32_t len);

void
MSChap2_MPPEGetMasterKey(const uint8_t * password, uint32_t password_len,
			 const uint8_t NTResponse[MSCHAP_NT_RESPONSE_SIZE],
			 uint8_t MasterKey[NT_MASTER_KEY_SIZE]);
void
MSChap2_MPPEGetAsymetricStartKey(const uint8_t MasterKey[NT_MASTER_KEY_SIZE],
				 uint8_t SessionKey[NT_SESSION_KEY_SIZE],
				 int SessionKeyLength,
				 bool IsSend,
				 bool IsServer);
#endif /* _EAP8021X_MSCHAP_H */
