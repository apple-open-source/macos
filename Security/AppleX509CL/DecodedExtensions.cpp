/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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
 * DecodedItem.cpp - class representing the common portions of 
 * NSS-format decoded certs and CRLs, with extensions parsed and 
 * decoded (still in NSS format).
 */

#include "DecodedItem.h"
#include "cldebugging.h"
#include "AppleX509CLSession.h" 
#include "CSPAttacher.h"
#include "CLFieldsCommon.h"
#include "clNssUtils.h"
#include "clNameUtils.h"
#include <Security/cssmapple.h>
#include <Security/oidscert.h>

#define MIN_EXTENSIONS		4		// initial size of *mExtensions

DecodedExten::DecodedExten(
	const CSSM_OID 	&extnId,	// copied
	bool			critical,
	void			*nssObj,	// NSS_KeyUsage, NSS_BasicConstraints, 
								//   etc. NOT COPIED, exists in same
								//   memory space as coder
	bool			berEncoded,	// indicates unknown extension which we
								// do not BER-decode when parsing a cert
	const SEC_ASN1Template *templ,	// to decode/encode if !berEncoded
	SecNssCoder		&coder,			// all local allocs from here
	const CSSM_DATA	*rawExtn)	// NSS_CertExtension.value, copied to
								//   mRawExtn
	: mCritical(critical),
	  mNssObj(nssObj),
	  mBerEncoded(berEncoded),
	  mTempl(templ),
	  mCoder(coder),
	  mRawExtn(NULL)
{
	coder.allocCopyItem(extnId, mExtnId);
	if(rawExtn) {
		mRawExtn = (CSSM_DATA *)coder.malloc(sizeof(CSSM_DATA));
		coder.allocCopyItem(*rawExtn, *mRawExtn);
	}
}

DecodedExten::~DecodedExten()
{
	/* the only stuff we allocated was in the coder pool and will be freed
	 * when coder is freed */
}

/*
 * Convert this extension to a CSSM_X509_EXTENSION, in the specified
 * (app-level) alloc space, after its contents have
 * been converted to a native CDSA object (CE_KeyUsage, etc.).
 */
void DecodedExten::convertToCdsa(
	void 					*cdsaObj,		// e.g. CE_KeyUsage
											// CSSM_DATA_PTR for berEncoded
	CSSM_X509_EXTENSION_PTR	cssmExt,		// contents RETURNED
	CssmAllocator			&alloc) const
{
	clAllocCopyData(alloc, mExtnId, cssmExt->extnId);
	cssmExt->critical = mCritical ? CSSM_TRUE : CSSM_FALSE;
	
	/* 
	 * in either case copy the raw extension data if we have it (we may not
	 * have it if this was created via setField). 
	 */
	if(mRawExtn) {
		clAllocCopyData(alloc, *mRawExtn, cssmExt->BERvalue);
	}
	else {
		cssmExt->BERvalue.Data = NULL;
		cssmExt->BERvalue.Length = 0;
	}
	if(mBerEncoded) {
		/* an extension we never parsed or understood */
		assert(cdsaObj == NULL);
		cssmExt->format = CSSM_X509_DATAFORMAT_ENCODED;
		cssmExt->value.parsedValue = NULL;
	}
	else {
		/* caller sees parsed version plus raw BER-encoded bytes */
		assert(cdsaObj != NULL);
		if(mRawExtn) {
			/* normal decode & parse case */
			cssmExt->format = CSSM_X509_DATAFORMAT_PAIR;
		}
		else {
			/* setField */
			cssmExt->format = CSSM_X509_DATAFORMAT_PARSED;
		}
		/* in app alloc's space, mallocd by getField*() */
		cssmExt->value.parsedValue = cdsaObj;
	}
}

/*
 * Convert a DecodedExten to a CSSM_X509_EXTENSION. This includes
 * the mapping of the extnId to a known CDSA type and type and doing the 
 * actual NSS-to-CDSA conversion. At the time this function is 
 * called, the DecodedExten either has a valid mNssObj, or it's an 
 * unknown extension type in which case mNssObj is an AsnOcts containing
 * the opaquely DER-encoded extension value. 
 * 
 * Currently only used when decoding a CRL and converting it en masse
 * to CDSA.
 */
