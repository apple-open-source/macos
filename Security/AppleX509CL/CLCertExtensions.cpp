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
 * CLCertExtensions.cpp - extensions support. A major component of DecodedCert.
 *
 * Created 9/8/2000 by Doug Mitchell. 
 * Copyright (c) 2000 by Apple Computer. 
 *
 */
 
#include "DecodedCert.h"
#include "cldebugging.h"
#include "CertBuilder.h"
#include "CLCertExtensions.h"
#include "SnaccUtils.h"
#include <Security/utilities.h>
#include <Security/oidscert.h>
#include <Security/cssmerr.h>
#include <Security/x509defs.h>
#include <Security/certextensions.h>
#include <Security/cdsaUtils.h>
#include <Security/sm_x509ce.h>
#include <Security/globalizer.h>

static AsnType *oidToSnaccObj(
	const AsnOid &extnId);

#define MIN_EXTENSIONS		4		// initial size of *mExtensions

/*
 * AsnOid "constants" which we construct and cache on demand to avoid the 
 * somewhat expensive op of constructing them every time we test for equality 
 * in oidToSnaccObj().
 */
class ExtOidCache
{
public:
	ExtOidCache() :
		mId_ce_keyUsage(id_ce_keyUsage_arc),
		mId_ce_basicConstraints(id_ce_basicConstraints_arc),
		mId_ce_extKeyUsage(id_ce_extKeyUsage_arc),
		mId_ce_subjectKeyIdentifier(id_ce_subjectKeyIdentifier_arc),
		mId_ce_authorityKeyIdentifier(id_ce_authorityKeyIdentifier_arc),
		mId_ce_subjectAltName(id_ce_subjectAltName_arc),
		mId_ce_certificatePolicies(id_ce_certificatePolicies_arc),
		mId_netscape_cert_type(id_netscape_cert_type_arc)
			{ }
		
	AsnOid 	mId_ce_keyUsage;
	AsnOid	mId_ce_basicConstraints;
	AsnOid	mId_ce_extKeyUsage;
	AsnOid	mId_ce_subjectKeyIdentifier;
	AsnOid	mId_ce_authorityKeyIdentifier;
	AsnOid	mId_ce_subjectAltName;
	AsnOid	mId_ce_certificatePolicies;
	AsnOid	mId_netscape_cert_type;
};

static ModuleNexus<ExtOidCache> extOidCache;

/*
 * Decode tbs->Extensions into mExtensions. This involves figuring out
 * what kind of object is represented in the octet string in the 
 * extension, decoding it, and placing the resulting AsnType in a 
 * new DecodedExten struct.
 *
 * Called when decoding either a cert (for caching it or getting its fields)
 * or a template (only via CertGetAllTemplateFields()).
 */
void DecodedCert::decodeExtensions()
{
	CASSERT(certificateToSign != NULL);
	Extensions *extensions = certificateToSign->extensions;
	if(extensions == NULL) {
		/* OK, no extensions present */
		return;
	}
	
	Extension *snaccExten = extensions->First();
	extensions->SetCurrToFirst();
	
	/* traverse extension list */
	while(snaccExten != 0) {
		/*
		 * For this extension->extnId, cook up an approppriate 
		 * AsnType (KeyUsage, etc.);
		 */
		AsnOid &extnId = snaccExten->extnId;
		bool berEncoded = false;
		AsnType *snaccObj = oidToSnaccObj(extnId);
		if(snaccObj == NULL) {
			/* 
			 * We don't know how to deal with this, just take the 
			 * raw bytes, copied from snaccExte.
			 */
			snaccObj = new AsnOcts(snaccExten->extnValue);
			berEncoded = true;
		}
		else {
			/* 
			 * We have a snacc-style object specific to this extension.
			 * Decode the extensions's extnValue into that object. We don't
			 * have to know what kind of object it is anymore. 
			 */
			CssmData cData(snaccExten->extnValue, snaccExten->extnValue.Len());
			try {
				SC_decodeAsnObj(cData, *snaccObj);
			}
			catch (...) {
				/* 
				 * FIXME - what do we do here? Is it safe to just ignore this
				 * extension, or is the whole cert invalid?
				 */
				errorLog0("decodeExtensions: extension decode error\n");
				delete snaccObj;
				snaccObj = NULL;
			}
		}	
		if(snaccObj != NULL) {
			/* add to mExtensions if the decode was successful */
			bool critical = false;		// default
			if(snaccExten->critical != NULL) {
				critical = *snaccExten->critical;
			}
			addExtension(snaccObj, 
				snaccExten->extnId, 
				critical,
				berEncoded);
		}
		extensions->GoNext();
		snaccExten = extensions->Curr();
	}
}

/*
 * Encode mExtensions into tbs->Extensions.
 *
 * Each extension object, currently stored as some AsnType subclass,
 * is BER-encoded and the reesult is stored as an octet string
 * (AsnOcts) in a new Extension object in the TBS.
 *
 * Only called from CertCreateTemplate via encodeTbs(). 
 */
#define MAX_EXTEN_SIZE	(4 * 1024)		/* SWAG for max encoded size */

