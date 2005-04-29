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


//
// AppleCSPUtils.h - CSP-wide utility functions
//

#ifndef	_H_APPLE_CSP_UTILS
#define _H_APPLE_CSP_UTILS

#include "cspdebugging.h"
#include <Security/cssmtype.h>
#include <security_utilities/alloc.h>
#include <security_cdsa_utilities/context.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* Key type */
typedef enum {
	CKT_Session,
	CKT_Private,
	CKT_Public
} cspKeyType;

/* Key storage type returned from cspParseKeyAttr() */
typedef enum {
	 CKS_Ref,
	 CKS_Data,
	 CKS_None
} cspKeyStorage;

#define KEY_ATTR_RETURN_MASK	(CSSM_KEYATTR_RETURN_DATA |		\
								 CSSM_KEYATTR_RETURN_REF  |		\
								 CSSM_KEYATTR_RETURN_NONE)

/*
 * Validate key attribute bits per specified key type.
 *
 * Used to check requested key attributes for new keys and for validating
 * incoming existing keys. For checking key attributes for new keys,
 * assumes that KEYATTR_RETURN_xxx bits have been checked elsewhere
 * and stripped off before coming here.
 */
void cspValidateKeyAttr(
	cspKeyType 	keyType,
	uint32 		keyAttr);

/*
 * Perform sanity check of incoming key attribute bits for a given
 * key type, and return a malKeyStorage value.
 *
 * Called from any routine which generates a new key. This specifically
 * excludes WrapKey().
 */
cspKeyStorage cspParseKeyAttr(
	cspKeyType 	keyType,
	uint32 		keyAttr);
	
/*
 * Validate key usage bits for specified key type.
 */
void cspValidateKeyUsageBits (
	cspKeyType	keyType,
	uint32		keyUsage);

/*
 * Validate existing key's usage bits against intended use.
 */
void cspValidateIntendedKeyUsage(
	const CSSM_KEYHEADER	*hdr,
	CSSM_KEYUSE				intendedUsage);

/*
 * Set up a key header.
 */
void setKeyHeader(
	CSSM_KEYHEADER &hdr,
	const Guid &myGuid,
	CSSM_ALGORITHMS alg, 
	CSSM_KEYCLASS keyClass,
	CSSM_KEYATTR_FLAGS attrs, 
	CSSM_KEYUSE use);

/*
 * Ensure that indicated CssmData can handle 'length' bytes 
 * of data. Malloc the Data ptr if necessary.
 */
void setUpCssmData(
	CssmData			&data,
	size_t				length,
	Allocator		&allocator);

void setUpData(
	CSSM_DATA			&data,
	size_t				length,
	Allocator		&allocator);
	
void freeCssmData(
	CssmData			&data, 
	Allocator		&allocator);
	
void freeData(
	CSSM_DATA			*data, 
	Allocator		&allocator,
	bool				freeStruct);		// free the CSSM_DATA itself

/*
 * Copy source to destination, mallocing destination if necessary.
 */
void copyCssmData(
	const CssmData		&src,
	CssmData			&dst,
	Allocator		&allocator);

void copyData(
	const CSSM_DATA		&src,
	CSSM_DATA			&dst,
	Allocator		&allocator);

/*
 * Compare two CSSM_DATAs, return CSSM_TRUE if identical.
 */
CSSM_BOOL cspCompareCssmData(
	const CSSM_DATA 	*data1,
	const CSSM_DATA 	*data2);

/*
 * This takes care of mallocing the and KeyLabel field. 
 */
void copyCssmHeader(
	const CssmKey::Header	&src,
	CssmKey::Header			&dst,
	Allocator			&allocator);
	
/*
 * Given a wrapped key, infer its raw format. 
 * This is a real kludge; it only works as long as each {algorithm, keyClass}
 * maps to exactly one format.  
 */
CSSM_KEYBLOB_FORMAT inferFormat(
	const CssmKey	&wrappedKey);

/*
 * Given a key and a Context, obtain the optional associated 
 * CSSM_ATTRIBUTE_{PUBLIC,PRIVATE,SYMMETRIC}_KEY_FORMAT attribute as a 
 * CSSM_KEYBLOB_FORMAT.
 */
CSSM_KEYBLOB_FORMAT requestedKeyFormat(
	const Context 	&context,
	const CssmKey	&key);
	
/* stateless function to calculate SHA-1 hash of a blob */

#define SHA1_DIGEST_SIZE	20
void cspGenSha1Hash(
	const void 		*inData,
	size_t			inDataLen,
	void			*out);			// caller mallocs, digest goes here

void cspVerifyKeyTimes(
	const CSSM_KEYHEADER &hdr);

#ifdef	__cplusplus
}
#endif

#endif	//  _H_APPLE_CSP_UTILS
