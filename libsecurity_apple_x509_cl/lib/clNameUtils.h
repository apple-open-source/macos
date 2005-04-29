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
 * clNameUtils.h - support for Name, GeneralizedName, all sorts of names
 */
 
#ifndef	_CL_NAME_UTILS_H_
#define _CL_NAME_UTILS_H_

#include <Security/cssmtype.h>
#include <security_utilities/alloc.h>
#include <Security/x509defs.h>
#include <Security/certextensions.h>
#include <Security/X509Templates.h>
#include <security_asn1/SecNssCoder.h>

void CL_nssAtvToCssm(
	const NSS_ATV				&nssObj,
	CSSM_X509_TYPE_VALUE_PAIR	&cssmObj,
	Allocator				&alloc
	#if !NSS_TAGGED_ITEMS
	, SecNssCoder					&coder
	#endif
	);
void CL_nssRdnToCssm(
	const NSS_RDN				&nssObj,
	CSSM_X509_RDN				&cssmObj,
	Allocator				&alloc,
	SecNssCoder					&coder);
void CL_nssNameToCssm(
	const NSS_Name				&nssObj,
	CSSM_X509_NAME				&cssmObj,
	Allocator				&alloc);

void CL_cssmAtvToNss(
	const CSSM_X509_TYPE_VALUE_PAIR	&cssmObj,
	NSS_ATV							&nssObj,
	SecNssCoder						&coder);
void CL_cssmRdnToNss(
	const CSSM_X509_RDN			&cssmObj,
	NSS_RDN						&nssObj,
	SecNssCoder					&coder);
void CL_cssmNameToNss(
	const CSSM_X509_NAME		&cssmObj,
	NSS_Name					&nssObj,
	SecNssCoder					&coder);

void CL_normalizeString(
	char 						*strPtr,
	int 						&strLen);		// IN/OUT
void CL_normalizeX509NameNSS(
	NSS_Name 					&nssName,
	SecNssCoder 				&coder);

void CL_nssGeneralNameToCssm(
	NSS_GeneralName &nssObj,
	CE_GeneralName &cdsaObj,
	SecNssCoder &coder,				// for temp decoding
	Allocator &alloc);			// destination 

void CL_nssGeneralNamesToCssm(
	const NSS_GeneralNames &nssObj,
	CE_GeneralNames &cdsaObj,
	SecNssCoder &coder,				// for temp decoding
	Allocator &alloc);			// destination 
void CL_cssmGeneralNameToNss(
	CE_GeneralName &cdsaObj,
	NSS_GeneralName &nssObj,		// actually an NSSTaggedItem
	SecNssCoder &coder);			// for temp decoding
void CL_cssmGeneralNamesToNss(
	const CE_GeneralNames &cdsaObj,
	NSS_GeneralNames &nssObj,
	SecNssCoder &coder);
	
void clCopyOtherName(
	const CE_OtherName 			&src,
	CE_OtherName 				&dst,
	Allocator					&alloc);

void CL_freeAuthorityKeyId(
	CE_AuthorityKeyID			&cdsaObj,
	Allocator					&alloc);
void CL_freeCssmGeneralName(
	CE_GeneralName				&genName,
	Allocator					&alloc);
void CL_freeCssmGeneralNames(
	CE_GeneralNames				*cdsaObj,
	Allocator					&alloc);
void CL_freeCssmDistPointName(
	CE_DistributionPointName	*cssmDpn,
	Allocator					&alloc);
void CL_freeCssmDistPoints(
	CE_CRLDistPointsSyntax		*cssmDps,
	Allocator					&alloc);
void CL_freeX509Name(
	CSSM_X509_NAME_PTR			x509Name,
	Allocator					&alloc);
void CL_freeX509Rdn(
	CSSM_X509_RDN_PTR			rdn,
	Allocator					&alloc);
void CL_freeOtherName(
	CE_OtherName				*cssmOther,
	Allocator					&alloc);
void CL_freeCssmIssuingDistPoint(
	CE_IssuingDistributionPoint	*cssmIdp,
	Allocator					&alloc);


#endif	/* _CL_NAME_UTILS_H_ */
