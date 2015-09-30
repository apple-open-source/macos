/*
 * Copyright (c) 2003-2004,2011,2014 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * pkcs12SafeBag.h : internal representation of various kinds
 *                   of P12 SafeBags
 */
 
#ifndef	_PKCS12_SAFE_BAG_H_
#define _PKCS12_SAFE_BAG_H_

#include <Security/cssmtype.h>
#include <security_pkcs12/pkcs12BagAttrs.h>
#include <security_pkcs12/pkcs12Templates.h>
#include <security_asn1/SecNssCoder.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificate.h>

/*
 * Abstract superclass of all safe bags.
 */
class P12SafeBag {
public:
	/* 
	 * Constructors:
	 *
	 * While decoding, specified attr array includes friendlyName 
	 * and localKeyId; we'll parse the array and snag those if 
	 * present as "specially treated" attrs. The rest of the 
	 * attrs - which we don't grok - get piled into mBagAttrs.
	 */
	P12SafeBag(
		NSS_Attribute 		**attrs, // including friendlyName, etc.
		SecNssCoder 		&coder);
		
	/*
	 * Constructor for encoding. The specified attr array only contains 
	 * attrs other than friendlyName and localKeyId; those attrs
	 * are passed in explicitly. 
	 *
	 * All arguments except for the coder are optional.
	 */
	P12SafeBag(
		CFStringRef			fname,
		CFDataRef			keyId,
		P12BagAttrs			*otherAttrs,	
		SecNssCoder 		&coder);
		
	~P12SafeBag();
	
	/* getters in CF terms - result is created and returned */
	CFStringRef friendlyName();
	CFDataRef 	localKeyId();

	/* getters in CSSM_DATA terms - result is just a reference */
	CSSM_DATA 	&friendlyNameCssm()		{ return mFriendlyName; }
	CSSM_DATA	&localKeyIdCssm()		{ return mLocalKeyId; }
	
	/* 
	 * Get all attrs, including friendlyName and localKeyId,
	 * in preparation for encoding.
	 */
	NSS_Attribute **getAllAttrs();
	
	/* 
	 * Copy out all attrs in API form. All incoming ptrs
	 * are optional.
	 */
	void copyAllAttrs(
		CFStringRef 		*friendlyName,
		CFDataRef			*localKeyId,
		P12BagAttrs			**bagAttrs);
	
private:
	/* 
	 * Setters in CF terms, used when constructing prior
	 * to encoding.
	 */
	void friendlyName(
		CFStringRef 		fname);
	void localKeyId(
		CFDataRef 			keyId);
		
	/*
	 * Create an NSS_Attribute * for friendlyName or keyId
	 */
	NSS_Attribute *makeAttr(
		const CSSM_OID		&attrId,
		const CSSM_DATA		&attrValue);
		
protected:
	/* 
	 * The common contents of all safe bag types, all optional.
	 * FriendlyName is stored in P12-native form, just as it's
	 * stored in the PFX. This is the contents of a BMPString.
	 */
	CSSM_DATA				mFriendlyName;
	CSSM_DATA				mLocalKeyId;
	P12BagAttrs				mBagAttrs;
	SecNssCoder				&mCoder;	// all our data mallocd here
};

/*
 * Individual bag types 
 */
class P12CertBag : public P12SafeBag {
public:
	/* for decode */
	P12CertBag(
		NSS_P12_CertBagType	certType,	// CT_X509, CT_SDSI
		CSSM_DATA			&certData,
		NSS_Attribute		**attrs,	// optional
		SecNssCoder			&coder);
		
	/* for encode */
	P12CertBag(
		NSS_P12_CertBagType	certType,	// CT_X509, CT_SDSI
		CSSM_DATA			&certData,
		CFStringRef			fname,
		CFDataRef			keyId,
		P12BagAttrs			*otherAttrs,// optional
		SecNssCoder			&coder);
	
	~P12CertBag();
	
	/* getters - result is just a ref to our data */
	NSS_P12_CertBagType		certType() 	{ return mCertType; }
	CSSM_DATA				&certData()	{ return mCertData; }

	/* convert to P12CertBag to SecCertificateRef */
	SecCertificateRef 		getSecCert();

private:
	NSS_P12_CertBagType		mCertType;
	CSSM_DATA				mCertData;
	SecCertificateRef		mCertRef;	/* created lazily */
};

