/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
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
 * clNssUtils.h - support for libnssasn1-based ASN1 encode/decode 
 */
 
#ifndef	_CL_NSS_UTILS_H_
#define _CL_NSS_UTILS_H_

#include <security_asn1/SecNssCoder.h>
#include <Security/certExtensionTemplates.h>
#include <security_utilities/alloc.h>
#include <Security/cssm.h>
#include "DecodedCert.h"

/*
 * A Allocator which is actually based upon a PLArenaPool. This only
 * mallocs, it doesn't have a free - all memory allocated with this
 * object is freed when the SecNssCoder associated with this object is 
 * freed. It's used to malloc the fields in DecodedCert.mCert and 
 * DecodedCrl.mCrl.
 */
class ArenaAllocator : public Security::Allocator
{
	NOCOPY(ArenaAllocator)
public:
	ArenaAllocator(SecNssCoder &coder)
		: mCoder(coder) { }
	~ArenaAllocator() { }
	void *malloc(size_t) throw(std::bad_alloc) ;
	void free(void *) throw() ;
	void *realloc(void *, size_t) throw(std::bad_alloc);
private:
	SecNssCoder		&mCoder;
};

/* 
 * Misc. alloc/copy with arbitrary Allocator 
 */

/* malloc d.Data, set d.Length */
void clAllocData(
	Allocator	&alloc,
	CSSM_DATA		&dst,
	size_t			len);

/* malloc and copy */
void clAllocCopyData(
	Allocator	&alloc,
	const CSSM_DATA	&src,
	CSSM_DATA		&dst);

/* return true if two CSSM_DATAs (or two CSSM_OIDs) compare equal */
bool clCompareCssmData(
	const CSSM_DATA *data1,
	const CSSM_DATA *data2);

/*
 * CSSM_DATA --> uint32
 */
uint32 clDataToInt(
	const CSSM_DATA &cdata, 
	CSSM_RETURN toThrow = CSSMERR_CL_INVALID_CERT_POINTER);
void clIntToData(
	uint32 num,
	CSSM_DATA &cdata,
	Allocator &alloc);
	
/* CSSM_BOOL <--> CSSM_DATA */
CSSM_BOOL clNssBoolToCssm(
	const CSSM_DATA	&nssBool);
void clCssmBoolToNss(
	CSSM_BOOL cBool,
	CSSM_DATA &nssBool,
	Allocator &alloc);

/* Bit String */
void clCssmBitStringToNss(
	CSSM_DATA &b);
void clNssBitStringToCssm(
	CSSM_DATA &b);

/* How many items in a NULL-terminated array of pointers? */
unsigned clNssArraySize(
	const void **array);

/* malloc a NULL-ed array of pointers of size num+1 */
void **clNssNullArray(
	uint32 num,
	SecNssCoder &coder);

CE_KeyUsage clBitStringToKeyUsage(
	const CSSM_DATA &cdata);

CSSM_ALGORITHMS CL_oidToAlg(
	const CSSM_OID &oid);

void CL_copyAlgId(
	const CSSM_X509_ALGORITHM_IDENTIFIER &srcAlgId, 
	CSSM_X509_ALGORITHM_IDENTIFIER &destAlgId, 
	Allocator &alloc);
void CL_freeCssmAlgId(
	CSSM_X509_ALGORITHM_IDENTIFIER	*cdsaObj,		// optional
	Allocator 					&alloc);


bool CL_nssTimeToCssm(
	const NSS_Time	    &derTime,
	CSSM_X509_TIME		&cssmObj,
	Allocator 		&alloc);	
void CL_cssmTimeToNss(
	const CSSM_X509_TIME &cssmTime, 
	NSS_Time			&nssTime, 
	SecNssCoder 		&coder);
void CL_freeCssmTime(
	CSSM_X509_TIME		*cssmTime,
	Allocator		&alloc);

void CL_nullAlgParams(
	CSSM_X509_ALGORITHM_IDENTIFIER	&algId);

void CL_copySubjPubKeyInfo(
	const CSSM_X509_SUBJECT_PUBLIC_KEY_INFO &srcInfo, 
	bool srcInBits,
	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO &dstInfo, 
	bool dstInBits,
	Allocator &alloc);