void DecodedCert::encodeExtensions()
{
	CertificateToSign *tbs = certificateToSign;
	CASSERT(mState == CS_Building);
	CASSERT((tbs != NULL) && (tbs->extensions == NULL));

	if(mNumExtensions == 0) {
		/* no extensions, no error */
		return;
	}
	tbs->extensions = new Extensions;
	Extensions *extns = tbs->extensions;
	
	/* for each of our DecodedExtens, append an Extension object to
	 * tbs->extensions */
	unsigned extenDex;
	for(extenDex=0; extenDex<mNumExtensions; extenDex++) {
		Extension *snaccExt = extns->Append();
		DecodedExten *decodedExt = &mExtensions[extenDex];
		
		/* BER-encode the extension object if appropriate */
		if(decodedExt->berEncoded) {
			/* unknown extension type, it's already encoded */
			/* A failure of this dynamic cast is a fata internal error */
			AsnOcts *rawOcts = dynamic_cast<AsnOcts *>(decodedExt->snaccObj);
			if(rawOcts == NULL) {
				errorLog0("encodeExtensions: dynamic_cast failure!\n");
				CssmError::throwMe(CSSMERR_CL_INTERNAL_ERROR);
			}
			snaccExt->extnValue.Set(*rawOcts);
		}
		else {
			CssmAutoData aData(alloc);
			try {
				SC_encodeAsnObj(*decodedExt->snaccObj, aData, MAX_EXTEN_SIZE);
			}
			catch (...) {
				errorLog0("encodeExtensions: extension encode error\n");
				throw;
			}
			CssmData &cData = aData.get();
			snaccExt->extnValue.Set((char *)cData.data(), cData.length());
		}
		snaccExt->critical = new AsnBool(decodedExt->critical);
		snaccExt->extnId.Set(*decodedExt->extnId);
	}
}


/* 
 * Add a new DecodedExten to mExtensions.
 * Called from decodeExtensions and setField*.
 * At this point, the actual extenmsion data is represented by some subclass
 * of AsnType - either a specific extension type (e.g. KeyUsage), or as an
 * octet string (AsnOcts) for extension types we don't understand (and which 
 * are encoded by the app). 
 */
void DecodedCert::addExtension(
	AsnType 			*snaccObj,	// e.g. KeyUsage
	const AsnOid		&extnId,	
	bool				critical,
	bool				berEncoded)	// i.e., we don't know how to parse
{
	/* cook up a new DecodedExten, reallocing mExtensions is necessary */
	if(mNumExtensions == mSizeofExtensions) {
		/* expand by doubling, or initial malloc */
		mSizeofExtensions = mNumExtensions ? 
			(2 * mNumExtensions) : MIN_EXTENSIONS;
		mExtensions = (DecodedExten *)alloc.realloc(
			mExtensions, mSizeofExtensions * sizeof(DecodedExten));
	}
	DecodedExten *decodedExt = &mExtensions[mNumExtensions++];
	decodedExt->extnId     = new AsnOid(extnId);
	decodedExt->critical   = critical;
	decodedExt->snaccObj   = snaccObj;
	decodedExt->berEncoded = berEncoded;
}

/* 
 * Search for DecodedExten by AsnOid or "any unknown extension".
 * Called from getField*() and inferKeyUsage. 
 * Returns NULL if specified extension not found.
 */
DecodedExten *DecodedCert::findDecodedExt(
	const AsnOid		&extnId,		// for known extensions
	bool				unknown,		// otherwise		
	uint32				index, 
	uint32				&numFields) const
{
	unsigned dex;
	DecodedExten *decodedExt;
	DecodedExten *rtnExt = NULL;
	unsigned found = 0;
	
	for(dex=0; dex<mNumExtensions; dex++) {
		CASSERT(mExtensions != NULL);
		decodedExt = &mExtensions[dex];
		/*
		 * known extensions: OID match
		 * unknown extensions: just know that we didn't decode it
		 */
		if( (!unknown && (*decodedExt->extnId == extnId)) || 
		    (unknown && decodedExt->berEncoded)) {
			
			if(found++ == index) {
				/* the one we want */
				rtnExt = decodedExt;
			}
			if((rtnExt != NULL) && (index != 0)) {
				/* only determine numFields on search for first one */
				break;
			}
		}
	}
	if(rtnExt != NULL) {
		/* sucessful return  */
		if(index == 0) {
			numFields = found;
		}
		return rtnExt;
	}
	else {
		return NULL;
	}
}

/*
 * Common code to pass info from a DecodedExten back to app.
 * Called from getField*().
 */
static void getFieldExtenCommon(
	void 				*cdsaObj,			// e.g. CE_KeyUsage
											// CSSM_DATA_PTR for berEncoded
	const DecodedExten	&decodedExt, 
	CssmOwnedData		&fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt;
	CssmAllocator &alloc = fieldValue.allocator;
	CssmData &fdata = fieldValue.get();
	
	cssmExt = (CSSM_X509_EXTENSION_PTR)alloc.malloc(sizeof(CSSM_X509_EXTENSION));
	fdata.Data = (uint8 *)cssmExt;
	fdata.Length = sizeof(CSSM_X509_EXTENSION);
	CL_snaccOidToCssm(*decodedExt.extnId,
		CssmOid::overlay(cssmExt->extnId),
		alloc);
	if(decodedExt.critical != NULL) {
		cssmExt->critical = decodedExt.critical ? CSSM_TRUE : CSSM_FALSE;
	}
	else {
		/* default */
		cssmExt->critical = false;
	}
	if(decodedExt.berEncoded) {
		/* an extension we never parsed or understood */
		cssmExt->format = CSSM_X509_DATAFORMAT_ENCODED;
		cssmExt->value.parsedValue = NULL;
		cssmExt->BERvalue = *(reinterpret_cast<CSSM_DATA_PTR>(cdsaObj));
	}
	else {
		cssmExt->format = CSSM_X509_DATAFORMAT_PARSED;
		cssmExt->value.parsedValue = cdsaObj;
		cssmExt->BERvalue.Data = NULL;
		cssmExt->BERvalue.Length = 0;
	}
}

