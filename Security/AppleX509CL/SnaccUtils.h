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
 * SnaccUtils.h - snacc-related cert functions
 *
 * Created 9/1/2000 by Doug Mitchell. 
 * Copyright (c) 2000 by Apple Computer. 
 */
 
#ifndef	_SNACC_UTILS_H_
#define _SNACC_UTILS_H_

#include <Security/cssmtype.h>
#include <Security/x509defs.h>
#include <Security/certextensions.h>
#include <Security/cssmdata.h>
#include "DecodedCert.h"

/* ghastly requirements of snacc-generated cert code */
#include <Security/asn-incl.h>
#include <Security/sm_vdatypes.h>
#include <Security/sm_x501if.h>
#include <Security/sm_x520sa.h>
#include <Security/sm_x411mtsas.h>
#include <Security/sm_x509cmn.h>
#include <Security/sm_x509af.h>
#include <Security/pkcs9oids.h>
#include <Security/sm_x509ce.h>
#include <Security/sm_cms.h>
#include <Security/sm_ess.h>

#ifdef __cplusplus
extern "C" {
#endif


void 
CL_certDecodeComponents(
	const CssmData 	&signedCert,		// DER-encoded
	CssmOwnedData	&TBSCert,			// still DER-encoded
	CssmOwnedData	&algId,				// ditto
	CssmOwnedData	&sig);				// ditto

void 
CL_certEncodeComponents(
	const CssmData		&TBSCert,		// DER-encoded
	const CssmData		&algId,			// ditto
	const CssmData		&rawSig,		// the raw bits, not encoded
	CssmOwnedData 		&signedCert);	// DER-encoded

void CL_snaccOidToCssm(
	const AsnOid 		&inOid,
	CssmOid				&outOid,
	CssmAllocator		&alloc);

/* convert algorithm identifier between CSSM and snacc formats */
void CL_cssmAlgIdToSnacc (
	const CSSM_X509_ALGORITHM_IDENTIFIER &cssmAlgId,
	AlgorithmIdentifier 			&snaccAlgId);

void CL_snaccAlgIdToCssm (
	const AlgorithmIdentifier 		&snaccAlgId,
	CSSM_X509_ALGORITHM_IDENTIFIER	&cssmAlgId,
	CssmAllocator					&alloc);

/* convert between uint32-style CSSM algorithm and snacc-style AsnOid */
CSSM_ALGORITHMS CL_snaccOidToCssmAlg(
	const AsnOid 					&oid);

void CL_cssmAlgToSnaccOid(
	CSSM_ALGORITHMS 				cssmAlg,
	AsnOid 							&oid);

/* set up a encoded NULL for AlgorithmIdentifier.parameters */
void CL_nullAlgParams(
	AlgorithmIdentifier				&snaccAlgId);

/* AsnOcts --> CSSM_DATA */
void CL_AsnOctsToCssmData(
	const AsnOcts 					&octs,
	CSSM_DATA						&cdata,
	CssmAllocator					&alloc);

/* snacc-style GeneralNames --> CE_GeneralNames */
/* GeneralNames from sm_x509cmn.h */
void CL_snaccGeneralNamesToCdsa(
	GeneralNames &snaccObj,
	CE_GeneralNames &cdsaObj,
	CssmAllocator &alloc);

/* CE_GeneralNames --> snacc-style GeneralNames */
GeneralNames *CL_cdsaGeneralNamesToSnacc(
	CE_GeneralNames 				&cdsaObj);

#define MAX_RDN_SIZE	(4 * 1024)

void CL_normalizeString(
	char 			*strPtr,
	int 			&strLen);
void CL_normalizeX509Name(
	Name 			&name,
	CssmAllocator 	&alloc);

/*
 * Obtain a CSSM_KEY from a SubjectPublicKeyInfo, inferring as much as we can
 * from required fields (subjectPublicKeyInfo) and extensions (for 
 * KeyUse, obtained from the optional DecodedCert).
 */
CSSM_KEY_PTR CL_extractCSSMKey(
	SubjectPublicKeyInfo	&snaccKeyInfo,
	CssmAllocator			&alloc,
	const DecodedCert		*decodedCert);			// optional

/*
 * Free key obtained in CL_extractCSSMKey().
 */
void CL_freeCSSMKey(
		CSSM_KEY_PTR		cssmKey,
		CssmAllocator		&alloc,
		bool				freeTop = true);	// delete the actual key
												// as well as contents

#ifdef __cplusplus
}
#endif

#endif	/* _SNACC_UTILS_H_ */

