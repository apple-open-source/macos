/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */
 
/*
 * pkcs12Coder.cpp - Public P12Coder coder functions.
 * 
 * Created 3/5/03 by Doug Mitchell. 
 */
 
#include "pkcs12Coder.h"
#include "pkcs12Debug.h"
#include "pkcs12Utils.h"
#include <Security/utilities.h>		// private API for CssmError
#include <Security/cssmerr.h>
#include <Security/cuCdsaUtils.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <Security/oidsalg.h>

/*
 * Default encryption parameters
 */
#define P12_ENCR_ITER_COUNT		2048
#define P12_MAC_ITER_COUNT		1
#define P12_WEAK_ENCR_ALG		CSSMOID_PKCS12_pbeWithSHAAnd40BitRC4
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
	mKeychain = NULL;
	mCspHand = 0;
	mDlDbHand.DLHandle = 0;
	mDlDbHand.DBHandle = 0;
	mWeakEncrAlg   = CSSMOID_PKCS12_pbeWithSHAAnd40BitRC4;
	mStrongEncrAlg = CSSMOID_PKCS12_pbeWithSHAAnd3Key3DESCBC;
	mWeakEncrIterCount = P12_ENCR_ITER_COUNT;
	mStrongEncrIterCount = P12_ENCR_ITER_COUNT;
	mMacIterCount  = P12_MAC_ITER_COUNT;
	mImportFlags = P12_KC_IMPORT_DEFAULT;
	mRawCspHand = 0;
	mClHand = 0;
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
	deleteVect(mCerts);
	deleteVect(mCrls);
	deleteVect(mKeys);
	deleteVect(mOpaques);

	if(mKeychain) {
		CFRelease(mKeychain);
	}
	if(mRawCspHand) {
		CSSM_ModuleDetach(mRawCspHand);
	}
	if(mClHand) {
		CSSM_ModuleDetach(mClHand);
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

/*
 * Private methods for obtaining passprases in CSSM_DATA form.
 */
CSSM_DATA &P12Coder::getMacPhrase()
{
	if(mMacPassData.Data != NULL) {
		return mMacPassData;
	}
	else if (mMacPassphrase) {
		importPassPhrase(mMacPassphrase, mMacPassData);
		return mMacPassData;
	}
	else {
		/* any other options, callback, etc. for getting this? */
		p12ErrorLog("no passphrase set\n");
		CssmError::throwMe(CSSMERR_CSP_MISSING_ATTR_PASSPHRASE);
	}
}

CSSM_DATA &P12Coder::getEncrPassPhrase()
{
	if(mEncrPassData.Data != NULL) {
		return mEncrPassData;
	}
	else if (mEncrPassPhrase) {
		importPassPhrase(mEncrPassPhrase, mEncrPassData);
		return mEncrPassData;
	}
	/* no separate passphrase found, use MAC passphrase */
	return getMacPhrase();
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

/* convert App passphrase to array of chars used in P12 PBE */
void P12Coder::importPassPhrase(
	CFStringRef				inPhrase,
	CSSM_DATA				&outPhrase)
{
	CFIndex len = CFStringGetLength(inPhrase);
	unsigned pwdLen = (len * sizeof(UniChar)) + 2;
	mCoder.allocItem(outPhrase, pwdLen);
	unsigned char *cp = outPhrase.Data;
	for(CFIndex dex=0; dex<len; dex++) {
		UniChar uc = CFStringGetCharacterAtIndex(inPhrase, dex);
		*cp++ = uc >> 8;
		*cp++ = uc & 0xff;
	}
	*cp++ = 0;
	*cp++ = 0;
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

