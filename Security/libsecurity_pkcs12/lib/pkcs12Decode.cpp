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
 * pkcs12Decode.h - P12Coder decoding engine.
 */
 
#include "pkcs12Coder.h"
#include "pkcs12Templates.h"
#include "pkcs12Utils.h"
#include "pkcs12Debug.h"
#include "pkcs12Crypto.h"
#include <security_cdsa_utilities/cssmerrors.h>
#include <security_asn1/nssUtils.h>

/* top-level PKCS12 PFX decoder */
void P12Coder::decode(
	CFDataRef				cdpfx)
{
	SecNssCoder localCdr;
	NSS_P12_DecodedPFX pfx;

	p12DecodeLog("decode");
	memset(&pfx, 0, sizeof(pfx));
	const CSSM_DATA rawBlob = {CFDataGetLength(cdpfx),
		(uint8 *)CFDataGetBytePtr(cdpfx)};
		
	if(localCdr.decodeItem(rawBlob, NSS_P12_DecodedPFXTemplate, &pfx)) {
		p12ErrorLog("Error on top-level decode of NSS_P12_DecodedPFX\n");
		P12_THROW_DECODE;
	}
	NSS_P7_DecodedContentInfo &dci = pfx.authSafe;
	if(dci.type != CT_Data) {
		/* no other types supported yet */
		p12ErrorLog("bad top-level contentType\n");
		P12_THROW_DECODE;
	}
	mIntegrityMode = kSecPkcs12ModePassword;

	if(pfx.macData == NULL) {
		/* not present is an error in kSecPkcs12ModePassword */
		p12ErrorLog("no MAC in PFX\n");
		P12_THROW_DECODE;
	}
	macParse(*pfx.macData, localCdr);

	const CSSM_DATA *macPhrase = getMacPassPhrase();
	const CSSM_KEY *macPassKey = getMacPassKey();
	if((macPhrase == NULL) && (macPassKey == NULL)) {
		p12ErrorLog("no passphrase set\n");
		CssmError::throwMe(CSSMERR_CSP_MISSING_ATTR_PASSPHRASE);
	}
	CSSM_RETURN crtn = p12VerifyMac(pfx, mCspHand, macPhrase, 
		macPassKey, localCdr);
	if(crtn) {
		p12LogCssmError("p12VerifyMac", crtn);
		CssmError::throwMe(errSecPkcs12VerifyFailure);
	}
	
	authSafeParse(*dci.content.data, localCdr);

	/*
	 * On success, if we have a keychain, store certs and CRLs there
	 */
	if(mKeychain != NULL) {
		storeDecodeResults();
	}
}

/*
 * Decrypt the contents of a NSS_P7_EncryptedData
 */
