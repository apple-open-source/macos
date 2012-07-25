/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
 * RSA_DSA_utils.h
 */
#ifndef	_RSA_DSA_UTILS_H_
#define _RSA_DSA_UTILS_H_

#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <AppleCSPSession.h>
#include <security_cdsa_utilities/context.h>

#ifdef	__cplusplus
extern "C" {
#endif

uint32 rsaMaxKeySize();
uint32 rsaMaxPubExponentSize();

/* 
 * Given a Context:
 * -- obtain CSSM key (there must only be one)
 * -- validate keyClass
 * -- validate keyUsage
 * -- convert to RSA *, allocating the RSA key if necessary
 */
RSA *contextToRsaKey(
	const Context 		&context,
	AppleCSPSession	 	&session,
	CSSM_KEYCLASS		keyClass,	  // CSSM_KEYCLASS_{PUBLIC,PRIVATE}_KEY
	CSSM_KEYUSE			usage,		  // CSSM_KEYUSE_ENCRYPT, CSSM_KEYUSE_SIGN, etc.
	bool				&mallocdKey,  // RETURNED
	CSSM_DATA			&label);	  // mallocd and RETURNED for OAEP

/* 
 * Convert a CssmKey to an RSA * key. May result in the creation of a new
 * RSA (when cssmKey is a raw key); allocdKey is true in that case
 * in which case the caller generally has to free the allocd key).
 */
RSA *cssmKeyToRsa(
	const CssmKey	&cssmKey,
	AppleCSPSession	&session,
	bool			&allocdKey,		// RETURNED
	CSSM_DATA		&label);		// mallocd and RETURNED for OAEP

/* 
 * Convert a raw CssmKey to a newly alloc'd RSA *.
 */
RSA *rawCssmKeyToRsa(
	const CssmKey	&cssmKey,
	CSSM_DATA		&label);		// mallocd and RETURNED for OAEP keys

/*
 * Given a partially formed DSA public key (with no p, q, or g) and a 
 * CssmKey representing a supposedly fully-formed DSA key, populate
 * the public key's p, g, and q with values from the fully formed key.
 */
CSSM_RETURN dsaGetParamsFromKey(
	DSA 			*partialKey,
	const CssmKey	&paramKey,
	AppleCSPSession	&session);

/* 
 * Given a Context:
 * -- obtain CSSM key (there must only be one)
 * -- validate keyClass
 * -- validate keyUsage
 * -- convert to DSA *, allocating the DSA key if necessary
 */
DSA *contextToDsaKey(
	const Context 		&context,
	AppleCSPSession	 	&session,
	CSSM_KEYCLASS		keyClass,	  // CSSM_KEYCLASS_{PUBLIC,PRIVATE}_KEY
	CSSM_KEYUSE			usage,		  // CSSM_KEYUSE_ENCRYPT, CSSM_KEYUSE_SIGN, etc.
	bool				&mallocdKey); // RETURNED

/* 
 * Convert a CssmKey to an DSA * key. May result in the creation of a new
 * DSA (when cssmKey is a raw key); allocdKey is true in that case
 * in which case the caller generally has to free the allocd key).
 */
DSA *cssmKeyToDsa(
	const CssmKey	&cssmKey,
	AppleCSPSession	&session,
	bool			&allocdKey);	// RETURNED

/* 
 * Convert a raw CssmKey to a newly alloc'd DSA *.
 */
DSA *rawCssmKeyToDsa(
	const CssmKey	&cssmKey,
	AppleCSPSession	&session,
	const CssmKey	*paramKey);		// optional

/*
 * Given a DSA private key, calculate its public component if it 
 * doesn't already exist. Used for calculating the key digest of 
 * an incoming raw private key.
 */
void dsaKeyPrivToPub(
	DSA *dsaKey);

#ifdef	__cplusplus
}
#endif

#endif	/*_RSA_DSA_UTILS_H_ */
