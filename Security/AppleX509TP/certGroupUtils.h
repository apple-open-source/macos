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
#include <Security/cssmalloc.h>
#include "TPCertInfo.h"
/*
 * Cheetah version of TP doesn't work with DLs. 
 */
#define TP_DL_ENABLE		1

#ifdef	__cplusplus
extern "C" {
#endif

/* quick & dirty port from OS9 to OS X... */
#define tpFree(alloc, ptr)			(alloc).free(ptr)
#define tpMalloc(alloc, size)		(alloc).malloc(size)
#define tpCalloc(alloc, num, size)	(alloc).calloc(num, size)

void tpCopyCssmData(
	CssmAllocator	&alloc,
	const CSSM_DATA	*src,
	CSSM_DATA_PTR	dst);
CSSM_DATA_PTR tpMallocCopyCssmData(
	CssmAllocator	&alloc,
	const CSSM_DATA	*src);
void tpFreeCssmData(
	CssmAllocator &alloc,
	CSSM_DATA_PTR 	data,
	CSSM_BOOL 		freeStruct);
CSSM_BOOL tpCompareCssmData(
	const CSSM_DATA *data1,
	const CSSM_DATA *data2);
CSSM_BOOL tpCompareOids(
	const CSSM_OID *oid1,
	const CSSM_OID *oid2);

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

#if	 TP_DL_ENABLE
TPCertInfo *tpFindIssuer(
	CssmAllocator 			&alloc,
	CSSM_CL_HANDLE			clHand,
	CSSM_CSP_HANDLE			cspHand,
	TPCertInfo				*subjectCert,
	const CSSM_DATA			*issuerName,		// passed for convenience
	const CSSM_DL_DB_LIST	*dbList,
	const char 				*cssmTimeStr,		// may be NULL
	CSSM_RETURN				*issuerExpired);	// RETURNED

#endif	/* TP_DL_ENABLE*/

CSSM_BOOL tpIsSameName( 
	const CSSM_DATA *pName1,
	const CSSM_DATA *pName2);

CSSM_RETURN tp_VerifyCert(
	CSSM_CL_HANDLE			clHand,
	CSSM_CSP_HANDLE			cspHand,
	TPCertInfo				*subjectCert,
	TPCertInfo				*issuerCert,
	CSSM_BOOL				checkIssuerCurrent,
	CSSM_BOOL				allowExpired);

CSSM_BOOL tp_CompareCerts(
	const CSSM_DATA			*cert1,
	const CSSM_DATA			*cert2);

/*
 * Given an OID, return the corresponding CSSM_ALGID.
 */
CSSM_ALGORITHMS tpOidToAldId(
	const CSSM_OID *oid,
	CSSM_ALGORITHMS *keyAlg);			// RETURNED

#ifdef	__cplusplus
}
#endif

#endif /* _CERT_GROUP_UTILS_H */