template<class NssType, class CdsaType>
void nssToCssm(
	const DecodedExten	&decodedExt,
	NssType				*&nssObj,		// RETURNED
	CdsaType			*&cdsaObj,		// mallocd and RETURNED
	CssmAllocator		&alloc)
{
	nssObj = (NssType *)(decodedExt.nssObj());  
	assert(nssObj != NULL);
	cdsaObj = (CdsaType *)alloc.malloc(sizeof(CdsaType));
	memset(cdsaObj, 0, sizeof(CdsaType));
}

void DecodedExten::parse(
	CSSM_X509_EXTENSION_PTR	cssmExt,		// mallocd by caller, RETURNED
	CssmAllocator			&alloc) const
{
	void *vCdsaObj = NULL;
	if(mBerEncoded) {
		/* non-understood extension */
		convertToCdsa(NULL, cssmExt, alloc);
		return;
	}
	if(clCompareCssmData(&mExtnId, &CSSMOID_AuthorityKeyIdentifier)) {
		CE_AuthorityKeyID *cdsaObj;
		NSS_AuthorityKeyId *nssObj;
		nssToCssm<NSS_AuthorityKeyId, CE_AuthorityKeyID>(
			*this,
			nssObj,
			cdsaObj,
			alloc);
		CL_nssAuthorityKeyIdToCssm(*nssObj, *cdsaObj, mCoder, alloc);
		vCdsaObj = cdsaObj;
	}
	/* same encoding (uint32) for all of these: */
	else if(clCompareCssmData(&mExtnId, &CSSMOID_CrlNumber) ||
	        clCompareCssmData(&mExtnId, &CSSMOID_DeltaCrlIndicator) ||
			clCompareCssmData(&mExtnId, &CSSMOID_CrlReason)) {
		CE_CrlNumber *cdsaObj;
		CSSM_DATA *nssObj;
		nssToCssm<CSSM_DATA, CE_CrlNumber>(
			*this,
			nssObj,
			cdsaObj,
			alloc);
		*cdsaObj = clDataToInt(*nssObj);
		vCdsaObj = cdsaObj;
	}
	/* same encoding (GeneralNames) for all of these: */
	else if(clCompareCssmData(&mExtnId, &CSSMOID_IssuerAltName) ||
	   clCompareCssmData(&mExtnId, &CSSMOID_SubjectAltName) ||
	   clCompareCssmData(&mExtnId, &CSSMOID_CertIssuer)) {
		CE_GeneralNames *cdsaObj;
		NSS_GeneralNames *nssObj;
		nssToCssm<NSS_GeneralNames, CE_GeneralNames>(
			*this,
			nssObj,
			cdsaObj,
			alloc);
		CL_nssGeneralNamesToCssm(*nssObj, *cdsaObj, mCoder, alloc);
		vCdsaObj = cdsaObj;
	}
	else if(clCompareCssmData(&mExtnId, &CSSMOID_IssuingDistributionPoint)) {
		CE_IssuingDistributionPoint *cdsaObj;
		NSS_IssuingDistributionPoint *nssObj;
		nssToCssm<NSS_IssuingDistributionPoint, CE_IssuingDistributionPoint>(
			*this,
			nssObj,
			cdsaObj,
			alloc);
		CL_nssIssuingDistPointToCssm(nssObj, cdsaObj, mCoder, alloc);
		vCdsaObj = cdsaObj;
	}
	
	/*
	 * cert entry extensions 
	 */
	else if(clCompareCssmData(&mExtnId, &CSSMOID_HoldInstructionCode)) {
		/* value is just an OID */
		CSSM_OID *cdsaObj;
		CSSM_DATA *nssObj;
		nssToCssm<CSSM_DATA, CSSM_OID>(
			*this,
			nssObj,
			cdsaObj,
			alloc);
		clAllocCopyData(alloc, *nssObj, *cdsaObj);
		vCdsaObj = cdsaObj;
	}
	else if(clCompareCssmData(&mExtnId, &CSSMOID_InvalidityDate)) {
		/* GeneralizedTime */
		CSSM_DATA *cdsaObj;
		CSSM_DATA *nssObj;
		nssToCssm<CSSM_DATA, CSSM_DATA>(
			*this,
			nssObj,
			cdsaObj,
			alloc);
		clAllocCopyData(alloc, *nssObj, *cdsaObj);
		vCdsaObj = cdsaObj;
	}
	else {
		/* if we get here, this routine is not keeping up with 
		 * clOidToNssInfo()  */
		assert(0);
		CssmError::throwMe(CSSMERR_CL_INTERNAL_ERROR);
	}
	convertToCdsa(vCdsaObj, cssmExt, alloc);
}


