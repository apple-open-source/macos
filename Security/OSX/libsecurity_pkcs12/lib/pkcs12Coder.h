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
 * pkcs12Coder.h - main PKCS12 encode/decode engine. This class
 *				   corresponds to a SecPkcs12CoderRef in the public API.
 */
 
#ifndef	_PKCS12_CODER_H_
#define _PKCS12_CODER_H_

#include <security_pkcs12/SecPkcs12.h>
#include <security_pkcs12/pkcs12SafeBag.h>
#include <vector>

/*
 * This class essentially consists of the following:
 *
 * -- bags of certs, CRLs, keys, and opaque blobs in the 
 *    form of P12SafeBag subclasses.
 *
 * -- getters and setters to let the app access those bags.
 *
 * -- a decode routine which takes an encoded PFX, rips it apart,
 *    and drops what it finds into the bags of cert, CRLS, etc.
 *
 * -- an encode routine which takes the contents of the bag and 
 *    creates an encoded PFX.
 *
 * Everything else is ephemera supporting the above four things.
 *
 * Memory usage:
 * -------------
 *
 * Memory allocation is done in three flavors:
 *
 * 1. Via CF objects, when exporting contents of bags to the app.
 * 2. Via our own member variable SecNssCoder. This is used to allocate
 *    the contents of the bags (certs, etc.) which persist across
 *    API calls. 
 * 3. "Local" SecNssCoders for encode and decode. Allocated in the 
 *    stack for those two exported functions and used for all decoding
 *    and encoding. 
 *
 * The contents of the bags persist as long as an instance of 
 * P12Coder does. 
 *
 * CF objects the app gives us (e..g, passphrases)
 * are retained when get them and released in our destructor.
 *
 * CF objects we give the app are, of course, the app's responsibility. 
 *
 * Everything else is allocd locally via the SecNssCoders in encode()
 * and decode() and has a scope not exceeding those functions. 
 */
 
class P12Coder {
public:
	/* only constructor */
	P12Coder();	
	~P12Coder();
	
	/*
	 * These public functions more or less correspond to 
	 * the public functions in SecPkcs12.h.
	 */
	void setMacPassPhrase(
		CFStringRef				passphrase);
	void setEncrPassPhrase(
		CFStringRef				passphrase);
	void setMacPassKey(
		const CSSM_KEY			*passKey);
	void setEncrPassKey(
		const CSSM_KEY			*passKey);
		
	/* main decode function */
	void decode(
		CFDataRef				pfx);
	
	/* 
	 * CSP and DLDB associated with keys.
	 * CSP handle is required, DLDB optional.
	*/
	void setKeychain(
		SecKeychainRef			keychain);
	void setCsp(
		CSSM_CSP_HANDLE			cspHand)	{ mCspHand = cspHand; }
	void setDlDb(
		CSSM_DL_DB_HANDLE		dlDbHand)	{ mDlDbHand = dlDbHand; }
	
	CSSM_CSP_HANDLE	cspHand()				{ return mCspHand; }

	/* private key import attributes */
	void setAccess(
		SecAccessRef			access);
	void setKeyUsage(
		CSSM_KEYUSE				keyUsage)	{ mKeyUsage = keyUsage; }
	void setKeyAttrs(
		CSSM_KEYATTR_FLAGS		keyAttrs);
		
	/* High-level import/export */
	void importFlags(
		SecPkcs12ImportFlags flags)			{ mImportFlags = flags; }
	SecPkcs12ImportFlags importFlags()	
											{ return mImportFlags; }

	void exportKeychainItems(
		CFArrayRef				items);
		
	/* getters */
	unsigned numCerts();
	unsigned numCrls();
	unsigned numKeys();
	unsigned numOpaqueBlobs();
	
	P12CertBag *getCert(
		unsigned				dex);
	P12CrlBag *getCrl(
		unsigned				dex);
	P12KeyBag *getKey(
		unsigned				dex);
	P12OpaqueBag *getOpaque(
		unsigned				dex);
		
	/* encoding */
	void encode(
		CFDataRef				*pfx);		// RETURNED
		
	void addCert(
		P12CertBag				*cert);
	void addCrl(
		P12CrlBag				*crl);
	void addKey(
		P12KeyBag				*key);
	void addOpaque(
		P12OpaqueBag			*opaque);
		
