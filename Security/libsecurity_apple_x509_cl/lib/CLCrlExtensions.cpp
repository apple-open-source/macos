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
 * CLCrlExtensions.cpp - CRL extensions support.
 */
 
#include "DecodedCrl.h"
#include "CLCrlExtensions.h"
#include "CLCertExtensions.h"
#include "clNssUtils.h"
#include "clNameUtils.h"
#include "CLFieldsCommon.h"
#include <security_utilities/utilities.h>
#include <Security/oidscert.h>
#include <Security/cssmerr.h>
#include <Security/x509defs.h>
#include <Security/certextensions.h>

#include <Security/SecAsn1Templates.h>

/***
 *** get/set/free functions called out from CrlFields.cpp
 ***/
/***
 *** CrlNumber , DeltaCRL
 *** CDSA format 	CE_CrlNumber (a uint32)
 *** NSS format 	CSSM_DATA, length 4
 *** OID 			CSSMOID_CrlNumber, CSSMOID_DeltaCrlIndicator
 ***/
 
/* set function for both */
void setFieldCrlNumber(		
	DecodedItem	&crl, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, 
		false);
	CE_CrlNumber *cdsaObj = (CE_CrlNumber *)cssmExt->value.parsedValue;
	
	/* CSSM_DATA and its contents in crl.coder's memory */
	ArenaAllocator alloc(crl.coder());
	CSSM_DATA_PTR nssVal = (CSSM_DATA_PTR)alloc.malloc(sizeof(CSSM_DATA));
	clIntToData(*cdsaObj, *nssVal, alloc);
	
	/* add to mExtensions */
	crl.addExtension(nssVal, cssmExt->extnId, cssmExt->critical, false,
		kSecAsn1IntegerTemplate); 
}

static
bool getFieldCrlCommon(
	DecodedItem		 	&crl,
	const CSSM_OID		&fieldId,		// identifies extension we seek
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	const DecodedExten *decodedExt;
	CSSM_DATA *nssObj;
	CE_CrlNumber *cdsaObj;
	bool brtn;
	
	brtn = crl.GetExtenTop<CSSM_DATA, CE_CrlNumber>(
		index,
		numFields,
		fieldValue.allocator,
		fieldId,
		nssObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	*cdsaObj = clDataToInt(*nssObj, CSSMERR_CL_INVALID_CRL_POINTER);
	
	/* pass back to caller */
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

bool getFieldCrlNumber(
	DecodedItem		 	&crl,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	return getFieldCrlCommon(crl, CSSMOID_CrlNumber, index, numFields, 
		fieldValue);
}

bool getFieldDeltaCrl(
	DecodedItem		 	&crl,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	return getFieldCrlCommon(crl, CSSMOID_DeltaCrlIndicator, index, 
		numFields, fieldValue);
}

void freeFieldIssuingDistPoint (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	Allocator &alloc = fieldValue.allocator;
	CE_IssuingDistributionPoint *cdsaObj = 
			(CE_IssuingDistributionPoint *)cssmExt->value.parsedValue;
	CL_freeCssmIssuingDistPoint(cdsaObj, alloc);
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}

void freeFieldCrlDistributionPoints (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	Allocator &alloc = fieldValue.allocator;
	CE_CRLDistPointsSyntax *cdsaObj = 
			(CE_CRLDistPointsSyntax *)cssmExt->value.parsedValue;
	CL_freeCssmDistPoints(cdsaObj, alloc);
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}

/* HoldInstructionCode - CSSM_OID */
/* InvalidityDate - CSSM_DATA */
void freeFieldOidOrData (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	Allocator &alloc = fieldValue.allocator;
	CSSM_DATA *cdsaObj = 
			(CSSM_DATA *)cssmExt->value.parsedValue;
	if(cdsaObj) {
		alloc.free(cdsaObj->Data);
	}
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}