/* 
 * Common code for top of setField* and freeField*().
 */
static CSSM_X509_EXTENSION_PTR verifySetFreeExtension(
	const CssmData &fieldValue,
	bool berEncoded)		// false: value in value.parsedValue
							// true : value in BERValue
{
	if(fieldValue.length() != sizeof(CSSM_X509_EXTENSION)) {
		errorLog2("Set/FreeExtension: bad length : exp %d got %d\n", 
			(int)sizeof(CSSM_X509_EXTENSION), (int)fieldValue.length());
		CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);			
	}
	CSSM_X509_EXTENSION_PTR cssmExt = 
		reinterpret_cast<CSSM_X509_EXTENSION_PTR>(fieldValue.data());
	if(berEncoded) {
		if((cssmExt->value.parsedValue != NULL) || (cssmExt->BERvalue.Data == NULL)) {
			CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);		
		}
	}
	else {
		if((cssmExt->value.parsedValue == NULL) || (cssmExt->BERvalue.Data != NULL)) {
			CssmError::throwMe(CSSMERR_CL_INVALID_FIELD_POINTER);		
		}
	}
	return cssmExt;
}

/*
 * Common free code for all extensions. Extension-specific code must
 * free anything beyond cdsaExt->Value.parsedValue, then we free everything 
 * else (except the extension struct itself, which is freed by 
 * DecodedCert::freeCertFieldData()). 
 */
static void freeFieldExtenCommon(
	CSSM_X509_EXTENSION_PTR	exten,
	CssmAllocator			&alloc)
{
	alloc.free(exten->extnId.Data);
	alloc.free(exten->BERvalue.Data);		// may be NULL
	alloc.free(exten->value.parsedValue);		// may be NULL
}


/* 
 * Cook up an AsnType associated with specified extnId 
 * When adding oid/type pairs here, also add to:
 *   -- fieldFuncs[] in CertFields.cpp
 *   -- and the get/set/free functions in this file.
 */
static AsnType *oidToSnaccObj(
	const AsnOid &extnId)
{
	ExtOidCache &oc = extOidCache();
	
	if(extnId == oc.mId_ce_keyUsage) {
		return new KeyUsage;
	}
	else if(extnId == oc.mId_ce_basicConstraints) {
		return new BasicConstraintsSyntax;
	}
	else if(extnId == oc.mId_ce_extKeyUsage) {
		return new ExtKeyUsageSyntax;
	}	
	else if(extnId == oc.mId_ce_subjectKeyIdentifier) {
		return new AsnOcts;
	}
	else if(extnId == oc.mId_ce_authorityKeyIdentifier) {
		return new AuthorityKeyIdentifier;
	}
	else if(extnId == oc.mId_ce_subjectAltName) {
		return new GeneralNames;
	}
	else if(extnId == oc.mId_ce_certificatePolicies) {
		return new CertificatePoliciesSyntax;
	}
	else if(extnId == oc.mId_netscape_cert_type) {
		return new AsnBits;
	}
	else {
		return NULL;
	}
}

/* common code for top of getField* */
template<class SnaccType, class CdsaType>
bool GetFieldTop(
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmAllocator		&alloc,
	const AsnOid		&fieldId,
	SnaccType			*&snaccObj,
	CdsaType			*&cdsaObj,
	DecodedExten		*&decodedExt)
{
	/* See if we have one of these in our list of DecodedExtens */
	decodedExt = cert.findDecodedExt(fieldId, false, index, numFields);
	if(decodedExt == NULL) {
		return false;
	}
	
	/* failure of this dynamic_cast is fatal */
	snaccObj = dynamic_cast<SnaccType *>(decodedExt->snaccObj);  
	if(snaccObj == NULL) {
		errorLog0("GetFieldTop: dynamic_cast failure\n");
		CssmError::throwMe(CSSMERR_CL_INTERNAL_ERROR);
	}
	cdsaObj = (CdsaType *)alloc.malloc(sizeof(CdsaType));
	memset(cdsaObj, 0, sizeof(CdsaType));
	return true;
}

/***
 *** get/set/free functions called out from CertFields.cpp
 ***/

/*
 * One common free for extensions whose parsed value doesn't go any deeper 
 * than cssmExt->value.parsedValue. 
 */
void freeFieldSimpleExtension (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	freeFieldExtenCommon(cssmExt, fieldValue.allocator);
}

/***
 *** KeyUsage 
 *** CDSA format 	CE_KeyUsage
 *** SNACC format 	KeyUsage
 *** OID 			CSSMOID_KeyUsage
 ***/
 
