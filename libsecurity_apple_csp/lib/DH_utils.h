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
 * DH_utils.h
 */
#ifndef	_DH_UTILS_H_
#define _DH_UTILS_H_

#include <openssl/dh.h>
#include <AppleCSPSession.h>
#include <security_cdsa_utilities/context.h>

#ifdef	__cplusplus
extern "C" {
#endif

void throwDh(
	const char *op);
	
/* 
 * Given a Context:
 * -- obtain CSSM key (there must only be one)
 * -- validate keyClass - MUST be private! (DH public keys are never found
 *    in contexts.)
 * -- validate keyUsage
 * -- convert to DH *, allocating the DH key if necessary
 */
DH *contextToDhKey(
	const Context 		&context,
	AppleCSPSession	 	&session,
	CSSM_ATTRIBUTE_TYPE	attr,		  // CSSM_ATTRIBUTE_KEY for normal private key
									  // CSSM_ATTRIBUTE_PUBLIC_KEY for public key
	CSSM_KEYCLASS		keyClass,	  // CSSM_KEYCLASS_{PUBLIC,PRIVATE}_KEY	
	CSSM_KEYUSE			usage,		  // CSSM_KEYUSE_ENCRYPT, 
									  //    CSSM_KEYUSE_SIGN, etc.
	bool				&mallocdKey); // RETURNED

/* 
 * Convert a CssmKey to an DH * key. May result in the creation of a new
 * DH (when cssmKey is a raw key); allocdKey is true in that case
 * in which case the caller generally has to free the allocd key).
 */
DH *cssmKeyToDh(
	const CssmKey	&cssmKey,
	AppleCSPSession	&session,
	bool			&allocdKey);	// RETURNED

/* 
 * Convert a raw CssmKey to a newly alloc'd DH *.
 */
DH *rawCssmKeyToDh(
	const CssmKey	&cssmKey);


#ifdef	__cplusplus
}
#endif

#endif	/*_DH_UTILS_H_ */
