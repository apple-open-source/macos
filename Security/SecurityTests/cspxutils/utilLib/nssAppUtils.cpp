/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */
/*
 * nssAppUtils.cpp
 */
 
#include "nssAppUtils.h"
#include "common.h"
#include "cspwrap.h"
#include <Security/SecAsn1Coder.h>
#include <Security/osKeyTemplates.h>	
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

/*
 * Create pubKeyPartial as copy of pubKey without the DSA params.
 * Returned partial key is RAW. Incoming key can be raw or ref.
 */
CSSM_RETURN extractDsaPartial(
	CSSM_CSP_HANDLE cspHand,
	const CSSM_KEY *pubKey, 
	CSSM_KEY_PTR pubKeyPartial)
{
	const CSSM_KEY *thePubKey = pubKey;
	CSSM_KEY rawPubKey;
	CSSM_RETURN crtn;
	
	if(pubKey->KeyHeader.BlobType == CSSM_KEYBLOB_REFERENCE) {
		/* first get this in raw form */
		crtn = cspRefKeyToRaw(cspHand, pubKey, &rawPubKey);
		if(crtn) {
			return crtn;
		}
		thePubKey = &rawPubKey;
	}
	
	/* decode raw public key */
	NSS_DSAPublicKeyX509 nssPub;
	SecAsn1CoderRef coder;
	
	OSStatus ortn = SecAsn1CoderCreate(&coder);
	if(ortn) {
		cssmPerror("SecAsn1CoderCreate", ortn);
		return ortn;
	}
	memset(&nssPub, 0, sizeof(nssPub));
	if(SecAsn1DecodeData(coder, &thePubKey->KeyData, kSecAsn1DSAPublicKeyX509Template,
			&nssPub)) {
		printf("***Error decoding DSA public key. Aborting.\n");
		return 1;
	}
	
	/* zero out the params and reencode */
	nssPub.dsaAlg.params = NULL;
	CSSM_DATA newKey = {0, NULL};
	if(SecAsn1EncodeItem(coder, &nssPub, kSecAsn1DSAPublicKeyX509Template,
			&newKey)) {
		printf("***Error reencoding DSA pub key\n");
		return 1;
	}
	
	/* copy - newKey is in coder space */
	*pubKeyPartial = *thePubKey;
	appCopyCssmData(&newKey, &pubKeyPartial->KeyData);

	if(pubKey->KeyHeader.BlobType == CSSM_KEYBLOB_REFERENCE) {
		/* free the KeyData mallocd by cspRefKeyToRaw */
		CSSM_FREE(thePubKey->KeyData.Data);
		pubKeyPartial->KeyHeader.BlobType = CSSM_KEYBLOB_RAW;
	}
	pubKeyPartial->KeyHeader.KeyAttr |= CSSM_KEYATTR_PARTIAL;
	SecAsn1CoderRelease(coder);
	return CSSM_OK;
}
