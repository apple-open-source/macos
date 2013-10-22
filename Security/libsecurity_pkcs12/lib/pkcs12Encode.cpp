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
 * pkcs12Encode.h - P12Coder encoding engine.
 * 
 * Unlike the decoding side of P12Coder, which can parse PFXs with
 * more or less arbitrary structures, this encoding engine has 
 * a specific layout for what bags go where in the jungle of 
 * SafeContents and ContentInfos. It would be impractical to allow
 * (or to expect) the app to specify this structure. 
 *
 * The knowledge of how a PFX is built out of various components
 * is encapsulated in the authSafeBuild() member function. The rest
 * of the functions in this file are pretty much "PFX-structure-
 * agnostic", so if one wanted to change the overall PFX structure,
 * one would only have to focus on the authSafeBuild() function. 
 */

#include "pkcs12Coder.h"
#include "pkcs12Debug.h"
#include "pkcs12Crypto.h"
#include "pkcs12Templates.h"
#include "pkcs12Utils.h"
#include <Security/cssmerr.h>
#include <Security/oidsattr.h>
#include <Security/SecBase.h>

void P12Coder::encode(
	CFDataRef				*cpfx)		// RETURNED
{
	p12EncodeLog("encode top");
	SecNssCoder localCdr;
	NSS_P12_DecodedPFX pfx;
	
	memset(&pfx, 0, sizeof(pfx));
	p12IntToData(3, pfx.version, localCdr);
	authSafeBuild(pfx.authSafe, localCdr);
	macSignPfx(pfx, localCdr);
	CSSM_DATA derPfx = {0, NULL};
	if(localCdr.encodeItem(&pfx, NSS_P12_DecodedPFXTemplate, derPfx)) {
		p12ErrorLog("Error encoding top-level pfx\n");
		P12_THROW_ENCODE;
	}
	CFDataRef cp = CFDataCreate(NULL, derPfx.Data, derPfx.Length);
	*cpfx = cp;
}

void P12Coder::macSignPfx(
	NSS_P12_DecodedPFX &pfx,
	SecNssCoder &localCdr)
{
	p12EncodeLog("macSignPfx");
	NSS_P12_MacData *macData = localCdr.mallocn<NSS_P12_MacData>();
	pfx.macData = macData;
	p12GenSalt(macData->macSalt, localCdr);
	p12IntToData(mMacIterCount, macData->iterations, localCdr);
	NSS_P7_DigestInfo &digInfo = macData->mac;
	
	/* this is not negotiable; it's the only one P12 allows */
	localCdr.allocCopyItem(CSSMOID_SHA1, digInfo.digestAlgorithm.algorithm);
	/* null algorithm parameters */
	p12NullAlgParams(digInfo.digestAlgorithm);
	
	const CSSM_DATA *macPhrase = getMacPassPhrase();
	const CSSM_KEY *macPassKey = getMacPassKey();
	if((macPhrase == NULL) && (macPassKey == NULL)) {
		p12ErrorLog("no passphrase set\n");
		CssmError::throwMe(CSSMERR_CSP_MISSING_ATTR_PASSPHRASE);
	}
	
	CSSM_RETURN crtn = p12GenMac(mCspHand, 
		*pfx.authSafe.content.data, 
		CSSM_ALGID_SHA1, mMacIterCount, macData->macSalt, 
		macPhrase, macPassKey, localCdr, digInfo.digest);
	if(crtn) {
		p12ErrorLog("Error generating PFX MAC\n");
		CssmError::throwMe(crtn);
	}
}

