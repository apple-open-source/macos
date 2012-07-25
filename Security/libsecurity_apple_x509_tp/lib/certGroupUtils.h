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
	certGroupUtils.h

	Created 10/9/2000 by Doug Mitchell. 
*/

#ifndef _CERT_GROUP_UTILS_H
#define _CERT_GROUP_UTILS_H

#include <Security/x509defs.h>
#include <security_utilities/alloc.h>
#include "TPCertInfo.h"

#ifdef	__cplusplus
extern "C" {
#endif

/* quick & dirty port from OS9 to OS X... */
#define tpFree(alloc, ptr)			(alloc).free(ptr)
#define tpMalloc(alloc, size)		(alloc).malloc(size)
#define tpCalloc(alloc, num, size)	(alloc).calloc(num, size)

void tpCopyCssmData(
	Allocator		&alloc,
	const CSSM_DATA	*src,
	CSSM_DATA_PTR	dst);
CSSM_DATA_PTR tpMallocCopyCssmData(
	Allocator		&alloc,
	const CSSM_DATA	*src);
void tpFreeCssmData(
	Allocator		&alloc,
	CSSM_DATA_PTR 	data,
	CSSM_BOOL 		freeStruct);
CSSM_BOOL tpCompareCssmData(
	const CSSM_DATA *data1,
	const CSSM_DATA *data2);
	
/*
 * This should break if/when CSSM_OID is not the same as
 * CSSM_DATA, which is exactly what we want.
 */
#define tpCompareOids(oid1, oid2)	tpCompareCssmData(oid1, oid2)

void tpFreePluginMemory(
	CSSM_HANDLE	hand,
	void 		*p);

CSSM_DATA_PTR tp_CertGetPublicKey( 
	TPCertInfo 		*cert,
	CSSM_DATA_PTR 	*valueToFree);			// used in tp_CertFreePublicKey
void tp_CertFreePublicKey(
	CSSM_CL_HANDLE	clHand,
	CSSM_DATA_PTR	value);

CSSM_X509_ALGORITHM_IDENTIFIER_PTR tp_CertGetAlgId( 
    TPCertInfo	 	*cert,
	CSSM_DATA_PTR 	*valueToFree);	// used in tp_CertFreeAlgId
void tp_CertFreeAlgId(
	CSSM_CL_HANDLE	clHand,
	CSSM_DATA_PTR	value);

CSSM_BOOL tp_CompareCerts(
	const CSSM_DATA			*cert1,
	const CSSM_DATA			*cert2);

void tpToLower(
	char *str,
	unsigned strLen);

void tpNormalizeAddrSpec(
	char		*addr,
	unsigned	addrLen,
	bool		normalizeAll);

CSSM_BOOL tpCompareHostNames(
	const char	 	*hostName,			// spec'd by app, tpToLower'd
	uint32			hostNameLen,
	char			*certName,			// from cert, we tpToLower
	uint32			certNameLen);

CSSM_BOOL tpCompareEmailAddr(
	const char	 	*appEmail,		// spec'd by app, tpToLower'd
	uint32			appEmailLen,
	char			*certEmail,		// from cert, we tpToLower
	uint32			certEmailLen,
	bool			normalizeAll);	// true : lower-case all certEmail characters

int decodeECDSA_SigAlgParams(
	const CSSM_DATA *params,
	CSSM_ALGORITHMS *cssmAlg);		/* RETURNED */

#ifdef	__cplusplus
}
#endif

#endif /* _CERT_GROUP_UTILS_H */