void P12Coder::encryptedDataDecrypt(
	const NSS_P7_EncryptedData &edata,
	SecNssCoder &localCdr,
	NSS_P12_PBE_Params *pbep,	// preparsed
	CSSM_DATA &ptext)			// result goes here in localCdr space
{
	p12DecodeLog("encryptedDataDecrypt");

	/* see if we can grok the encr alg */
	CSSM_ALGORITHMS		keyAlg;			// e.g., CSSM_ALGID_DES
	CSSM_ALGORITHMS		encrAlg;		// e.g., CSSM_ALGID_3DES_3KEY_EDE
	CSSM_ALGORITHMS		pbeHashAlg;		// SHA1 or MD5
	uint32				keySizeInBits;
	uint32				blockSizeInBytes;	// for IV, optional
	CSSM_PADDING		padding;		// CSSM_PADDING_PKCS7, etc.
	CSSM_ENCRYPT_MODE	mode;			// CSSM_ALGMODE_CBCPadIV8, etc.
	PKCS_Which			pkcs;
	
	bool found = pkcsOidToParams(&edata.contentInfo.encrAlg.algorithm,
		keyAlg, encrAlg, pbeHashAlg, keySizeInBits, blockSizeInBytes,
		padding, mode, pkcs);
	if(!found || (pkcs != PW_PKCS12)) {
		p12ErrorLog("EncryptedData encrAlg not understood\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
		
	uint32 iterCount;
	if(!p12DataToInt(pbep->iterations, iterCount)) {
		p12ErrorLog("encryptedDataDecrypt: badly formed iterCount\n");
		P12_THROW_DECODE;
	}
	const CSSM_DATA *pwd = getEncrPassPhrase();
	const CSSM_KEY *passKey = getEncrPassKey();
	if((pwd == NULL) && (passKey == NULL)) {
		p12ErrorLog("no passphrase set\n");
		CssmError::throwMe(CSSMERR_CSP_MISSING_ATTR_PASSPHRASE);
	}
	
	/* go */
	CSSM_RETURN crtn = p12Decrypt(mCspHand,
		edata.contentInfo.encrContent,
		keyAlg, encrAlg, pbeHashAlg,
		keySizeInBits, blockSizeInBytes,
		padding, mode,
		iterCount, pbep->salt,
		pwd,
		passKey, 
		localCdr, 
		ptext);
	if(crtn) {
		CssmError::throwMe(crtn);
	}
}


/*
 * Parse an CSSM_X509_ALGORITHM_IDENTIFIER specific to P12.
 * Decode the alg params as a NSS_P12_PBE_Params and parse and 
 * return the result if the pbeParams is non-NULL.
 */
void P12Coder::algIdParse(
	const CSSM_X509_ALGORITHM_IDENTIFIER &algId,
	NSS_P12_PBE_Params *pbeParams,		// optional
	SecNssCoder &localCdr)
{
	p12DecodeLog("algIdParse");

	const CSSM_DATA &param = algId.parameters;
	if(pbeParams == NULL) {
		/* alg params are uninterpreted */
		return;
	}
	
	if(param.Length == 0) {
		p12ErrorLog("algIdParse: no alg parameters\n");
		P12_THROW_DECODE;
	}
	
	memset(pbeParams, 0, sizeof(*pbeParams));
	if(localCdr.decodeItem(param, 
			NSS_P12_PBE_ParamsTemplate, pbeParams)) {
		p12ErrorLog("Error decoding NSS_P12_PBE_Params\n");
		P12_THROW_DECODE;
	}
}

/*
 * Parse a NSS_P7_EncryptedData - specifically in the context
 * of a P12 in password privacy mode. (The latter assumption is
 * to enable us to infer CSSM_X509_ALGORITHM_IDENTIFIER.parameters
 * format). 
 */
void P12Coder::encryptedDataParse(
	const NSS_P7_EncryptedData &edata,
	SecNssCoder &localCdr,
	NSS_P12_PBE_Params *pbep)		// optional, RETURNED
{
	p12DecodeLog("encryptedDataParse");

	/*
	 * Parse the alg ID, save PBE params for when we do the decrypt
	 * key unwrap
	 */
	const NSS_P7_EncrContentInfo &ci = edata.contentInfo;
	const CSSM_X509_ALGORITHM_IDENTIFIER &algId = ci.encrAlg;
	algIdParse(algId, pbep, localCdr);
}

/*
 * ShroudedKeyBag parser w/decrypt
 */
void P12Coder::shroudedKeyBagParse(
	const NSS_P12_SafeBag &safeBag,
	SecNssCoder &localCdr)
{
	p12DecodeLog("Found shrouded key bag");
	if(mPrivKeyImportState == PKIS_NoMore) {
		CssmError::throwMe(errSecMultiplePrivKeys);	
	}

	const NSS_P12_ShroudedKeyBag *keyBag = safeBag.bagValue.shroudedKeyBag;
	const CSSM_X509_ALGORITHM_IDENTIFIER &algId = keyBag->algorithm;
	NSS_P12_PBE_Params pbep;
	algIdParse(algId, &pbep, localCdr);

	/*
	 * Prepare for decryption
	 */
	CSSM_ALGORITHMS		keyAlg;			// e.g., CSSM_ALGID_DES
	CSSM_ALGORITHMS		encrAlg;		// e.g., CSSM_ALGID_3DES_3KEY_EDE
	CSSM_ALGORITHMS		pbeHashAlg;		// SHA1 or MD5
	uint32				keySizeInBits;
	uint32				blockSizeInBytes;	// for IV, optional
	CSSM_PADDING		padding;		// CSSM_PADDING_PKCS7, etc.
	CSSM_ENCRYPT_MODE	mode;			// CSSM_ALGMODE_CBCPadIV8, etc.
	PKCS_Which			pkcs;
	
	bool found = pkcsOidToParams(&algId.algorithm,
		keyAlg, encrAlg, pbeHashAlg, keySizeInBits, blockSizeInBytes,
		padding, mode, pkcs);
	if(!found || (pkcs != PW_PKCS12)) {
		p12ErrorLog("ShroudedKeyBag encrAlg not understood\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}

	uint32 iterCount;
	if(!p12DataToInt(pbep.iterations, iterCount)) {
		p12ErrorLog("ShroudedKeyBag: badly formed iterCount\n");
		P12_THROW_DECODE;
	}
	const CSSM_DATA *encrPhrase = getEncrPassPhrase();
	const CSSM_KEY *passKey = getEncrPassKey();
	if((encrPhrase == NULL) && (passKey == NULL)) {
		p12ErrorLog("no passphrase set\n");
		CssmError::throwMe(CSSMERR_CSP_MISSING_ATTR_PASSPHRASE);
	}
	
	/* We'll own the actual CSSM_KEY memory */
	CSSM_KEY_PTR privKey = (CSSM_KEY_PTR)mCoder.malloc(sizeof(CSSM_KEY));
	memset(privKey, 0, sizeof(CSSM_KEY));
	
	CSSM_DATA labelData;
	p12GenLabel(labelData, localCdr);	
	
	CSSM_RETURN crtn = p12UnwrapKey(mCspHand,
		mDlDbHand.DLHandle ? &mDlDbHand : NULL,
		mImportFlags & kSecImportKeys,
		keyBag->encryptedData,
		keyAlg, encrAlg, pbeHashAlg,
		keySizeInBits, blockSizeInBytes,
		padding, mode,
		iterCount, pbep.salt,
		encrPhrase,
		passKey,
		localCdr, 
		labelData,
		mAccess,
		mNoAcl,
		mKeyUsage,
		mKeyAttrs,
		privKey);
	if(crtn) {
		p12ErrorLog("Error unwrapping private key\n");
		CssmError::throwMe(crtn);
	}
	p12DecodeLog("unwrapped shrouded key bag");

	P12KeyBag *p12bag = new P12KeyBag(privKey, mCspHand,
		safeBag.bagAttrs, labelData, mCoder);
	addKey(p12bag);
	
	if(mPrivKeyImportState == PKIS_AllowOne) {
		mPrivKeyImportState = PKIS_NoMore;	
	}
}

/*
 * (unshrouded) KeyBag parser
 */
void P12Coder::keyBagParse(
	const NSS_P12_SafeBag &safeBag,
	SecNssCoder &localCdr)
{
	if(mPrivKeyImportState == PKIS_NoMore) {
		CssmError::throwMe(errSecMultiplePrivKeys);	
	}
	
	/* FIXME - should be able to parse and handle this.... */
	p12DecodeLog("found keyBag");
	NSS_P12_KeyBag *keyBag = safeBag.bagValue.keyBag;
	P12OpaqueBag *p12Bag = new P12OpaqueBag(safeBag.bagId, 
		/* this breaks when NSS_P12_KeyBag is not a CSSM_DATA */
		*keyBag,			
		safeBag.bagAttrs,
		mCoder);
	addOpaque(p12Bag);
}

/*
 * CertBag parser
 */
void P12Coder::certBagParse(
	const NSS_P12_SafeBag &safeBag,
	SecNssCoder &localCdr)
{
	p12DecodeLog("found certBag");
	NSS_P12_CertBag *certBag = safeBag.bagValue.certBag;
	switch(certBag->type) {
		case CT_X509:
		case CT_SDSI:
			break;
		default:
			p12ErrorLog("certBagParse: unknown cert type\n");
			P12_THROW_DECODE;
	}
	P12CertBag *p12Bag = new P12CertBag(certBag->type, 
		certBag->certValue,
		safeBag.bagAttrs,
		mCoder);
	addCert(p12Bag);
}

/*
 * CrlBag parser
 */
void P12Coder::crlBagParse(
	const NSS_P12_SafeBag &safeBag,
	SecNssCoder &localCdr)
{
	p12DecodeLog("found crlBag");
	NSS_P12_CrlBag *crlBag = safeBag.bagValue.crlBag;
	switch(crlBag->type) {
		case CRT_X509:
			break;
		default:
			p12ErrorLog("crlBagParse: unknown CRL type\n");
			P12_THROW_DECODE;
	}
	P12CrlBag *p12Bag = new P12CrlBag(crlBag->type, 
		crlBag->crlValue,
		safeBag.bagAttrs,
		mCoder);
	addCrl(p12Bag);
}

/*
 * SecretBag parser
 */
void P12Coder::secretBagParse(
	const NSS_P12_SafeBag &safeBag,
	SecNssCoder &localCdr)
{
	p12DecodeLog("found secretBag");
	NSS_P12_SecretBag *secretBag = safeBag.bagValue.secretBag;
	P12OpaqueBag *p12Bag = new P12OpaqueBag(safeBag.bagId, 
		/* this breaks when NSS_P12_SecretBag is not a CSSM_DATA */
		*secretBag,			
		safeBag.bagAttrs,
		mCoder);
	addOpaque(p12Bag);
}

/*
 * SafeContentsBag parser
 */
void P12Coder::safeContentsBagParse(
	const NSS_P12_SafeBag &safeBag,
	SecNssCoder &localCdr)
{
	p12DecodeLog("found SafeContents safe bag");
	NSS_P12_SafeContentsBag *scBag = safeBag.bagValue.safeContentsBag;
	P12OpaqueBag *p12Bag = new P12OpaqueBag(safeBag.bagId, 
		/* this breaks when NSS_P12_SafeContentsBag is not a CSSM_DATA */
		*scBag,			
		safeBag.bagAttrs,
		mCoder);
	addOpaque(p12Bag);
}

/*
 * Parse an encoded NSS_P12_SafeContents. This could be either 
 * present as plaintext in an AuthSafe or decrypted. 
 */
void P12Coder::safeContentsParse(
	const CSSM_DATA &contentsBlob,
	SecNssCoder &localCdr)
{
	p12DecodeLog("safeContentsParse");

	NSS_P12_SafeContents sc;
	memset(&sc, 0, sizeof(sc));
	if(localCdr.decodeItem(contentsBlob, NSS_P12_SafeContentsTemplate,
			&sc)) {
		p12ErrorLog("Error decoding SafeContents\n");
		P12_THROW_DECODE;
	}
	unsigned numBags = nssArraySize((const void **)sc.bags);
	for(unsigned dex=0; dex<numBags; dex++) {
		NSS_P12_SafeBag *bag = sc.bags[dex];
		assert(bag != NULL);
		
		/* ensure that *something* is there */
		if(bag->bagValue.keyBag == NULL) {
			p12ErrorLog("safeContentsParse: Empty SafeBag\n");
			P12_THROW_DECODE;
		}
		
		/*
		 * Break out to individual bag type
		 */
		switch(bag->type) {
			case BT_KeyBag:
				keyBagParse(*bag, localCdr);
				break;
			case BT_ShroudedKeyBag:
				shroudedKeyBagParse(*bag, localCdr);
				break;
			case BT_CertBag:
				certBagParse(*bag, localCdr);
				break;
			case BT_CrlBag:
				crlBagParse(*bag, localCdr);
				break;
			case BT_SecretBag:
				secretBagParse(*bag ,localCdr);
				break;
			case BT_SafeContentsBag:
				safeContentsBagParse(*bag, localCdr);
				break;
			default:
				p12ErrorLog("unknown  p12 BagType (%u)\n",
					(unsigned)bag->type);
				P12_THROW_DECODE;
		}
	}
}

/*
 * Parse a ContentInfo in the context of (i.e., as an element of)
 * an AuthenticatedSafe.
 */
void P12Coder::authSafeElementParse(
	const NSS_P7_DecodedContentInfo *info,
	SecNssCoder &localCdr)
{
	p12DecodeLog("authSafeElementParse");
	switch(info->type) {
		case CT_Data:
			/* unencrypted SafeContents */
			safeContentsParse(*info->content.data, localCdr);
			break;
			
		case CT_EncryptedData:
		{
			NSS_P12_PBE_Params pbep;
			encryptedDataParse(*info->content.encryptData, localCdr, &pbep);

			/* 
			 * Decrypt contents to get a SafeContents and
			 * then parse that.
			 */
			CSSM_DATA ptext = {0, NULL};
			encryptedDataDecrypt(*info->content.encryptData,
				localCdr, &pbep, ptext);
			safeContentsParse(ptext, localCdr);
			break;
		}	
		default:
			p12ErrorLog("authSafeElementParse: unknown sage type (%u)\n",
				(unsigned)info->type);
				
			/* well, save it as an opaque bag for now */
			P12OpaqueBag *opaque = new P12OpaqueBag(
				info->contentType, *info->content.data,
				NULL, 	// no attrs
				localCdr);
			addOpaque(opaque);
			break;
	}
}

/*
 * Parse an encoded NSS_P12_AuthenticatedSafe
 */
void P12Coder::authSafeParse(
	const CSSM_DATA &authSafeBlob,
	SecNssCoder &localCdr)
{
	p12DecodeLog("authSafeParse");

	NSS_P12_AuthenticatedSafe authSafe;
	
	memset(&authSafe, 0, sizeof(authSafe));
	if(localCdr.decodeItem(authSafeBlob,
			NSS_P12_AuthenticatedSafeTemplate,
			&authSafe)) {
		p12ErrorLog("Error decoding authSafe\n");
		P12_THROW_DECODE;
	}
	unsigned numInfos = nssArraySize((const void **)authSafe.info);
	for(unsigned dex=0; dex<numInfos; dex++) {
		NSS_P7_DecodedContentInfo *info = authSafe.info[dex];
		authSafeElementParse(info, localCdr);
	}
}

void P12Coder::macParse(
	const NSS_P12_MacData &macData, 
	SecNssCoder &localCdr)
{
	p12DecodeLog("macParse");
	algIdParse(macData.mac.digestAlgorithm, NULL, localCdr);
	const CSSM_DATA &iter = macData.iterations;
	if(iter.Length > 4) {
		p12ErrorLog("malformed iteration length (%u)\n",
				(unsigned)iter.Length);
		P12_THROW_DECODE;
	}
}

