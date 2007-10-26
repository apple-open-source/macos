/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
 * pkcs12Coder.cpp - Public P12Coder coder functions.
 */
 
#include "pkcs12Coder.h"
#include "pkcs12Debug.h"
#include "pkcs12Utils.h"
#include <Security/cssmerr.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <Security/oidsalg.h>

/*
 * Default encryption parameters
 */
#define P12_ENCR_ITER_COUNT		2048
#define P12_MAC_ITER_COUNT		1
#define P12_WEAK_ENCR_ALG		CSSMOID_PKCS12_pbewithSHAAnd40BitRC2CBC
#define P12_STRONG_ENCR_ALG		CSSMOID_PKCS12_pbeWithSHAAnd3Key3DESCBC

/* 
 * Default import flags. 
 */
#define P12_KC_IMPORT_DEFAULT	(kSecImportCertificates | \
								 kSecImportCRLs | \
								 kSecImportKeys)

P12Coder::P12Coder()
{
	init();
}

/* one-time init from all constructors */
void P12Coder::init()
{
	mPrivacyMode = kSecPkcs12ModePassword;
	mIntegrityMode = kSecPkcs12ModePassword;
	mMacPassphrase = NULL;
	mEncrPassPhrase = NULL;
	mMacPassData.Data = NULL;
	mMacPassData.Length = 0;
	mEncrPassData.Data = NULL;
	mEncrPassData.Length = 0;
	mMacPassKey = NULL;
	mEncrPassKey = NULL;
	mKeychain = NULL;
	mCspHand = 0;
	mDlDbHand.DLHandle = 0;
	mDlDbHand.DBHandle = 0;
	mPrivKeyImportState = PKIS_NoLimit;
	mWeakEncrAlg   = P12_WEAK_ENCR_ALG;
	mStrongEncrAlg = P12_STRONG_ENCR_ALG;
	mWeakEncrIterCount = P12_ENCR_ITER_COUNT;
	mStrongEncrIterCount = P12_ENCR_ITER_COUNT;
	mMacIterCount  = P12_MAC_ITER_COUNT;
	mImportFlags = P12_KC_IMPORT_DEFAULT;
	mRawCspHand = 0;
	mClHand = 0;
	mAccess = NULL;
	mNoAcl = false;
	mKeyUsage = CSSM_KEYUSE_ANY;		/* default */
	/* default key attrs; we add CSSM_KEYATTR_PERMANENT if importing to 
	 * a keychain */
	mKeyAttrs = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE | 
		CSSM_KEYATTR_SENSITIVE;
}

/* 
 * FIXME - can't we get vector's destructor to do this?
 */
#define deleteVect(v) 		\
{							\
	while(v.size()) {		\
		delete v[0];		\
		v.erase(v.begin());	\
	}						\
	v.clear();				\
}

P12Coder::~P12Coder()
{	
	if(mMacPassphrase) {
		CFRelease(mMacPassphrase);
	}
	if(mEncrPassPhrase) {
		CFRelease(mEncrPassPhrase);
	}
	if(mAccess) {
		CFRelease(mAccess);
	}
	deleteVect(mCerts);
	deleteVect(mCrls);
	deleteVect(mKeys);
	deleteVect(mOpaques);

	if(mKeychain) {
		CFRelease(mKeychain);
	}
	if(mRawCspHand) {
		cuCspDetachUnload(mRawCspHand, CSSM_TRUE);
	}
	if(mClHand) {
		cuClDetachUnload(mClHand);
	}
}

void P12Coder::setKeychain(
	SecKeychainRef			keychain)
{
	OSStatus ortn = SecKeychainGetCSPHandle(keychain, &mCspHand);
	if(ortn) {
		MacOSError::throwMe(ortn);
	}
	ortn = SecKeychainGetDLDBHandle(keychain, &mDlDbHand);
	if(ortn) {
		MacOSError::throwMe(ortn);
	}
	CFRetain(keychain);
	mKeychain = keychain;
}

void P12Coder::setAccess(
	SecAccessRef			access)
{
	if(mAccess) {
		CFRelease(mAccess);
	}
	mAccess = access;
	if(mAccess) {
		CFRetain(mAccess);
	}
	else {
		/* NULL ==> no ACL */
		mNoAcl = true;
	}
}

#define SEC_KEYATTR_RETURN_MASK		\
	(CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_RETURN_NONE)

void P12Coder::setKeyAttrs(
	CSSM_KEYATTR_FLAGS		keyAttrs)
{
	/* ensure we're generating a ref key no matter what caller asks for */
	mKeyAttrs = keyAttrs;
	mKeyAttrs &= ~SEC_KEYATTR_RETURN_MASK;
	mKeyAttrs |= CSSM_KEYATTR_RETURN_REF;
}

/*
 * Private methods for obtaining passprases in CSSM_DATA form.
 */
const CSSM_DATA *P12Coder::getMacPassPhrase()
{
	if(mMacPassData.Data != NULL) {
		return &mMacPassData;
	}
	else if (mMacPassphrase) {
		p12ImportPassPhrase(mMacPassphrase, mCoder, mMacPassData);
		return &mMacPassData;
	}
	else {
		return NULL;
	}
}