class P12CrlBag : public P12SafeBag {
public:
	/* decode */
	P12CrlBag(
		NSS_P12_CrlBagType	crlType,	// CRT_X509, only for now
		CSSM_DATA			&crlData,
		NSS_Attribute		**attrs,	// optional
		SecNssCoder			&coder);
		
	/* encode */
	P12CrlBag(
		NSS_P12_CrlBagType	crlType,	// CRT_X509, only for now
		CFDataRef			crlData,
		CFStringRef			fname,
		CFDataRef			keyId,
		P12BagAttrs			*otherAttrs,
		SecNssCoder			&coder);
	
	~P12CrlBag();

	/* getters - result is just a ref to our data */
	NSS_P12_CrlBagType		crlType() 	{ return mCrlType; }
	CSSM_DATA				&crlData()	{ return mCrlData; }

	
private:
	NSS_P12_CrlBagType		mCrlType;
	CSSM_DATA				mCrlData;
};

/* for both shrouded and plain */
class P12KeyBag : public P12SafeBag {
public:
	/* decode */
	P12KeyBag(
		CSSM_KEY_PTR		key,	
		CSSM_CSP_HANDLE		cspHand,
		NSS_Attribute		**attrs,	// optional
		CSSM_DATA			&labelData,
		SecNssCoder			&coder);
		
	/* encode */
	P12KeyBag(
		const CSSM_KEY		*key,	
		CSSM_CSP_HANDLE		cspHand,
		CFStringRef			fname,
		CFDataRef			keyId,
		P12BagAttrs			*otherAttrs,
		SecNssCoder			&coder,
		/* for SecPkcs12ExportKeychainItems() */
		SecKeyRef			keyRef = NULL);
	
	~P12KeyBag();
	
	void setLabel(
		const CSSM_DATA		&newLabel);
		
	/* reusable key setter */
	void setKey(
		CSSM_KEY_PTR		cssmKey);

	/*
	 * Keys are kind of special in that they can't easily be copied.
	 * On encode, the app owns the key. On decode, we own the 
	 * reference key but caller (P12Coder) owns the actual
	 * memory associated with the CSSM_KEY. 
	 */
	CSSM_KEY_PTR			key()				{ return mKey; }
	CSSM_DATA				&label()			{ return mLabel; }
	const CSSM_ACCESS_CREDENTIALS 
							*privKeyCreds()		{ return mPrivKeyCreds; }
	bool					dupKey()			{ return mDupKey; }
	void					dupKey(bool b)		{ mDupKey = b; }
	
private:
	CSSM_KEY_PTR			mKey;
	CSSM_CSP_HANDLE			mCspHand;
	
	/*
	 * When constructed via SecPkcs12ExportKeychainItems(), we
	 * have to hold a reference to the SecKeyRef which was
	 * used to create this object. 
	 */
	SecKeyRef				mKeyRef;
	
	/*
	 * When false, app owns key. When true, we have to free 
	 * key in our destructor.
	 *
	 * NOTE: mKeyRef and mWeOwnKey are mutually exclusive. 
	 */
	 bool					mWeOwnKey;
	 
	/* somewhat unique label when stored in DLDB */
	CSSM_DATA				mLabel;

	/* for encode only, owned by app */
	const CSSM_ACCESS_CREDENTIALS *mPrivKeyCreds;

	/* indicates a key we looked up, not imported */
	bool					mDupKey;
	
	void freeKey();

};

/*
 * Others we don't implement 
 */
class P12OpaqueBag : public P12SafeBag {
public:
	/* decode */
	P12OpaqueBag(
		const CSSM_OID		&oid,
		const CSSM_DATA		&blob,
		NSS_Attribute		**attrs,	// optional
		SecNssCoder			&coder);
		
	/* encode */
	P12OpaqueBag(
		CFDataRef			oid,
		CFDataRef			blob,
		CFStringRef			fname,
		CFDataRef			keyId,
		P12BagAttrs			*otherAttrs,
		SecNssCoder			&coder);
	
	~P12OpaqueBag();
	
	CSSM_OID 				&oid()		{ return mOid; }
	CSSM_DATA				&blob()		{ return mBlob; }
	
private:
	CSSM_OID				mOid;
	CSSM_DATA				mBlob;
};


#endif	/* _PKCS12_SAFE_BAG_H_ */