/*
 * This is the heart of the encoding engine. All knowledge of 
 * "what bags go where" is here. The PFX structure implemented here
 * is derived from empirical observation of PFXs obtained from
 * Mozilla 1.2b and from the DoD test vectors for "Conformance 
 * Testing of Relying Party Client Certificate Path Processing 
 * Logic", written by Cygnacom, Septemtber 28, 2001. 
 *
 * The PFX structure is as follows:
 *
 * -- One AuthenticatedSafe element (a PKCS7 ContentInfo) containing
 *    all certificates and CRLs. 
 *
 *    ContentInfo.type = CT_EncryptedData
 *    Encryption algorithm is our "weak" encryption Alg, default 
 *        of CSSMOID_PKCS12_pbeWithSHAAnd40BitRC4.
 *
 * -- One AuthenticatedSafe element containing all private keys in
 *    the form of ShroudedKeyBags.
 *
 *    ContentInfo.type = CT_Data
 *    Encryption algorithm for shrouded key bags is our "strong"
 *        encryption, default CSSMOID_PKCS12_pbeWithSHAAnd3Key3DESCBC
 *
 * -- Everything else goes in another AuthenticatedSafe element.
 *
 *    ContentInfo.type = CT_EncryptedData
 *    Encryption algorithm is our "strong" encryption Alg
 */
void P12Coder::authSafeBuild(
	NSS_P7_DecodedContentInfo &authSafe,
	SecNssCoder &localCdr)
{
	p12EncodeLog("authSafeBuild top");

	/* how many contentInfos are we going to build? */
	unsigned numContents = 0;
	if(mCerts.size() || mCrls.size()) {
		numContents++;
	}
	if(mKeys.size()) {
		numContents++;
	}
	if(mOpaques.size()) {
		numContents++;
	}

	if(numContents == 0) {
		p12ErrorLog("authSafeBuild: no contents\n");
		MacOSError::throwMe(errSecParam);
	}
	
	NSS_P7_DecodedContentInfo **contents = 
		(NSS_P7_DecodedContentInfo **)p12NssNullArray(numContents, 
			localCdr);
	unsigned contentDex = 0;
	
	NSS_P12_SafeBag **safeBags;

	/* certs & crls */
	unsigned numBags = (unsigned)(mCerts.size() + mCrls.size());
	p12EncodeLog("authSafeBuild : %u certs + CRLS", numBags);
	if(numBags) {
		safeBags = (NSS_P12_SafeBag **)p12NssNullArray(numBags, localCdr);
		unsigned bagDex = 0;
		for(unsigned dex=0; dex<mCerts.size(); dex++) {
			safeBags[bagDex++] = certBagBuild(mCerts[dex], localCdr);
		} 
		for(unsigned dex=0; dex<mCrls.size(); dex++) {
			safeBags[bagDex++] = crlBagBuild(mCrls[dex], localCdr);
		} 
		contents[contentDex++] = safeContentsBuild(safeBags, 
			CT_EncryptedData, &mWeakEncrAlg, mWeakEncrIterCount, localCdr);
	}
	
	/* shrouded keys - encrypted at bag level */
	numBags = (unsigned)mKeys.size();
	if(numBags) {
		p12EncodeLog("authSafeBuild : %u keys", numBags);
		safeBags = (NSS_P12_SafeBag **)p12NssNullArray(numBags, localCdr);
		unsigned bagDex = 0;
		for(unsigned dex=0; dex<numBags; dex++) {
			safeBags[bagDex++] = keyBagBuild(mKeys[dex], localCdr);
		} 
		contents[contentDex++] = safeContentsBuild(safeBags, 
			CT_Data, NULL, 0, localCdr);
	}
	
	/* opaque */
	numBags = (unsigned)mOpaques.size();
	if(numBags) {
		p12EncodeLog("authSafeBuild : %u opaques", numBags);
		safeBags = (NSS_P12_SafeBag **)p12NssNullArray(numBags, localCdr);
		unsigned bagDex = 0;
		for(unsigned dex=0; dex<numBags; dex++) {
			safeBags[bagDex++] = opaqueBagBuild(mOpaques[dex], localCdr);
		} 
		contents[contentDex++] = safeContentsBuild(safeBags, 
			CT_EncryptedData, &mStrongEncrAlg, mStrongEncrIterCount, localCdr);
	}
	
	/*
	 * Encode the whole elements array into authSafe.content.data
	 */
	NSS_P12_AuthenticatedSafe safe;
	safe.info = contents;
	CSSM_DATA *adata = localCdr.mallocn<CSSM_DATA>();
	authSafe.content.data = adata;
	adata->Data = NULL;
	adata->Length = 0;
	if(localCdr.encodeItem(&safe, NSS_P12_AuthenticatedSafeTemplate,
			*adata)) {
		p12ErrorLog("authSafeBuild: error encoding auth safe\n");
		P12_THROW_ENCODE;
	}
	authSafe.type = CT_Data;
	authSafe.contentType = CSSMOID_PKCS7_Data;
}