const CSSM_DATA *P12Coder::getEncrPassPhrase()
{
	if(mEncrPassData.Data != NULL) {
		return &mEncrPassData;
	}
	else if (mEncrPassPhrase) {
		p12ImportPassPhrase(mEncrPassPhrase, mCoder, mEncrPassData);
		return &mEncrPassData;
	}
	/* no separate passphrase found, use MAC passphrase */
	return getMacPassPhrase();
}

/*
 * These return a CSSM_KEY_PTR is the app had specified
 * PassKeys, else they return NULL.
 */
const CSSM_KEY *P12Coder::getMacPassKey()
{
	return mMacPassKey;
}

const CSSM_KEY *P12Coder::getEncrPassKey()
{
	if(mEncrPassKey != NULL) {
		return mEncrPassKey;
	}
	else {
		return getMacPassKey();
	}
}

/* 
 * Lazy evaluation of module handles. 
 */
CSSM_CSP_HANDLE P12Coder::rawCspHand()
{
	if(mRawCspHand != 0) {
		return mRawCspHand;
	}
	mRawCspHand = cuCspStartup(CSSM_TRUE);
	if(mRawCspHand == 0) {
		CssmError::throwMe(CSSMERR_CL_INTERNAL_ERROR);
	}
	return mRawCspHand;
}

CSSM_CL_HANDLE P12Coder::clHand()
{
	if(mClHand != 0) {
		return mClHand;
	}
	mClHand = cuClStartup();
	if(mClHand == 0) {
		CssmError::throwMe(CSSMERR_CL_INTERNAL_ERROR);
	}
	return mClHand;
}

/*
 * These public functions more or less correspond to 
 * the public functions in SecPkcs12.h.
 */
void P12Coder::setMacPassPhrase(
	CFStringRef				passphrase)
{
	CFRetain(passphrase);
	mMacPassphrase = passphrase;
}

void P12Coder::setEncrPassPhrase(
	CFStringRef				passphrase)
{
	CFRetain(passphrase);
	mEncrPassPhrase = passphrase;
}
	
void P12Coder::setMacPassKey(
	const CSSM_KEY  *passKey)
{
	mMacPassKey = passKey;
}

void P12Coder::setEncrPassKey(
	const CSSM_KEY  *passKey)
{
	mEncrPassKey = passKey;
}
	
/* getters */
unsigned P12Coder::numCerts()
{
	return mCerts.size();
}

unsigned P12Coder::numCrls()
{
	return mCrls.size();
}

unsigned P12Coder::numKeys()
{
	return mKeys.size();
}

unsigned P12Coder::numOpaqueBlobs()
{
	return mOpaques.size();
}

P12CertBag *P12Coder::getCert(
	unsigned				dex)
{
	if(mCerts.size() < (dex + 1)) {
		MacOSError::throwMe(paramErr);
	}
	return mCerts[dex];
}

P12CrlBag *P12Coder::getCrl(
	unsigned				dex)
{
	if(mCrls.size() < (dex + 1)) {
		MacOSError::throwMe(paramErr);
	}
	return mCrls[dex];
}

P12KeyBag *P12Coder::getKey(
	unsigned				dex)
{
	if(mKeys.size() < (dex + 1)) {
		MacOSError::throwMe(paramErr);
	}
	return mKeys[dex];
}

P12OpaqueBag *P12Coder::getOpaque(
	unsigned				dex)
{
	if(mOpaques.size() < (dex + 1)) {
		MacOSError::throwMe(paramErr);
	}
	return mOpaques[dex];
}
	
/*
 * These four "add" functions are invoked by the app prior to encoding
 * and by our decoder while decoding.
 */
void P12Coder::addCert(
	P12CertBag				*cert)
{
	mCerts.push_back(cert);
}

void P12Coder::addCrl(
	P12CrlBag				*crl)
{
	mCrls.push_back(crl);
}

void P12Coder::addKey(
	P12KeyBag				*key)
{
	mKeys.push_back(key);
}

void P12Coder::addOpaque(
	P12OpaqueBag			*opaque)
{
	mOpaques.push_back(opaque);
}

	
/* little known, but public,  functions */
void P12Coder::integrityMode(
	SecPkcs12Mode			mode)
{
	if(mode != kSecPkcs12ModePassword) {
		MacOSError::throwMe(paramErr);
	}
	mIntegrityMode = mode;
}

void P12Coder::privacyMode(
	SecPkcs12Mode			mode)
{
	if(mode != kSecPkcs12ModePassword) {
		MacOSError::throwMe(paramErr);
	}
	mPrivacyMode = mode;
}

CFDataRef P12Coder::weakEncrAlg()
{
	return p12CssmDataToCf(mWeakEncrAlg);
}

CFDataRef P12Coder::strongEncrAlg()
{
	return p12CssmDataToCf(mStrongEncrAlg);
}

void P12Coder::weakEncrAlg(
	CFDataRef				alg)
{
	p12CfDataToCssm(alg, mWeakEncrAlg, mCoder);
}

void P12Coder::strongEncrAlg(
	CFDataRef				alg)
{
	p12CfDataToCssm(alg, mStrongEncrAlg, mCoder);
}

void P12Coder::limitPrivKeyImport(
	bool foundOneKey)
{
	if(foundOneKey) {
		mPrivKeyImportState = PKIS_NoMore;
	}
	else {
		mPrivKeyImportState = PKIS_AllowOne;
	}
}