#pragma mark ------ DecodedExtensions ------

/*
 * A variable-size array of DecodedExtens.
 * Used for storing cert and CRL extensions as well as per-CRL-entry 
 * extensions.
 */
DecodedExtensions::DecodedExtensions(	
	SecNssCoder		&coder,
	CssmAllocator	&alloc)
	:	mCoder(coder),
		mAlloc(alloc),
		mExtensions(NULL),
		mNumExtensions(0),
		mSizeofExtensions(0)
{

}
	
DecodedExtensions::~DecodedExtensions()
{
	for(unsigned i=0; i<mNumExtensions; i++) {
		assert(mExtensions[i] != NULL);
		delete mExtensions[i];
	}
	mAlloc.free(mExtensions);
	mExtensions = NULL;
	mNumExtensions = 0;
	mSizeofExtensions = 0;
}


/*
 * Initialize by decoding a NSS-style NSS_CertExtension array.
 * This involves figuring out what kind of object is represented in the 
 * octet string in the  extension, decoding it, and appending the resulting 
 * NSS object to mExtensions in the form of a DecodedExten.
 *
 * Called when decoding either a cert or a CRL (for caching it or 
 * getting its fields) or a cert template (only via 
 * CertGetAllTemplateFields()).
 */
void DecodedExtensions::decodeFromNss(
	NSS_CertExtension 	**extensions)	
{
	if(extensions == NULL) {
		/* OK, no extensions present */
		return;
	}
	unsigned numExtens = clNssArraySize((const void **)extensions);

	/* traverse extension list */
	for(unsigned dex=0; dex<numExtens; dex++) {
		NSS_CertExtension *nssExten = extensions[dex];
		
		/*
		 * For this extension->extnId, cook up an approppriate 
		 * NSS-specific type (NSS_KeyUsage, etc.);
		 */
		CSSM_DATA &rawExtn = nssExten->value;
		bool berEncoded = false;
		bool found;								// we understand this OID
		unsigned nssObjLen;						// size of associated NSS object
		const SEC_ASN1Template *templ = NULL;	// template for decoding
		void *nssObj = NULL;					// decode destination
		found = clOidToNssInfo(nssExten->extnId, nssObjLen, templ);
		if(!found) {
			/* 
			 * We don't know how to deal with this.
			 */
			berEncoded = true;
		}
		else {
			/* 
			 * Create NSS-style object specific to this extension, just
			 * by knowing its length and ASN template. 
			 * Decode the extensions's extnValue into that object. We don't
			 * have to know what kind of object it is anymore. 
			 */
			assert(templ != NULL);
			nssObj = mCoder.malloc(nssObjLen);
			memset(nssObj, 0, nssObjLen);
			PRErrorCode prtn;
			prtn = mCoder.decodeItem(rawExtn, templ, nssObj); 
			if(prtn) {
				/* 
				 * FIXME - what do we do here? For now flag it 
				 * as an non-understood extension...
				 */
				clErrorLog("decodeExtensions: extension decode error\n");
				nssObj = NULL;
				berEncoded = true;
			}
		}	
		if((nssObj != NULL) || berEncoded) {
			/* append if the decode was successful */
			addExtension(nssExten->extnId,
				clNssBoolToCssm(nssExten->critical),
				nssObj, 
				berEncoded,
				templ,
				&rawExtn);
		}
	}
}

/*
 * Encode into a NSS-style Extensions.
 *
 * Each extension object, currently stored as some AsnType subclass,
 * is BER-encoded and the result is stored as an octet string
 * (AsnOcts) in a new Extension object in the TBS.
 *
 * Called from {Crl,Cert}CreateTemplate via encode{Tbs,Cts}(). 
 */
