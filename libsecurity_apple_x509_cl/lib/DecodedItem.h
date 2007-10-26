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
 * DecodedItem.h - class representing the common portions of NSS-format
 * decoded certs and CRLs, with extensions parsed and decoded (still in
 * NSS format).
 *
 * When a DecodedItem (cert or CRL) is quiescent and cached in the CL 
 * (either by an explicit cache call like CSSM_CL_CertCache or 
 * CSSM_CL_CrlCache(), or during a succession of GetFirst/GetNext field
 * ops), the item is stored in the CL in what we call NSS form. NSS is
 * the module we use to perform DER encoding and decoding; NSS form 
 * refers to structs defining Certs, CRLs, and extensions which are
 * directly encodable and decodable by the NSS library. NSS structs are
 * similar to their CDSA counterparts, sometimes identical, usually
 * subtly different (due to requirements of the NSS module). 
 *
 * Decoding a cert or a CRL:
 * -------------------------
 *
 * When an app decodes a cert or CRL for any reason, the following phases
 * are executed:
 *
 * PHASE I
 * -------
 *
 * Basic BER-decode if the incoming CSSM_DATA blob. This happens in the 
 * constructors for DecodedCert and DecodedCrl. A modified/restricted 
 * version of this occurs in DecodedCert::decodeTbs(), which is used 
 * during a CSSM_CL_CertGetAllTemplateFields() call. 
 *
 * PHASE II
 * --------
 *
 * Extensions are converted from untyped blobs - which is how they look
 * after PHASE I - to NSS-style C structs. This is done by examining
 * the ExtnId of each cert's or CRL's extensions and doing a BER decode
 * specific to that extension type. This is performed in 
 * DecodedExtensions.decodeFromNss() which is called immediately after
 * the top-level decode performed in PHASE I. 
 *
 * It is at this point that a cert or CRL can be cached in the CL's 
 * cacheMap or queryMap (see AppleX509CLSession.{h,cpp}. We call this 
 * state "NSS Form". 
 *
 * PHASE III (CRLs only)
 * --------------------
 *
 * This occurs when an app is actually fetching a full CRL in 
 * CDSA form. Individual entries in a CRL's revocation list also 
 * contain per-entry extension lists. These are converted from 
 * untyped blobs to meaningful NSS-style extension structs as 
 * in PHASE II prior to the conversion to CDSA form in PHASE IV.
 * 
 * PHASE IV
 * ---------
 *
 * This occurs when an app is actually fetching fields in CDSA form. 
 * This involves converting objects from NSS form to CDSA form 
 * (if necessary) and copying to the session allocator's memory space. 
 *
 * The rationale behind this phased approach - in particular, the 
 * reason that in-memory items are stored in NSS form - is that this
 * minimizes the number of copies between the intiial parse of a cert 
 * or CRL and the final GetField op. Since a GetField op inherently 
 * requires a copy (from internal memory to the session allocator's 
 * space), and conversion from NSS to CDSA form is basically a bunch of 
 * copies as well, we might as well just stop with the item in CRL
 * format as soon as PHASE II is complete. Note that completion of 
 * PHASE II is in fact required before caching a cert since that enables
 * us to have access to extension-specific info while a cert is 
 * cached. The KeyUsage and ExtendedKeyUsage extensions are used in 
 * this manner to get key info from a TBS cert. 
 *
 * 
 * Creating and encoding a cert:
 * -----------------------------
 *
 * Creating a cert (creating CRLs is not supported in this release) 
 * follows more or less the reverse procedure, as follows:
 *
 * PHASE I
 * -------
 *
 * During a CSSM_CL_CertCreateTemplate() op, all fields which the 
 * app wishes to specify are passed into the CL in CDSA form. These
 * fields are converted to NSS form in a temporary DecodedCert. This
 * includes extensions (in NSS form). 
 *
 * PHASE II
 * --------
 * 
 * Extensions in NSS form are encoded and bundled up into the final, 
 * BER-encode ready NSS_CertExtension array form. This occurs 
 * in DecodedCert::encodeExtensions(), called from the top of 
 * DecodedCert::encodeTbs(). We're still processing an app's
 * CSSM_CL_CertCreateTemplate() call at this point. 
 *
 * PHASE III
 * ---------
 *
 * Final DER-encoding of a TBS cert is performed in 
 * DecodedCert::encodeTbs(). The resulting CSSM_DATA is 
 * passed back to the app as what CDSA calls a template. 
 * This completes the CSSM_CL_CertCreateTemplate() call. 
 *
 * PHASE IV
 * --------
 *
 * The TBS cert blob is signed and the resulting DER-encoded
 * cert is passed back to the app. 
 */

#ifndef	_DECODED_ITEM_H_
#define _DECODED_ITEM_H_

#include <Security/cssmtype.h>
#include <security_cdsa_utilities/cssmdata.h>

#include "cldebugging.h"
#include "DecodedExtensions.h"
#include <security_asn1/SecNssCoder.h>

/* state of a DecodedItem */
typedef enum {
	IS_Empty,
	IS_DecodedAll,		// can't set fields in this state
	IS_DecodedTBS,		// ditto
	IS_Building			// in the process of setting fields
} ItemState;


class AppleX509CLSession;

class DecodedItem
{
public:
	DecodedItem(
		AppleX509CLSession	&session);	

	virtual ~DecodedItem();
	
	SecNssCoder &coder() { return mCoder; }
	
	static void describeFormat(
		Allocator 		&alloc,
		uint32 				&NumberOfFields,
		CSSM_OID_PTR 		&OidList);

public:
	/***
	 *** Extensions support
	 ***/
	 
	/* called from decodeExtensions and setField* */
	void addExtension(
		void 				*nssThing,	// e.g. NSS_KeyUsage
		const CSSM_OID		&extnId,		
		bool				critical,
		bool				berEncoded,
		const SecAsn1Template *templ,		// to decode/encode if !berEncoded
		const CSSM_DATA		*rawExtn=NULL)	// Extension.extnValue, copied, only for
											//   setField*()
			{ mDecodedExtensions.addExtension(extnId, critical, nssThing,
				berEncoded, templ, rawExtn);
			}

	const DecodedExten *findDecodedExt(
		const CSSM_OID		&extnId,		// for known extensions
		bool				unknown,		// otherwise		
		uint32				index, 
		uint32				&numFields) const;

	const DecodedExtensions		&decodedExtens() const 
		{ return mDecodedExtensions; }
	
	/* 
	 * Common code for get extension field routines. 
	 * Given an OID identifying an extension and an index, see if 
	 * we have the specified extension in mDecodedExtensions and
	 * return the NSS and CDSA style objects as well as the 
	 * DecodedExten.
	 */
	template<class NssType, class CdsaType>
	bool GetExtenTop(
		unsigned			index,			// which occurrence (0 = first)
		uint32				&numFields,		// RETURNED
		Allocator			&alloc,
		const CSSM_OID		&fieldId,		// identifies extension we seek
		NssType				*&nssObj,		// RETURNED
		CdsaType			*&cdsaObj,		// mallocd and RETURNED
		const DecodedExten	*&decodedExt) const	// RETURNED
	{
		/* See if we have one of these in our list of DecodedExtens */
		decodedExt = findDecodedExt(fieldId, false, index, numFields);
		if(decodedExt == NULL) {
			return false;
		}
		nssObj = (NssType *)decodedExt->nssObj();  
		cdsaObj = (CdsaType *)alloc.malloc(sizeof(CdsaType));
		memset(cdsaObj, 0, sizeof(CdsaType));
		return true;
	}

protected:
	ItemState			mState;
	Allocator		&mAlloc;
	SecNssCoder			mCoder;			// from which all local allocs come
	AppleX509CLSession	&mSession;
	DecodedExtensions	mDecodedExtensions;
	
};


#endif	/* _DECODED_ITEM_H_ */