/*
 * Build a AuthSafe element of specified type out of the 
 * specified array of bags.
 */
NSS_P7_DecodedContentInfo *P12Coder::safeContentsBuild(
	NSS_P12_SafeBag **bags,
	NSS_P7_CI_Type type,	// CT_Data, CT_EncryptedData
	CSSM_OID *encrOid,		// only for CT_EncryptedData
	unsigned iterCount,		// ditto
	SecNssCoder &localCdr)
{
	p12EncodeLog("safeContentsBuild type %u", (unsigned)type);

	/*
	 * First, encode the bag array as a SafeContents
	 */
	CSSM_DATA encSafeContents = {0, NULL};
	NSS_P12_SafeContents safeContents = {bags};
	if(localCdr.encodeItem(&safeContents,
			NSS_P12_SafeContentsTemplate, encSafeContents)) {
		p12ErrorLog("error encoding SafeContents\n");
		P12_THROW_ENCODE;
	}
	
	NSS_P7_DecodedContentInfo *dci = 
		localCdr.mallocn<NSS_P7_DecodedContentInfo>();
	dci->type = type;
	if(type == CT_Data) {
		/* plaintext gets encoded as an octet string */
		localCdr.allocCopyItem(CSSMOID_PKCS7_Data, dci->contentType);
		dci->content.data = localCdr.mallocn<CSSM_DATA>();
		localCdr.allocCopyItem(encSafeContents, *dci->content.data);
	}
	else if(type == CT_EncryptedData) {
		/* encrypt the encoded SafeContents */
		localCdr.allocCopyItem(CSSMOID_PKCS7_EncryptedData, 
			dci->contentType);
		dci->content.encryptData = localCdr.mallocn<NSS_P7_EncryptedData>();
		NSS_P7_EncryptedData *ed = dci->content.encryptData;
		assert(encrOid != NULL);
		encryptData(encSafeContents, *encrOid, iterCount, *ed, localCdr);
	}
	else {
		p12ErrorLog("bad type in safeContentsBuild\n");
		CssmError::throwMe(CSSMERR_CL_INTERNAL_ERROR);
	}
	return dci;
}

/*
 * Encrypt the specified plaintext with specified algorithm.
 * Drop result and other interesting info into an NSS_P7_EncryptedData.
 */