	/* little known, but public,  functions */
	SecPkcs12Mode integrityMode()		{ return mIntegrityMode; }
	SecPkcs12Mode privacyMode()			{ return mPrivacyMode; }
	void integrityMode(
		SecPkcs12Mode			mode);
	void privacyMode(
		SecPkcs12Mode			mode);
		
	/*
	 * Public API calls use our coder to create P12SafeBags
	 */
	SecNssCoder	&coder()			{ return mCoder; }
	
	/* encryption parameter getters/setters */
	unsigned weakEncrIterCount()	{ return mWeakEncrIterCount; }
	unsigned strongEncrIterCount()	{ return mStrongEncrIterCount; }
	unsigned macEncrIterCount()		{ return mMacIterCount; }
	
	void weakEncrIterCount(
		unsigned ic)				{ mWeakEncrIterCount = ic; }
	void strongEncrIterCount(
		unsigned ic)				{ mStrongEncrIterCount = ic; }
	void macEncrIterCount(
		unsigned ic)				{ mMacIterCount = ic; }

	CFDataRef weakEncrAlg();
	CFDataRef strongEncrAlg();
	
	void weakEncrAlg(
		CFDataRef				alg);
	void strongEncrAlg(
		CFDataRef				alg);
		
	/* panic button, delete anything stored in a DB during decode */
	void deleteDecodedItems();
	
	void limitPrivKeyImport(
		bool foundOneKey);
	
private:
	void init();					// one-time init from all constructors
	
	/* 
	 * Passphrase handling.
	 *
	 * These two convert the app-supplied CFStringRefs into
	 * CSSM_DATAs; if PassKeys are used, these just NULL out 
	 * the returned data. 
	 */
	const CSSM_DATA *getMacPassPhrase();
	const CSSM_DATA *getEncrPassPhrase();

	/*
	 * These return a CSSM_KEY_PTR is the app had specified
	 * PassKeys, else they return NULL.
	 */
	const CSSM_KEY *getMacPassKey();
	const CSSM_KEY *getEncrPassKey();
	
	/* in pkcs12Keychain.cpp */
	void storeDecodeResults();
	void setPrivateKeyHashes();
	void notifyKeyImport();
	P12CertBag *findCertForKey(
		P12KeyBag 				*keyBag);

	void addSecKey(
		SecKeyRef				keyRef);
	void addSecCert(
		SecCertificateRef		certRef);

	/* Lazy evaluation of module handles. */
	CSSM_CSP_HANDLE rawCspHand();
	CSSM_CL_HANDLE 	clHand();

	/* 
	 * A bunch of private encode/decode methods. This makes me
	 * long for ObjC-style categories so these would not 
	 * have to appear in this file.
	 *
	 * See implementation for comments and descriptions. 
	 */
	void encryptedDataDecrypt(
		const NSS_P7_EncryptedData 	&edata,
		SecNssCoder 				&localCdr,
		NSS_P12_PBE_Params 			*pbep,
		CSSM_DATA 					&ptext);
		
	void algIdParse(
		const CSSM_X509_ALGORITHM_IDENTIFIER &algId,
		NSS_P12_PBE_Params 			*pbeParams,	
		SecNssCoder 				&localCdr);
 
	void encryptedDataParse(
		const NSS_P7_EncryptedData 	&edata,
		SecNssCoder 				&localCdr,
		NSS_P12_PBE_Params 			*pbep);

	void shroudedKeyBagParse(
		const NSS_P12_SafeBag 		&safeBag,
		SecNssCoder 				&localCdr);
		
	void keyBagParse(
		const NSS_P12_SafeBag 		&safeBag,
		SecNssCoder 				&localCdr);
		
	void certBagParse(
		const NSS_P12_SafeBag 		&safeBag,
		SecNssCoder 				&localCdr);
		
	void crlBagParse(
		const NSS_P12_SafeBag 		&safeBag,
		SecNssCoder 				&localCdr);
		
	void secretBagParse(
		const NSS_P12_SafeBag 		&safeBag,
		SecNssCoder 				&localCdr);
		
	void safeContentsBagParse(
		const NSS_P12_SafeBag 		&safeBag,
		SecNssCoder 				&localCdr);
		
	void safeContentsParse(
		const CSSM_DATA 			&contentsBlob,
		SecNssCoder 				&localCdr);

	void authSafeElementParse(
		const NSS_P7_DecodedContentInfo *info,
		SecNssCoder 				&localCdr);
		
	void macParse(
		const NSS_P12_MacData 		&macData, 
		SecNssCoder					&localCdr);

