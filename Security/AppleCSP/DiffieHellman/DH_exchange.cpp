/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 * DH_exchange.cp - Diffie-Hellman key exchange
 */
 
#include "DH_exchange.h"
#include <Security/cssmerr.h>
#include <Security/utilities.h>
#include "DH_utils.h"
#include <strings.h>
#include <open_ssl/opensslUtils/opensslUtils.h>

void DeriveKey_DH (
	const Context &context,
	const CssmData &Param,			// other's public key
	CSSM_DATA *keyData,				// mallocd by caller
									// we fill in keyData->Length bytes
	AppleCSPSession &session)
{
	bool mallocdKey;
	size_t privSize;
	
	/* private DH key from context */
	DH *privKey = contextToDhKey(context, session, CSSM_KEYUSE_DERIVE, 
		mallocdKey);
	privSize = DH_size(privKey);
	if(privSize < keyData->Length) {
		/* we've been asked for more bits than this key can generate */
		CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEY_SIZE);
	}
	BIGNUM *pubKey = BN_bin2bn(Param.Data, Param.Length, NULL);
	if(pubKey == NULL) {
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);
	}
	unsigned char *buf = (unsigned char *)session.malloc(privSize);
	int rtn = DH_compute_key(buf, pubKey, privKey);
	if(rtn >= 0) {
		/*
		 * FIXME : I have not found a specification describing *which*
		 * bytes of the value we just computed we are supposed to
		 * use as the actual key bytes. We use the M.S. bytes.
		 */
		memmove(keyData->Data, buf, keyData->Length);
	}
	if(mallocdKey) {
		DH_free(privKey);
	}
	BN_free(pubKey);
	session.free(buf);
	if(rtn < 0) {
		throwRsaDsa("DH_compute_key");
	}
}