CSSM_KEY_PTR CL_extractCSSMKeyNSS(
	const CSSM_X509_SUBJECT_PUBLIC_KEY_INFO	&keyInfo,
	Allocator				&alloc,
	const DecodedCert		*decodedCert);			// optional
void CL_CSSMKeyToSubjPubKeyInfoNSS(
	const CSSM_KEY 						&cssmKey,
	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO	&nssKeyInfo,
	SecNssCoder							&coder);
void CL_freeCSSMKey(
	CSSM_KEY_PTR		cssmKey,
	Allocator			&alloc,
	bool				freeTop = true);	// delete the actual key
											// as well as contents
											
void CL_cssmAuthorityKeyIdToNss(
	const CE_AuthorityKeyID 	&cdsaObj,
	NSS_AuthorityKeyId 			&nssObj,
	SecNssCoder 				&coder);
void CL_nssAuthorityKeyIdToCssm(
	const NSS_AuthorityKeyId 	&nssObj,
	CE_AuthorityKeyID 			&cdsaObj,
	SecNssCoder 				&coder,	// for temp decoding
	Allocator					&alloc);

void CL_cssmInfoAccessToNss(
	const CE_AuthorityInfoAccess	&cdsaObj,
	NSS_AuthorityInfoAccess			&nssObj,
	SecNssCoder						&coder);
void CL_infoAccessToCssm(
	const NSS_AuthorityInfoAccess 	&nssObj,
	CE_AuthorityInfoAccess			&cdsaObj,
	SecNssCoder						&coder,	// for temp decoding
	Allocator						&alloc);
void CL_freeInfoAccess(
	CE_AuthorityInfoAccess			&cssmInfo,
	Allocator						&alloc);

void CL_cssmQualCertStatementsToNss(
	const CE_QC_Statements	 	&cdsaObj,
	NSS_QC_Statements 			&nssObj,
	SecNssCoder 				&coder);
void CL_qualCertStatementsToCssm(
	const NSS_QC_Statements 	&nssObj,
	CE_QC_Statements 			&cdsaObj,
	SecNssCoder 				&coder,	// for temp decoding
	Allocator					&alloc);
void CL_freeQualCertStatements(
	CE_QC_Statements			&cssmQCs,
	Allocator					&alloc);

void CL_decodeDistributionPointName(
	const CSSM_DATA				&nssBlob,
	CE_DistributionPointName	&cssmDpn,
	SecNssCoder					&coder,
	Allocator					&alloc);
void CL_encodeDistributionPointName(
	CE_DistributionPointName 	&cpoint,
	CSSM_DATA 					&npoint,
	SecNssCoder 				&coder);
void CL_cssmDistPointsToNss(
	const CE_CRLDistPointsSyntax 	&cdsaObj,
	NSS_CRLDistributionPoints		&nssObj,
	SecNssCoder 					&coder);
void CL_nssDistPointsToCssm(
	const NSS_CRLDistributionPoints	&nssObj,
	CE_CRLDistPointsSyntax			&cdsaObj,
	SecNssCoder 					&coder,	// for temp decoding
	Allocator						&alloc);

void CL_nssIssuingDistPointToCssm(
	NSS_IssuingDistributionPoint *nssIdp,
	CE_IssuingDistributionPoint	*cssmIdp,
	SecNssCoder					&coder,
	Allocator					&alloc);

CSSM_ALGORITHMS CL_nssDecodeECDSASigAlgParams(
	const CSSM_DATA &algParams,
	SecNssCoder &coder);

void CL_certCrlDecodeComponents(
	const CssmData 	&signedItem,		// DER-encoded cert or CRL
	CssmOwnedData	&tbsBlob,			// still DER-encoded
	CssmOwnedData	&algId,				// ditto
	CssmOwnedData	&rawSig);			// raw bits (not an encoded AsnBits)
void 
CL_certEncodeComponents(
	const CssmData		&TBSCert,		// DER-encoded
	const CssmData		&algId,			// ditto
	const CssmData		&rawSig,		// raw bits, not encoded
	CssmOwnedData 		&signedCert);	// DER-encoded

#endif	/* _CL_NSS_UTILS_H_ */
