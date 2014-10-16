/*
 * Copyright (c) 2000-2004,2011-2012,2014 Apple Inc. All Rights Reserved.
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
 * oidsalg.c - OIDs defining crypto algorithms
 */

#include <Security/oidsbase.h>
#include <Security/cssmtype.h>
#include "cssmapple.h"
#include <Security/oidsalg.h>
#include <string.h>

#pragma mark ----- CSSM_OID <--> CSSM_ALGORITHMS -----

typedef struct {
	const CSSM_OID 	*oid;
	CSSM_ALGORITHMS	alg;
} OidToAlgEnt;

static const OidToAlgEnt oidToAlgMap[] = 
{
	{&CSSMOID_RSA, CSSM_ALGID_RSA },
	{&CSSMOID_MD2WithRSA, CSSM_ALGID_MD2WithRSA },
	{&CSSMOID_MD5WithRSA, CSSM_ALGID_MD5WithRSA },
	{&CSSMOID_SHA1WithRSA, CSSM_ALGID_SHA1WithRSA },
	{&CSSMOID_SHA1WithRSA_OIW, CSSM_ALGID_SHA1WithRSA },
	{&CSSMOID_SHA1, CSSM_ALGID_SHA1},
	{&CSSMOID_MD5, CSSM_ALGID_MD5 },
	/* 
	 * These OIDs have three variants - one for BSAFE, CMS, and JDK 1.1.
	 * On the oid-to-alg map, we'll handle either one, mapping to 
	 * the same CSSM alg. When we map from alg to OID, we'll use
	 * the CMS variant (being first in the list).
	 */
	{&CSSMOID_DSA_CMS, CSSM_ALGID_DSA },
	{&CSSMOID_DSA, CSSM_ALGID_DSA },
	{&CSSMOID_DSA_JDK, CSSM_ALGID_DSA },
	{&CSSMOID_SHA1WithDSA_CMS, CSSM_ALGID_SHA1WithDSA },
	{&CSSMOID_SHA1WithDSA, CSSM_ALGID_SHA1WithDSA },
	{&CSSMOID_SHA1WithDSA_JDK, CSSM_ALGID_SHA1WithDSA },
	/*
	 * Multiple entries for Diffie-Hellman. We favor the PKCS3 version for
	 * mapping alg to OID.
	 */
	{&CSSMOID_DH, CSSM_ALGID_DH},
	{&CSSMOID_ANSI_DH_PUB_NUMBER, CSSM_ALGID_DH},
	{&CSSMOID_ANSI_DH_STATIC, CSSM_ALGID_DH},
	{&CSSMOID_ANSI_DH_ONE_FLOW, CSSM_ALGID_DH},
	{&CSSMOID_ANSI_DH_EPHEM, CSSM_ALGID_DH},
	{&CSSMOID_ANSI_DH_HYBRID1, CSSM_ALGID_DH},
	{&CSSMOID_ANSI_DH_HYBRID2, CSSM_ALGID_DH},
	{&CSSMOID_ANSI_DH_HYBRID_ONEFLOW, CSSM_ALGID_DH},
	{&CSSMOID_APPLE_FEE, CSSM_ALGID_FEE },
	{&CSSMOID_APPLE_ASC, CSSM_ALGID_ASC },
	{&CSSMOID_APPLE_FEE_MD5, CSSM_ALGID_FEE_MD5 },
	{&CSSMOID_APPLE_FEE_SHA1, CSSM_ALGID_FEE_SHA1 },
	{&CSSMOID_APPLE_FEED, CSSM_ALGID_FEED },
	{&CSSMOID_APPLE_FEEDEXP, CSSM_ALGID_FEEDEXP },
	/* the current valid alg --> OID mapping */
	{&CSSMOID_ECDSA_WithSHA1, CSSM_ALGID_SHA1WithECDSA},
	/* for backwards compatibility */
	{&CSSMOID_APPLE_ECDSA, CSSM_ALGID_SHA1WithECDSA },
	{&CSSMOID_SHA224, CSSM_ALGID_SHA224},
	{&CSSMOID_SHA256, CSSM_ALGID_SHA256},
	{&CSSMOID_SHA384, CSSM_ALGID_SHA384},
	{&CSSMOID_SHA512, CSSM_ALGID_SHA512},
	{&CSSMOID_SHA224WithRSA, CSSM_ALGID_SHA224WithRSA },
	{&CSSMOID_SHA256WithRSA, CSSM_ALGID_SHA256WithRSA },
	{&CSSMOID_SHA384WithRSA, CSSM_ALGID_SHA384WithRSA },
	{&CSSMOID_SHA512WithRSA, CSSM_ALGID_SHA512WithRSA },
	{&CSSMOID_RSAWithOAEP, CSSM_ALGMODE_PKCS1_EME_OAEP },
	{&CSSMOID_ECDSA_WithSHA224, CSSM_ALGID_SHA224WithECDSA },
	{&CSSMOID_ECDSA_WithSHA256, CSSM_ALGID_SHA256WithECDSA },
	{&CSSMOID_ECDSA_WithSHA384, CSSM_ALGID_SHA384WithECDSA },
	{&CSSMOID_ECDSA_WithSHA512, CSSM_ALGID_SHA512WithECDSA },
	/* AlgId.algorithm for ECDSA public key */
	{&CSSMOID_ecPublicKey, CSSM_ALGID_ECDSA },
	/* This OID is accompanied by an additional digest OID in AlgId.parameters */
	{&CSSMOID_ECDSA_WithSpecified, CSSM_ALGID_ECDSA_SPECIFIED },
	{NULL, 0}
};

#define NUM_OID_TO_ALGS	(sizeof(oidToAlgMap) / sizeof(oidToAlgMap[0]))

/*
 * Compare two CSSM_DATAs (or two CSSM_OIDs), return true if identical.
 */
static bool compareCssmData(
	const CSSM_DATA *data1,
	const CSSM_DATA *data2)
{	
	if((data1 == NULL) || (data1->Data == NULL) || 
	   (data2 == NULL) || (data2->Data == NULL) ||
	   (data1->Length != data2->Length)) {
		return false;
	}
	if(data1->Length != data2->Length) {
		return false;
	}
	if(memcmp(data1->Data, data2->Data, data1->Length) == 0) {
		return true;
	}
	else {
		return false;
	}
}

bool cssmOidToAlg(
	const CSSM_OID *oid,
	CSSM_ALGORITHMS *alg)		// RETURNED
{
	const OidToAlgEnt *ent;
	
	for(ent=oidToAlgMap; ent->oid; ent++) {
		if(compareCssmData(ent->oid, oid)) {
			*alg = ent->alg;
			return true;
		}
	}
	return false;
}

const CSSM_OID *cssmAlgToOid(
	CSSM_ALGORITHMS algId)
{
	const OidToAlgEnt *ent;
	
	for(ent=oidToAlgMap; ent->oid; ent++) {
		if(ent->alg == algId) {
			return ent->oid;
		}
	}
	return NULL;
}