void setFieldKeyUsage(		
	DecodedCert	&cert, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	CE_KeyUsage *cdsaObj = (CE_KeyUsage *)cssmExt->value.parsedValue;
	
	/* brute-force cdsaObj --> snaccObj */
	char bits[sizeof(CE_KeyUsage)];
	bits[0] = static_cast<char>((*cdsaObj) >> 8);
	bits[1] = static_cast<char>(*cdsaObj);
	memmove(bits, cdsaObj, sizeof(CE_KeyUsage));
	KeyUsage *snaccObj = new KeyUsage(bits, sizeof(CE_KeyUsage) * 8);
	
	/* add to mExtensions */
	cert.addExtension(snaccObj, cssmExt->extnId, cssmExt->critical, false); 
}


bool getFieldKeyUsage(
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	DecodedExten *decodedExt;
	KeyUsage *snaccObj;
	CE_KeyUsage *cdsaObj;
	bool brtn;
	
	brtn = GetFieldTop<KeyUsage, CE_KeyUsage>(
		cert,
		index,
		numFields,
		fieldValue.allocator,
		id_ce_keyUsage,
		snaccObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	
	unsigned toCopy = (snaccObj->BitLen() + 7) / 8;
	if(toCopy > 2) {
		/* I hope I never see this... */
		errorLog0("getFieldKeyUsage: KeyUsage larger than 2 bytes!\n");
		toCopy = 2;
	}
	unsigned char bits[2] = {0, 0};
	memmove(bits, snaccObj->BitOcts(), toCopy);
	*cdsaObj = (((unsigned)bits[0]) << 8) | bits[1];
	
	/* pass back to caller */
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

/***
 *** Basic Constraints 
 *** CDSA format: 	CE_BasicConstraints
 *** SNACC format 	BasicConstraintsSyntax
 *** OID 			CSSMOID_BasicConstraints
 ***/
 
void setFieldBasicConstraints(		
	DecodedCert	&cert, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	BasicConstraintsSyntax *snaccObj = new BasicConstraintsSyntax;
	CE_BasicConstraints *cdsaObj = (CE_BasicConstraints *)cssmExt->value.parsedValue;
	
	/* brute-force cdsaObj --> snaccObj */
	snaccObj->cA = new AsnBool(cdsaObj->cA ? true : false);
	if(cdsaObj->pathLenConstraintPresent) {
		AsnIntType val = cdsaObj->pathLenConstraint;
		snaccObj->pathLenConstraint = new AsnInt(val);
	}

	/* add to mExtensions */
	cert.addExtension(snaccObj, cssmExt->extnId, cssmExt->critical, false); 
}


bool getFieldBasicConstraints(
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	DecodedExten *decodedExt;
	BasicConstraintsSyntax *snaccObj;
	CE_BasicConstraints *cdsaObj;
	bool brtn;
	
	brtn = GetFieldTop<BasicConstraintsSyntax, CE_BasicConstraints>(
		cert,
		index,
		numFields,
		fieldValue.allocator,
		id_ce_basicConstraints,
		snaccObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}

	if(snaccObj->cA == NULL) {
		/* default */
		cdsaObj->cA = CSSM_FALSE;
	}
	else {
		bool val = *snaccObj->cA;
		cdsaObj->cA = val ? CSSM_TRUE : CSSM_FALSE;
	}
	if(snaccObj->pathLenConstraint == NULL) {
		cdsaObj->pathLenConstraintPresent = CSSM_FALSE;
		cdsaObj->pathLenConstraint = 0;
	}
	else {
		cdsaObj->pathLenConstraintPresent = CSSM_TRUE;
		AsnIntType val = *snaccObj->pathLenConstraint;
		cdsaObj->pathLenConstraint = val;
	}
	
	/* pass back to caller */
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

/***
 *** Extended Key Usage
 *** CDSA format: 	CE_ExtendedKeyUsage
 *** SNACC format 	ExtKeyUsageSyntax
 *** OID 			CSSMOID_ExtendedKeyUsage
 ***/
void setFieldExtKeyUsage(		
	DecodedCert	&cert, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	ExtKeyUsageSyntax *snaccObj = new ExtKeyUsageSyntax;
	CE_ExtendedKeyUsage *cdsaObj = (CE_ExtendedKeyUsage *)cssmExt->value.parsedValue;
	
	/* brute-force cdsaObj --> snaccObj, one 'purpose' (OID) at a time */
	unsigned oidDex;
	for(oidDex=0; oidDex<cdsaObj->numPurposes; oidDex++) {
		KeyPurposeId *snaccPurp = snaccObj->Append();
		CSSM_OID_PTR cdsaPurp = &cdsaObj->purposes[oidDex];
		snaccPurp->Set(reinterpret_cast<char *>(cdsaPurp->Data), cdsaPurp->Length); 
	}

	/* add to mExtensions */
	cert.addExtension(snaccObj, cssmExt->extnId, cssmExt->critical, false); 
}

bool getFieldExtKeyUsage(
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	DecodedExten *decodedExt;
	ExtKeyUsageSyntax *snaccObj;
	CE_ExtendedKeyUsage *cdsaObj;
	bool brtn;
	
	brtn = GetFieldTop<ExtKeyUsageSyntax, CE_ExtendedKeyUsage>(
		cert,
		index,
		numFields,
		fieldValue.allocator,
		id_ce_extKeyUsage,
		snaccObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	
	/* brute force snaccObj --> cdsaObj, one purpose at a time */ 
	CssmAllocator &alloc = fieldValue.allocator;
	cdsaObj->numPurposes = snaccObj->Count();
	cdsaObj->purposes = (CSSM_OID_PTR)alloc.malloc(cdsaObj->numPurposes * 
			sizeof(CSSM_OID));
	snaccObj->SetCurrToFirst();
	unsigned oidDex;
	for(oidDex=0; oidDex<cdsaObj->numPurposes; oidDex++) {
		CL_snaccOidToCssm(*snaccObj->Curr(),
			CssmOid::overlay(cdsaObj->purposes[oidDex]),
			alloc);
		snaccObj->GoNext();
	}
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

void freeFieldExtKeyUsage(
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	CssmAllocator &alloc = fieldValue.allocator;
	CE_ExtendedKeyUsage *cdsaObj = 
		(CE_ExtendedKeyUsage *)cssmExt->value.parsedValue;
	unsigned oidDex;
	for(oidDex=0; oidDex<cdsaObj->numPurposes; oidDex++) {
		alloc.free(cdsaObj->purposes[oidDex].Data);
	}
	alloc.free(cdsaObj->purposes);
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}

/***
 *** Subject Key Identifier
 *** CDSA format: 	CE_SubjectKeyID, which is just a CSSM_DATA
 *** SNACC format 	AsnOcts
 *** OID 			CSSMOID_SubjectKeyIdentifier
 ***/
 
void setFieldSubjectKeyId(		
	DecodedCert	&cert, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	CE_SubjectKeyID *cdsaObj = (CE_SubjectKeyID *)cssmExt->value.parsedValue;
	AsnOcts *snaccObj = new AsnOcts((char *)cdsaObj->Data, cdsaObj->Length);
	cert.addExtension(snaccObj, cssmExt->extnId, cssmExt->critical, false); 
}

bool getFieldSubjectKeyId(
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	DecodedExten *decodedExt;
	AsnOcts *snaccObj;
	CE_SubjectKeyID *cdsaObj;
	bool brtn;
	
	brtn = GetFieldTop<AsnOcts, CE_SubjectKeyID>(
		cert,
		index,
		numFields,
		fieldValue.allocator,
		id_ce_subjectKeyIdentifier,
		snaccObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	CL_AsnOctsToCssmData(*snaccObj, *cdsaObj, fieldValue.allocator);
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

void freeFieldSubjectKeyId (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	CssmAllocator &alloc = fieldValue.allocator;
	CE_SubjectKeyID *cdsaObj = (CE_SubjectKeyID *)cssmExt->value.parsedValue;
	alloc.free(cdsaObj->Data);
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}

/***
 *** Authority Key Identifier
 *** CDSA format: 	CE_AuthorityKeyID
 *** SNACC format 	AuthorityKeyIdentifier
 *** OID 			CSSMOID_AuthorityKeyIdentifier
 ***/
 
void setFieldAuthorityKeyId(		
	DecodedCert	&cert, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	CE_AuthorityKeyID *cdsaObj = (CE_AuthorityKeyID *)cssmExt->value.parsedValue;
	AuthorityKeyIdentifier *snaccObj = new AuthorityKeyIdentifier;

	/* 
	 * brute-force a CDSA-style CE_AuthorityKeyID into snacc-style 
	 * AuthorityKeyIdentifier
	 */
	if(cdsaObj->keyIdentifierPresent) {
		/* Just an AsnOcts */
		snaccObj->keyIdentifier = new AsnOcts((char *)cdsaObj->keyIdentifier.Data,
			cdsaObj->keyIdentifier.Length);
	}
	if(cdsaObj->generalNamesPresent ) {
		/* GeneralNames, the hard one */
		snaccObj->authorityCertIssuer = CL_cdsaGeneralNamesToSnacc(
			*cdsaObj->generalNames);
	}
	if(cdsaObj->serialNumberPresent) {
		/* boils down to BigIntegerStr, or AsnOcts */
		snaccObj->authorityCertSerialNumber = 
			new CertificateSerialNumber((char *)cdsaObj->serialNumber.Data,
				cdsaObj->serialNumber.Length);
	}
	
	cert.addExtension(snaccObj, cssmExt->extnId, cssmExt->critical, false); 
}

bool getFieldAuthorityKeyId(
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	DecodedExten *decodedExt;
	AuthorityKeyIdentifier *snaccObj;
	CE_AuthorityKeyID *cdsaObj;
	bool brtn;
	CssmAllocator &alloc = fieldValue.allocator;
	
	brtn = GetFieldTop<AuthorityKeyIdentifier, CE_AuthorityKeyID>(
		cert,
		index,
		numFields,
		alloc,
		id_ce_authorityKeyIdentifier,
		snaccObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	
	/* brute-force a snacc-style AuthorityKeyIdentifier into CDSA format */
	if(snaccObj->keyIdentifier != NULL) {
		/* Just an AsnOcts */
		cdsaObj->keyIdentifierPresent = CSSM_TRUE;
		CL_AsnOctsToCssmData(*snaccObj->keyIdentifier, 
			cdsaObj->keyIdentifier, 
			alloc);
	}
	if(snaccObj->authorityCertIssuer != NULL) {
		/* GeneralNames, the hard one */
		cdsaObj->generalNamesPresent = CSSM_TRUE;
		cdsaObj->generalNames = (CE_GeneralNames *)alloc.malloc(sizeof(CE_GeneralName));
			CL_snaccGeneralNamesToCdsa(*snaccObj->authorityCertIssuer, 
				*cdsaObj->generalNames,
				alloc);
	}
	if(snaccObj->authorityCertSerialNumber != NULL) {
		/* boils down to BigIntegerStr, or AsnOcts */
		cdsaObj->serialNumberPresent = CSSM_TRUE;
		CL_AsnOctsToCssmData(*snaccObj->authorityCertSerialNumber, 
			cdsaObj->serialNumber, 
			alloc);
	}
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

static void freeFieldGeneralNames(
	CE_GeneralNames *cdsaObj,
	CssmAllocator &alloc)
{
	if(cdsaObj == NULL) {
		return;
	}
	for(unsigned i=0; i<cdsaObj->numNames; i++) {
		alloc.free(cdsaObj->generalName[i].name.Data);
	}
	if(cdsaObj->numNames) {
		memset(cdsaObj->generalName, 0, cdsaObj->numNames * sizeof(CE_GeneralName));
	}
	memset(cdsaObj, 0, sizeof(CE_GeneralNames));
}

void freeFieldAuthorityKeyId (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	CssmAllocator &alloc = fieldValue.allocator;
	CE_AuthorityKeyID *cdsaObj = (CE_AuthorityKeyID *)cssmExt->value.parsedValue;
	alloc.free(cdsaObj->keyIdentifier.Data);
	freeFieldGeneralNames(cdsaObj->generalNames, alloc);
	alloc.free(cdsaObj->serialNumber.Data);
	memset(cdsaObj, 0, sizeof(CE_AuthorityKeyID));
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}

/***
 *** Subject alternate name
 *** CDSA Format:	CE_GeneralNames
 *** SNACC format:	GeneralNames
 *** OID: 			CSSMOID_SubjectAltName
 ***/
void setFieldSubjAltName(		
	DecodedCert	&cert, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	CE_GeneralNames *cdsaObj = (CE_GeneralNames *)cssmExt->value.parsedValue;
	GeneralNames *snaccObj = CL_cdsaGeneralNamesToSnacc(*cdsaObj);
	cert.addExtension(snaccObj, cssmExt->extnId, cssmExt->critical, false); 
}

bool getFieldSubjAltName(
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	DecodedExten *decodedExt;
	GeneralNames *snaccObj;
	CE_GeneralNames *cdsaObj;
	bool brtn;
	
	brtn = GetFieldTop<GeneralNames, CE_GeneralNames>(
		cert,
		index,
		numFields,
		fieldValue.allocator,
		id_ce_subjectAltName,
		snaccObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}
	CL_snaccGeneralNamesToCdsa(*snaccObj, *cdsaObj,	fieldValue.allocator);
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

void freeFieldSubjAltName (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	CssmAllocator &alloc = fieldValue.allocator;
	CE_GeneralNames *cdsaObj = (CE_GeneralNames *)cssmExt->value.parsedValue;
	freeFieldGeneralNames(cdsaObj, alloc);
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}

/***
 *** Certificate Policies
 *** CDSA Format:	CE_CertPolicies
 *** SNACC format:	CertificatePoliciesSyntax
 *** OID: 			CSSMOID_CertificatePolicies
 ***/
 
#define MAX_IA5_NAME_SIZE	1024

void setFieldCertPolicies(		
	DecodedCert	&cert, 
	const CssmData &fieldValue) 
{
	CssmAllocator &alloc = CssmAllocator::standard();
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	CertificatePoliciesSyntax *snaccObj = new CertificatePoliciesSyntax;
	CE_CertPolicies *cdsaObj = (CE_CertPolicies *)cssmExt->value.parsedValue;
	
	/* brute-force cdsaObj --> snaccObj */
	for(unsigned polDex=0; polDex<cdsaObj->numPolicies; polDex++) {
		CE_PolicyInformation *cPolInfo = &cdsaObj->policies[polDex];
		PolicyInformation *sPolInfo = snaccObj->Append();
		sPolInfo->policyIdentifier.Set((char *)cPolInfo->certPolicyId.Data,
			cPolInfo->certPolicyId.Length);
		if(cPolInfo->numPolicyQualifiers != 0) {
			sPolInfo->policyQualifiers = new PolicyInformationSeqOf;
		}
		for(unsigned qualDex=0; qualDex<cPolInfo->numPolicyQualifiers; qualDex++) {
			CE_PolicyQualifierInfo *cQualInfo = &cPolInfo->policyQualifiers[qualDex];
			PolicyQualifierInfo *sQualInfo = sPolInfo->policyQualifiers->Append();
			
			/* OK we're at the lowest level. 
			 * policyQualifierId == id_qt_cps: qualifier is an IA5 string,
			 * incoming data is its contents. Else incoming data is an encoded
			 * blob we pass on directly.
			 */
			sQualInfo->policyQualifierId.Set(
				(char *)cQualInfo->policyQualifierId.Data,
					cQualInfo->policyQualifierId.Length);

			/* we'll convert this incoming ptr/len.... */
			uint8 *rawData = cQualInfo->qualifier.Data;
			unsigned rawDataLen = cQualInfo->qualifier.Length;
			/* to this, somehow; it'll be used to build the required AsnAny.  */
			CssmAutoData aData(alloc);
			
			if(sQualInfo->policyQualifierId == id_qt_cps) {
				/* build & encode an IA5String */
				IA5String *ia5 = new IA5String((char *)rawData, rawDataLen);
				SC_encodeAsnObj(*ia5, aData, MAX_IA5_NAME_SIZE);
				delete ia5;
			}
			else {
				/* copy over directly */
				aData.copy(rawData, rawDataLen);
			}
			
			/* install the result into CSM_Buffer, which mallocs & copies */
			sQualInfo->qualifier = new AsnAny;
			char *cp = aData;
			sQualInfo->qualifier->value = new CSM_Buffer(cp, aData.length());
			aData.reset();
		}	/* for each qualifier */
	}	/* for each policy */
	
	/* add to mExtensions */
	cert.addExtension(snaccObj, cssmExt->extnId, cssmExt->critical, false); 
}

bool getFieldCertPolicies(
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	DecodedExten *decodedExt;
	CertificatePoliciesSyntax *snaccObj;
	CE_CertPolicies *cdsaObj;
	bool brtn;
	CssmAllocator &alloc = fieldValue.allocator;
	brtn = GetFieldTop<CertificatePoliciesSyntax, CE_CertPolicies>(
		cert,
		index,
		numFields,
		fieldValue.allocator,
		id_ce_certificatePolicies,
		snaccObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}

	/* brute force CertificatePoliciesSyntax --> CE_CertPolicies */
	cdsaObj->numPolicies = snaccObj->Count();
	unsigned sz = cdsaObj->numPolicies * sizeof(CE_PolicyInformation);
	cdsaObj->policies = (CE_PolicyInformation *)alloc.malloc(sz);
	memset(cdsaObj->policies, 0, sz);
	snaccObj->SetCurrToFirst();
	for(unsigned polDex=0; polDex<cdsaObj->numPolicies; polDex++) {
		CE_PolicyInformation *cPolInfo = &cdsaObj->policies[polDex];
		PolicyInformation *sPolInfo = snaccObj->Curr();
		CssmOid &cOid = CssmOid::overlay(cPolInfo->certPolicyId);
		CL_snaccOidToCssm(sPolInfo->policyIdentifier, cOid, alloc);
		if(sPolInfo->policyQualifiers == NULL) {
			continue;
		}
		cPolInfo->numPolicyQualifiers = sPolInfo->policyQualifiers->Count();
		cPolInfo->policyQualifiers = (CE_PolicyQualifierInfo *)
			alloc.malloc(cPolInfo->numPolicyQualifiers *
				sizeof(CE_PolicyQualifierInfo));
		sPolInfo->policyQualifiers->SetCurrToFirst();
		for(unsigned qualDex=0; qualDex<cPolInfo->numPolicyQualifiers; qualDex++) {
			PolicyQualifierInfo *sQualInfo = sPolInfo->policyQualifiers->Curr();
			CE_PolicyQualifierInfo *cQualInfo = &cPolInfo->policyQualifiers[qualDex];
			
			/* 
			 * leaf. 
			 * policyQualifierId == id_qt_cps : IA5String - decode and return
			 * contents. Else return whole thing. 
			 */
			CssmOid &cOid2 = CssmOid::overlay(cQualInfo->policyQualifierId);
			CL_snaccOidToCssm(sQualInfo->policyQualifierId, cOid2, alloc);
			/* convert this: */
			CSM_Buffer *cbuf = sQualInfo->qualifier->value;
			/* to this */
			CssmRemoteData outData(alloc, cQualInfo->qualifier);
			if(sQualInfo->policyQualifierId == id_qt_cps) {
				IA5String ia5;
				CssmAutoData berData(alloc, cbuf->Access(), cbuf->Length());
				/* error is fatal, punt the whole kit'n'kaboodle and leak */
				SC_decodeAsnObj(berData, ia5);
				char *src = ia5;
				outData.copy(src, ia5.Len());
			}
			else {
				outData.copy(cbuf->Access(), cbuf->Length());
			}
			outData.release();
			sPolInfo->policyQualifiers->GoNext();
		}
		snaccObj->GoNext();
	}
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

void freeFieldCertPolicies (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	CssmAllocator &alloc = fieldValue.allocator;
	CE_CertPolicies *cdsaObj = (CE_CertPolicies *)cssmExt->value.parsedValue;
	for(unsigned polDex=0; polDex<cdsaObj->numPolicies; polDex++) {
		CE_PolicyInformation *cPolInfo = &cdsaObj->policies[polDex];
		alloc.free(cPolInfo->certPolicyId.Data);
		for(unsigned qualDex=0; qualDex<cPolInfo->numPolicyQualifiers; qualDex++) {
			CE_PolicyQualifierInfo *cQualInfo = &cPolInfo->policyQualifiers[qualDex];
			alloc.free(cQualInfo->policyQualifierId.Data);
			alloc.free(cQualInfo->qualifier.Data);
		}
		alloc.free(cPolInfo->policyQualifiers);
	}
	alloc.free(cdsaObj->policies);
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}

/***
 *** Netscape cert type
 *** CDSA Format:	CE_NetscapeCertType (a uint16)
 *** SNACC format:	AsnBits
 *** OID: 			CSSMOID_NetscapeCertType
 ***/
void setFieldNetscapeCertType(		
	DecodedCert	&cert, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	CE_NetscapeCertType *cdsaObj = (CE_NetscapeCertType *)cssmExt->value.parsedValue;
	
	char bits[sizeof(CE_NetscapeCertType)];
	bits[0] = static_cast<char>((*cdsaObj) >> 8);
	bits[1] = static_cast<char>(*cdsaObj);
	memmove(bits, cdsaObj, sizeof(CE_NetscapeCertType));
	AsnBits *snaccObj = new AsnBits(bits, sizeof(CE_NetscapeCertType) * 8);
	
	/* add to mExtensions */
	cert.addExtension(snaccObj, cssmExt->extnId, cssmExt->critical, false); 
}

bool getFieldNetscapeCertType(
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	DecodedExten *decodedExt;
	AsnBits *snaccObj;
	CE_NetscapeCertType *cdsaObj;
	bool brtn;
	
	brtn = GetFieldTop<AsnBits, CE_NetscapeCertType>(
		cert,
		index,
		numFields,
		fieldValue.allocator,
		id_netscape_cert_type,
		snaccObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}

	unsigned toCopy = (snaccObj->BitLen() + 7) / 8;
	if(toCopy > 2) {
		/* I hope I never see this... */
		errorLog0("getFieldNetscapeCertType: bitstring larger than 2 bytes!\n");
		toCopy = 2;
	}
	unsigned char bits[2] = {0, 0};
	memmove(bits, snaccObj->BitOcts(), toCopy);
	*cdsaObj = (((unsigned)bits[0]) << 8) | bits[1];
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

/***
 *** unknown extensions 
 *** CDSA format: raw bytes in a CSSM_DATA. This data is the BER-encoding of
 ***              some extension struct we don't know about.
 *** SNACC format AsnOcts
 *** OID 		  CSSMOID_X509V3CertificateExtensionCStruct
 ***/

void setFieldUnknownExt(
	DecodedCert	&cert, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, true);
	AsnOcts *snaccObj = new AsnOcts(
		reinterpret_cast<char *>(cssmExt->BERvalue.Data),
		cssmExt->BERvalue.Length);
	cert.addExtension(snaccObj, cssmExt->extnId, cssmExt->critical, true); 
}

bool getFieldUnknownExt(
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	AsnOid noOidLikeThis (1, 2);	// a dummy argument
	DecodedExten *decodedExt = cert.findDecodedExt(noOidLikeThis, 
		true, index, numFields);
	if(decodedExt == NULL) {
		return false;
	}
	/* failure of this dynamic_cast is fatal */
	AsnOcts *snaccObj = dynamic_cast<AsnOcts *>(decodedExt->snaccObj);  
	if(snaccObj == NULL) {
		errorLog0("getFieldUnknownExt: dynamic_cast failure\n");
		CssmError::throwMe(CSSMERR_CL_INTERNAL_ERROR);
	}
	char *octData = *snaccObj;
	CssmAutoData encodedBytes(fieldValue.allocator, octData, snaccObj->Len());
	/* easier way to do this...? */
	CssmData cData = encodedBytes.release();
	getFieldExtenCommon(&cData, *decodedExt, fieldValue);
	return true;
}

void freeFieldUnknownExt (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, true);
	CssmAllocator &alloc = fieldValue.allocator;
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}



#if template_pcode

/***
 *** Subject alternate name
 *** CDSA Format:	CE_GeneralNames
 *** SNACC format:	GeneralNames
 *** OID: 			CSSMOID_SubjectAltName
 ***/
void setFieldSomeExt(		
	DecodedCert	&cert, 
	const CssmData &fieldValue) 
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	new an approppriate AsnType snaccObj (KeyUsage, etc.);
	extension-specific e.g. CE_KeyUsage *cdsaObj = cssmExt->value.parsedValue;
	/* brute-force cdsaObj --> snaccObj */
	...
	/* add to mExtensions */
	cert.addExtension(snaccObj, cssmExt->extnId, cssmExt->critical, false); 
}

bool getFieldSomeExt(
	const DecodedCert 	&cert,
	unsigned			index,			// which occurrence (0 = first)
	uint32				&numFields,		// RETURNED
	CssmOwnedData		&fieldValue) 
{
	DecodedExten *decodedExt;
	e.g. ExtKeyUsageSyntax *snaccObj;
	e.g. CE_ExtendedKeyUsage *cdsaObj;
	bool brtn;
	
	brtn = GetFieldTop<ExtKeyUsageSyntax, CE_ExtendedKeyUsage>(
		cert,
		index,
		numFields,
		fieldValue.allocator,
		e.g. id_ce_extKeyUsage,
		snaccObj,
		cdsaObj,
		decodedExt);
	if(!brtn) {
		return false;
	}

	brute force snaccObj --> cdsaObj;
	getFieldExtenCommon(cdsaObj, *decodedExt, fieldValue);
	return true;
}

/* only used if complex structs below cssmExt->value.parsedValue */
void freeFieldSomeExt (
	CssmOwnedData		&fieldValue)
{
	CSSM_X509_EXTENSION_PTR cssmExt = verifySetFreeExtension(fieldValue, false);
	CssmAllocator &alloc = fieldValue.allocator;
	free the stuff in cssmExt->value.parsedValue;
	freeFieldExtenCommon(cssmExt, alloc);		// frees extnId, parsedValue, BERvalue
}

#endif