void P12Coder::encryptData(
	const CSSM_DATA			&ptext,
	CSSM_OID 				&encrOid,
	unsigned 				iterCount,
	NSS_P7_EncryptedData	&ed,
	SecNssCoder				&localCdr)
{
	p12EncodeLog("encryptData");

	/* do the raw encrypt first to make sure we can do it... */
	CSSM_ALGORITHMS		keyAlg;			// e.g., CSSM_ALGID_DES
	CSSM_ALGORITHMS		encrAlg;		// e.g., CSSM_ALGID_3DES_3KEY_EDE
	CSSM_ALGORITHMS		pbeHashAlg;		// SHA1 or MD5
	uint32				keySizeInBits;
	uint32				blockSizeInBytes;	// for IV, optional
	CSSM_PADDING		padding;		// CSSM_PADDING_PKCS7, etc.
	CSSM_ENCRYPT_MODE	mode;			// CSSM_ALGMODE_CBCPadIV8, etc.
	PKCS_Which			pkcs;
	
	bool found = pkcsOidToParams(&encrOid,
		keyAlg, encrAlg, pbeHashAlg, keySizeInBits, blockSizeInBytes,
		padding, mode, pkcs);
	if(!found || (pkcs != PW_PKCS12)) {
		p12ErrorLog("encryptData encrAlg not understood\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
	
	/* Salt: we generate random bytes */
	CSSM_DATA salt;
	p12GenSalt(salt, localCdr);

	const CSSM_DATA *pwd = getEncrPassPhrase();
	const CSSM_KEY *passKey = getEncrPassKey();
	if((pwd == NULL) && (passKey == NULL)) {
		p12ErrorLog("no passphrase set\n");
		CssmError::throwMe(CSSMERR_CSP_MISSING_ATTR_PASSPHRASE);
	}
	CSSM_DATA ctext = {0, NULL};
	
	CSSM_RETURN crtn = p12Encrypt(mCspHand, ptext, 
		keyAlg, encrAlg, pbeHashAlg,
		keySizeInBits, blockSizeInBytes,
		padding, mode,
		iterCount, salt,
		pwd, passKey, localCdr,
		ctext);
	if(crtn) {
		CssmError::throwMe(crtn);
	}
	
	/* Now fill in the NSS_P7_EncryptedData */
	p12IntToData(0, ed.version, localCdr);
	NSS_P7_EncrContentInfo &eci = ed.contentInfo;
	localCdr.allocCopyItem(CSSMOID_PKCS7_Data, eci.contentType);
	algIdBuild(eci.encrAlg, encrOid, salt, iterCount, localCdr);
	eci.encrContent = ctext;
}

/* 
 * Fill in an CSSM_X509_ALGORITHM_IDENTIFIER with parameters in
 * the form of an encoded NSS_P12_PBE_Params
 */
void P12Coder::algIdBuild(
	CSSM_X509_ALGORITHM_IDENTIFIER	&algId,
	const CSSM_OID &algOid,
	const CSSM_DATA &salt,
	unsigned iterCount,
	SecNssCoder &localCdr)
{
	p12EncodeLog("algIdBuild");
	localCdr.allocCopyItem(algOid, algId.algorithm);
	NSS_P12_PBE_Params pbeParams;
	pbeParams.salt = salt;
	p12IntToData(iterCount, pbeParams.iterations, localCdr);
	if(localCdr.encodeItem(&pbeParams, NSS_P12_PBE_ParamsTemplate,
			algId.parameters)) {
		p12ErrorLog("error encoding NSS_P12_PBE_Params\n");
		P12_THROW_ENCODE;
	}
}

#pragma mark --- Individual Bag Builders ---

NSS_P12_SafeBag *P12Coder::certBagBuild(
	P12CertBag *cert,
	SecNssCoder &localCdr)
{
	p12EncodeLog("certBagBuild");

	NSS_P12_SafeBag *safeBag = localCdr.mallocn<NSS_P12_SafeBag>();
	safeBag->bagId = CSSMOID_PKCS12_certBag;
	safeBag->type = BT_CertBag;
	
	NSS_P12_CertBag *certBag = localCdr.mallocn<NSS_P12_CertBag>();
	safeBag->bagValue.certBag = certBag;
	const CSSM_OID *certTypeOid = NULL;
	switch(cert->certType()) {
		case CT_X509:
			certTypeOid = &CSSMOID_PKCS9_X509Certificate;
			break;
		case CT_SDSI:
			certTypeOid = &CSSMOID_PKCS9_SdsiCertificate;
			break;
		default:
			p12ErrorLog("unknown certType on encode\n");
			CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
			
	}
	
	/* copies not needed, same scope as P12CertBag */
	certBag->bagType = *certTypeOid;
	certBag->type = cert->certType();
	certBag->certValue = cert->certData();
	safeBag->bagAttrs = cert->getAllAttrs();
	return safeBag;
}

NSS_P12_SafeBag *P12Coder::crlBagBuild(
	P12CrlBag *crl,
	SecNssCoder &localCdr)
{
	p12EncodeLog("crlBagBuild");

	NSS_P12_SafeBag *safeBag = localCdr.mallocn<NSS_P12_SafeBag>();
	safeBag->bagId = CSSMOID_PKCS12_crlBag;
	safeBag->type = BT_CrlBag;
	
	NSS_P12_CrlBag *crlBag = localCdr.mallocn<NSS_P12_CrlBag>();
	safeBag->bagValue.crlBag = crlBag;
	const CSSM_OID *crlTypeOid = NULL;
	switch(crl->crlType()) {
		case CRT_X509:
			crlTypeOid = &CSSMOID_PKCS9_X509Crl;
			break;
		default:
			p12ErrorLog("unknown crlType on encode\n");
			CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
			
	}
	
	/* copies not needed, same scope as P12CrlBag */
	crlBag->bagType = *crlTypeOid;
	crlBag->type = crl->crlType();
	crlBag->crlValue = crl->crlData();
	safeBag->bagAttrs = crl->getAllAttrs();
	return safeBag;
}

NSS_P12_SafeBag *P12Coder::keyBagBuild(
	P12KeyBag *key,
	SecNssCoder &localCdr)
{
	p12EncodeLog("keyBagBuild");

	NSS_P12_SafeBag *safeBag = localCdr.mallocn<NSS_P12_SafeBag>();
	safeBag->bagId = CSSMOID_PKCS12_shroudedKeyBag;
	safeBag->type = BT_ShroudedKeyBag;
	
	NSS_EncryptedPrivateKeyInfo *keyInfo = localCdr.
		mallocn<NSS_EncryptedPrivateKeyInfo>();
	safeBag->bagValue.shroudedKeyBag = keyInfo;
	safeBag->bagAttrs = key->getAllAttrs();

	/* Prepare for key wrap */
	CSSM_DATA salt;
	p12GenSalt(salt, localCdr);
	algIdBuild(keyInfo->algorithm, mStrongEncrAlg, salt, 
		mStrongEncrIterCount, localCdr);
	
	CSSM_ALGORITHMS		keyAlg;			// e.g., CSSM_ALGID_DES
	CSSM_ALGORITHMS		encrAlg;		// e.g., CSSM_ALGID_3DES_3KEY_EDE
	CSSM_ALGORITHMS		pbeHashAlg;		// SHA1 or MD5
	uint32				keySizeInBits;
	uint32				blockSizeInBytes;	// for IV, optional
	CSSM_PADDING		padding;		// CSSM_PADDING_PKCS7, etc.
	CSSM_ENCRYPT_MODE	mode;			// CSSM_ALGMODE_CBCPadIV8, etc.
	PKCS_Which			pkcs;
	
	bool found = pkcsOidToParams(&mStrongEncrAlg,
		keyAlg, encrAlg, pbeHashAlg, keySizeInBits, blockSizeInBytes,
		padding, mode, pkcs);
	if(!found || (pkcs != PW_PKCS12)) {
		/* app config error - they gave us bogus algorithm */
		p12ErrorLog("keyBagBuild encrAlg not understood\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
	const CSSM_DATA *encrPhrase = getEncrPassPhrase();
	const CSSM_KEY *passKey = getEncrPassKey();
	if((encrPhrase == NULL) && (passKey == NULL)) {
		p12ErrorLog("no passphrase set\n");
		CssmError::throwMe(CSSMERR_CSP_MISSING_ATTR_PASSPHRASE);
	}
	CSSM_DATA shroudedBits = {0, NULL};
	
	CSSM_RETURN crtn = p12WrapKey(mCspHand,
		key->key(), key->privKeyCreds(),
		keyAlg, encrAlg, pbeHashAlg,
		keySizeInBits, blockSizeInBytes,
		padding, mode,
		mStrongEncrIterCount, salt,
		encrPhrase,
		passKey,
		localCdr, 
		shroudedBits);
	if(crtn) {
		p12ErrorLog("Error wrapping private key\n");
		CssmError::throwMe(crtn);
	}
	
	keyInfo->encryptedData = shroudedBits;
	return safeBag;
}

NSS_P12_SafeBag *P12Coder::opaqueBagBuild(
	P12OpaqueBag *opaque,
	SecNssCoder &localCdr)
{
	p12EncodeLog("opaqueBagBuild");
	NSS_P12_SafeBag *safeBag = localCdr.mallocn<NSS_P12_SafeBag>();
	safeBag->bagId = opaque->oid();
	safeBag->bagValue.secretBag = &opaque->blob();
	safeBag->bagAttrs = opaque->getAllAttrs();
	return safeBag;
}
