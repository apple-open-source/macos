/*
 * Copyright (c) 2000-2002,2011,2014 Apple Inc. All Rights Reserved.
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
 * DecodedExtensions.h - Intermediate representation of extensions. 
 */

#ifndef	_DECODED_EXTENSIONS_H_
#define _DECODED_EXTENSIONS_H_

#include <Security/cssmtype.h>
#include <security_cdsa_utilities/cssmdata.h>

#include <security_asn1/SecNssCoder.h>
#include <Security/X509Templates.h>

#include "cldebugging.h"

class DecodedExten 
{
	NOCOPY(DecodedExten)
public:
	
	/* member variables only set on construct */
	DecodedExten(
		const CSSM_OID 	&extnId,		// copied
		bool			critical,
		void			*nssObj,		// NSS_KeyUsage, NSS_BasicConstraints, 
										//   etc. NOT COPIED, exists in same
										//   memory space as coder
		bool			berEncoded,		// indicates unknown extension 
										//   which we do not BER-decode 
										//   when parsing a cert
		const SecAsn1Template *templ,	// to decode/encode if !berEncoded
		SecNssCoder		&coder,			// all local allocs from here
		const CSSM_DATA	*rawExtn=NULL);	// NSS_CertExtension.value, copied to
										//   mRawExtn, optional (not present 
										//   during a SetField op)
	~DecodedExten();
	
	/*
	 * Convert this extension to a CSSM_X509_EXTENSION, after its contents 
	 * have been converted to a native CDSA object (CE_KeyUsage, etc.).
	 * This is the final step of a getField op involving an extension.
	 */
	void convertToCdsa(
		void 					*cdsaObj,	// e.g. CE_KeyUsage
											// CSSM_DATA_PTR for berEncoded
		CSSM_X509_EXTENSION_PTR	cssmExt,	// contents mallocd in alloc's space
											//   and RETURNED		
		Allocator			&alloc) const;
	
	/*
 	 * Convert a DecodedExten to a CSSM_X509_EXTENSION. This includes
	 * the mapping of the extnId to a known CDSA type and type and doing the 
	 * actual NSS-to-CDSA conversion. 	
	 */
	void parse( 
		CSSM_X509_EXTENSION_PTR	cssmExt,	// mallocd by caller, contents
											// mallocd in alloc's space, 
											// RETURNED
		Allocator			&alloc) const;

	const CSSM_OID	&extnId() const			{ return mExtnId; }
	bool		critical() const 			{ return mCritical; }
	void		*nssObj() const 			{ return mNssObj; }
	bool		berEncoded() const			{ return mBerEncoded; }
	const SecAsn1Template *templ()	const	{ return mTempl; }
	CSSM_DATA	*rawExtn() const			{ return mRawExtn; }
private:			
	CSSM_X509EXT_TAGandVALUE *createTagAndValue(
		const CSSM_DATA		&berValue,
		Allocator			&alloc) const;
		
	CSSM_OID	mExtnId;
	bool		mCritical;
	void		*mNssObj;		// NSS_KeyUsage, NSS_BasicConstraints, etc.
	bool		mBerEncoded;	// indicates unknown extension which we
								// do not BER-decode when parsing a cert
	const SecAsn1Template *mTempl;
								// used for decode/encode
	SecNssCoder	&mCoder;
	CSSM_DATA	*mRawExtn;		// optional BER-encoded extension data, not
								// present if created via SetField()
};

/*
 * A variable-size array of DecodedExtens.
 * Used for storing cert and CRL extensions as well as per-CRL-entry 
 * extensions.
 */
class DecodedExtensions 
{
	NOCOPY(DecodedExtensions)
public:
	DecodedExtensions(	
		SecNssCoder		&coder,				// for local allocs
		Allocator	&alloc);			// for user space (copyout) allocs
	~DecodedExtensions();

	/* Initialize by decoding an NSS-style NSS_CertExtension array */
	void decodeFromNss(
		NSS_CertExtension 	**extensions);	
		
	/* Encode into a NSS-style NSS_CertExtension array, allocating 
	 * both the array pointers and the NSS_Extension elements themselves */
	void encodeToNss(
		NSS_CertExtension 	**&extensions);
		
	/* add/retrieve entries */
	void addExtension(
		const CSSM_OID 	&extnId,		// copied
		bool			critical,
		void			*nssObj,		// NSS_BasicConstraints, 
										//   etc. NOT COPIED, exists in same
										//   memory space as coder
		bool			berEncoded,		// indicates unknown extension 
										//    which we do not BER-decode 
										//    when parsing a cert
		const SecAsn1Template *templ, 	// required if !berEncoded
		const CSSM_DATA	*rawExtn=NULL);	// NSS_CertExtension.value, copied,
										//   optional (not present during a 
										//   SetField op)
	const DecodedExten *getExtension(
		unsigned 		extenDex) const;
		
	/* 
	 * Convert to CSSM_X509_EXTENSIONS, including conversion to 
	 * native CDSA C structs. Only used on CRL side, on a getField
	 * returning the entire parsed CRL. */
	void convertToCdsa(
		CSSM_X509_EXTENSIONS		&cssmExtens,
		Allocator				&alloc) const;
		
	unsigned numExtensions() const 		{ return mNumExtensions; }
	
private:
	SecNssCoder			&mCoder;
	Allocator		&mAlloc;
	DecodedExten		**mExtensions;
	unsigned			mNumExtensions;		// # valid DecodedExtens
	unsigned			mSizeofExtensions;	// mallocd size in mExtensions
};

#endif	/* _DECODED_EXTENSIONS_H_ */