	void authSafeParse(
		const CSSM_DATA 			&authSafeBlob,
		SecNssCoder 				&localCdr);

	/* private encoding routines */
	NSS_P7_DecodedContentInfo *safeContentsBuild(
		NSS_P12_SafeBag				**bags,
		NSS_P7_CI_Type 				type,		// CT_Data, CT_EncryptedData
		CSSM_OID 					*encrOid,	// only for CT_EncryptedData
		unsigned 					iterCount,	// ditto
		SecNssCoder					&localCdr);

	void authSafeBuild(
		NSS_P7_DecodedContentInfo 	&authSafe,
		SecNssCoder 				&localCdr);

	void encryptData(
		const CSSM_DATA				&ptext,
		CSSM_OID 					&encrOid,
		unsigned 					iterCount,
		NSS_P7_EncryptedData		&ed,
		SecNssCoder					&localCdr);

	void algIdBuild(
		CSSM_X509_ALGORITHM_IDENTIFIER	&algId,
		const CSSM_OID 				&algOid,
		const CSSM_DATA 			&salt,
		unsigned 					iterCount,
		SecNssCoder 				&localCdr);
		
	void macSignPfx(
		NSS_P12_DecodedPFX 			&pfx,
		SecNssCoder 				&localCdr);
		
	NSS_P12_SafeBag *certBagBuild(
		P12CertBag 					*cert,
		SecNssCoder 				&localCdr);

	NSS_P12_SafeBag *crlBagBuild(
		P12CrlBag 					*crl,
		SecNssCoder 				&localCdr);

	NSS_P12_SafeBag *keyBagBuild(
		P12KeyBag 					*key,
		SecNssCoder 				&localCdr);

	NSS_P12_SafeBag *opaqueBagBuild(
		P12OpaqueBag				*op,
		SecNssCoder 				&localCdr);

	/* member variables */
	SecPkcs12Mode				mPrivacyMode;
	SecPkcs12Mode				mIntegrityMode;
	
	/* passwords - as app gave us, and translated into ready-to-use
	 * unicode strings */
	CFStringRef					mMacPassphrase;
	CFStringRef					mEncrPassPhrase;
	CSSM_DATA					mMacPassData;
	CSSM_DATA					mEncrPassData;
	
	/* passphrases in key form */
	const CSSM_KEY				*mMacPassKey;
	const CSSM_KEY				*mEncrPassKey;
	
	/*
	 * App has to either set mKeychain or mCspHand. In the former
	 * case we infer both mCspHand and mDlDbHand from the keychainRef.
	 */
	SecKeychainRef				mKeychain;
	CSSM_CSP_HANDLE				mCspHand;
	CSSM_DL_DB_HANDLE			mDlDbHand;
	
	/*
	 * LimitPrivateKeyImport mechanism
	 */
	typedef enum {
		PKIS_NoLimit,			// no limit
		PKIS_AllowOne,			// allow import of at most one
		PKIS_NoMore				// found one, no more allowed
	} p12PrivKeyImportState;
	
	p12PrivKeyImportState		mPrivKeyImportState;
	
	/*
	 * Encryption/MAC parameters
	 */
	CSSM_OID					mWeakEncrAlg;		// for certs and CRLs
	CSSM_OID					mStrongEncrAlg;
	unsigned					mWeakEncrIterCount;
	unsigned					mStrongEncrIterCount;
	unsigned					mMacIterCount;

	/*
	 * Import flags
	 */
	SecPkcs12ImportFlags		mImportFlags;
	
	/* 
	 * Four individual piles of safe bags
	 */
	vector<P12CertBag *>		mCerts;
	vector<P12CrlBag *>			mCrls;
	vector<P12KeyBag *>			mKeys;
	vector<P12OpaqueBag *>		mOpaques;
	
	/*
	 * Internal CSSM module handles, lazily evaluated.
	 */
	CSSM_CSP_HANDLE				mRawCspHand;
	CSSM_CL_HANDLE				mClHand;
	
	/* 
	 * Imported private key attributes
	 */
	SecAccessRef				mAccess;
	bool						mNoAcl;		/* true when NULL passed to setAccess() */
	CSSM_KEYUSE					mKeyUsage;
	CSSM_KEYATTR_FLAGS			mKeyAttrs;
	
	/*
	 * The source of most (all?) of our privately allocated data
	 */
	SecNssCoder					mCoder;
};

#endif	/* _PKCS12_CODER_H_ */