void DecodedExtensions::encodeToNss(
	NSS_CertExtension 	**&extensions)
{
	assert(extensions == NULL);

	if(mNumExtensions == 0) {
		/* no extensions, no error */
		return;
	}
	
	/* malloc a NULL_terminated array of NSS_CertExtension pointers */
	unsigned len = (mNumExtensions + 1) * sizeof(NSS_CertExtension *);
	extensions = (NSS_CertExtension **)mCoder.malloc(len);
	memset(extensions, 0, len);
	
	/* grind thru our DecodedExtens, creating an NSS_CertExtension for 
	 * each one */
	for(unsigned extenDex=0; extenDex<mNumExtensions; extenDex++) {
		NSS_CertExtension *thisNssExten = 
			(NSS_CertExtension *)mCoder.malloc(sizeof(NSS_CertExtension));
		memset(thisNssExten, 0, sizeof(NSS_CertExtension));
		extensions[extenDex] = thisNssExten;
		
		const DecodedExten *decodedExt = getExtension(extenDex);
		
		/* BER-encode the extension object if appropriate */
		if(decodedExt->berEncoded()) {
			/* unknown extension type, it's already encoded */
			const CSSM_DATA *srcBer = (const CSSM_DATA *)decodedExt->rawExtn();
			assert(srcBer != NULL);
			mCoder.allocCopyItem(*srcBer, thisNssExten->value);
		}
		else {
			PRErrorCode prtn;
			prtn = mCoder.encodeItem(decodedExt->nssObj(),
				decodedExt->templ(), thisNssExten->value);
			if(prtn) {
				clErrorLog("encodeToNss: extension encode error");
				CssmError::throwMe(CSSMERR_CL_INTERNAL_ERROR);
			}
		}
		ArenaAllocator arenaAlloc(mCoder);
		if(decodedExt->critical()) {
			/* optional, default false */
			clCssmBoolToNss(CSSM_TRUE, thisNssExten->critical, arenaAlloc);
		}
		mCoder.allocCopyItem(decodedExt->extnId(), thisNssExten->extnId);
	}
}

/* add/retrieve entries */
void DecodedExtensions::addExtension(
	const CSSM_OID 	&extnId,	// copied
	bool			critical,
	void			*nssObj,	// NSS_KeyUsage, NSS_BasicConstraints, 
								//   etc. NOT COPIED, exists in same
								//   memory space as coder
	bool			berEncoded,	// indicates unknown extension which we
								// do not BER-decode when parsing a cert
	const SEC_ASN1Template *templ, // required if !berEncoded
	const CSSM_DATA	*rawExtn) 	// NSS_CertExtension.value, copied,
								//   optional (not present during a 
								//   SetField op)
{
	if(mNumExtensions == mSizeofExtensions) {
		/* expand by doubling, or initial malloc */
		mSizeofExtensions = mNumExtensions ? 
			(2 * mNumExtensions) : MIN_EXTENSIONS;
		mExtensions = (DecodedExten **)mAlloc.realloc(
			mExtensions, mSizeofExtensions * sizeof(DecodedExten));
	}
	mExtensions[mNumExtensions++] = new DecodedExten(extnId,
		critical, nssObj, berEncoded, templ, mCoder, rawExtn);
}
	
const DecodedExten *DecodedExtensions::getExtension(
	unsigned extenDex) const
{
	assert(extenDex < mNumExtensions);
	return mExtensions[extenDex];
}

/* Convert to CSSM_X509_EXTENSIONS */
/* Currently only used when decoding a CRL and converting it en masse
 * to CDSA */
void DecodedExtensions::convertToCdsa(
	CSSM_X509_EXTENSIONS		&cssmExtens,
	CssmAllocator				&alloc) const
{
	memset(&cssmExtens, 0, sizeof(cssmExtens));
	if(mNumExtensions == NULL) {
		return;
	}
	cssmExtens.extensions = (CSSM_X509_EXTENSION_PTR)alloc.malloc(
		sizeof(CSSM_X509_EXTENSION) * mNumExtensions);
	memset(cssmExtens.extensions, 0, 
		sizeof(CSSM_X509_EXTENSION) * mNumExtensions);
	cssmExtens.numberOfExtensions = mNumExtensions;
	for(unsigned dex=0; dex<mNumExtensions; dex++) {
		try {
			getExtension(dex)->parse(&cssmExtens.extensions[dex], alloc);
		}
		catch(...) {
			/* FIXME - what now? */
			clFieldLog("DecodedExtensions:convertToCdsa: extension "
				"decode error");
		}
	}
}

