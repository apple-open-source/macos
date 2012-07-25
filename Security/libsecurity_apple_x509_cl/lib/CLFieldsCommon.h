/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
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
 * CLFieldsCommon.h - get/set/free routines common to certs and CRLs
 */

#ifndef	_CL_FIELDS_COMMON_H_
#define _CL_FIELDS_COMMON_H_

#include <Security/cssmtype.h>
#include <security_cdsa_utilities/cssmdata.h>

#include "DecodedItem.h"

#include <security_utilities/globalizer.h>

#include <Security/X509Templates.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * As of the NSS conversion, getField ops do NOT take a const
 * DecodedItem argument since many of them use the DecodedItem's
 * SecNssCoder for intermediate ops.
 */
typedef bool (getItemFieldFcn) (
	DecodedItem			&item,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue);	// RETURNED
typedef void (setItemFieldFcn) (
	DecodedItem			&item,
	const CssmData		&fieldValue);
typedef void (freeFieldFcn) (
	CssmOwnedData		&fieldValue);

bool clOidToNssInfo(
	const CSSM_OID			&oid,
	unsigned				&nssObjLen,		// RETURNED
	const SecAsn1Template	*&templ);		// RETURNED

/*
 * Routines for common validity checking for certificateToSign fields.
 *
 * Call from setField*: verify field isn't already set, optionally validate
 * input length
 */
void tbsSetCheck(
	void				*fieldToSet,
	const CssmData		&fieldValue,
	uint32				expLength,
	const char			*op);

/*
 * Call from getField* for unique fields - detect missing field or 
 * index out of bounds.
 */
bool tbsGetCheck(
	const void			*requiredField,
	uint32				reqIndex);

/* common extension get/set/free */
void getFieldExtenCommon(
	void 				*cdsaObj,			// e.g. CE_KeyUsage
											// CSSM_DATA_PTR for berEncoded
	const DecodedExten &decodedExt, 
	CssmOwnedData		&fieldValue); 
	
CSSM_X509_EXTENSION_PTR verifySetFreeExtension(
	const CssmData 		&fieldValue,
	bool 				berEncoded);		// false: value in value.parsedValue
											// true : value in BERValue
void freeFieldExtenCommon(
	CSSM_X509_EXTENSION_PTR	exten,
	Allocator			&alloc);

/*
 * Common code for get/set subject/issuer name (C struct version)
 */
bool getField_RDN_NSS (
	const NSS_Name 		&nssName,
	CssmOwnedData		&fieldValue);	// RETURNED

void freeField_RDN  (
	CssmOwnedData		&fieldValue);

/* get normalized RDN */
bool getField_normRDN_NSS (
	const CSSM_DATA		&derName,
	uint32				&numFields,		// RETURNED (if successful, 0 or 1)
	CssmOwnedData		&fieldValue);	// RETURNED

/*
 * Common code for Time fields - Validity not before/after, this/next update
 * Format: CSSM_X509_TIME
 */
void freeField_Time (
	CssmOwnedData	&fieldValue);

bool getField_TimeNSS (
	const NSS_Time 	&derTime,
	unsigned		index,			// which occurrence (0 = first)
	uint32			&numFields,		// RETURNED
	CssmOwnedData	&fieldValue);	// RETURNED
void setField_TimeNSS (
	const CssmData	&fieldValue,
	NSS_Time		&nssTime,
	SecNssCoder		&coder);

void getField_AlgIdNSS (
	const CSSM_X509_ALGORITHM_IDENTIFIER 	&srcAlgId,
	CssmOwnedData							&fieldValue);	// RETURNED
void setField_AlgIdNSS (
	const CssmData					&fieldValue,
	CSSM_X509_ALGORITHM_IDENTIFIER 	&dstAlgId,
	SecNssCoder						&coder);

void freeField_AlgId (
	CssmOwnedData				&fieldValue);

getItemFieldFcn getFieldUnknownExt, getField_Unimplemented;
setItemFieldFcn setFieldUnknownExt, setField_ReadOnly;
freeFieldFcn freeFieldUnknownExt, freeFieldSimpleExtension;

#ifdef	__cplusplus
}
#endif

#endif	/* _CL_FIELDS_COMMON_H_ */
