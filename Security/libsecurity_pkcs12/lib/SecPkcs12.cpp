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
 * SecPkcs12.cpp
 */
 
#include "SecPkcs12.h"
#include "pkcs12Coder.h"
#include "pkcs12BagAttrs.h"
#include "pkcs12SafeBag.h"
#include "pkcs12Utils.h"
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <Security/SecBasePriv.h>

/*
 * API function call wrappers, impermeable to C++ exceptions
 */
#define BEGIN_P12API \
	try {

#define END_P12API \
	} \
	catch (const MacOSError &err) { return err.osStatus(); } \
	catch (const CommonError &err) { return SecKeychainErrFromOSStatus(err.osStatus()); } \
	catch (const std::bad_alloc &) { return memFullErr; } \
	catch (...) { return internalComponentErr; } \
    return noErr;

/* catch incoming NULL parameters */
static inline void required(
	const void *param)
{
	if(param == NULL) {
		MacOSError::throwMe(paramErr);
	}
}

/*
 * Standard means of casting a SecPkcs12CoderRef to a P12Coder *
 */
static inline P12Coder *P12CoderCast(
	SecPkcs12CoderRef coder)
{
	required(coder);
	return reinterpret_cast<P12Coder *>(coder);
}

/*
 * Standard means of casting a SecPkcs12AttrsRef to a P12BagAttrs *
 * This one uses the P12BagAttrsStandAlone version, not tied to
 * a specific P12Coder (actually, to a P12Coder's SecNssCoder).
 */
static inline P12BagAttrsStandAlone *P12AttrsCast(
	SecPkcs12AttrsRef attrs)
{
	if(attrs == NULL) {
		MacOSError::throwMe(paramErr);
	}
	return reinterpret_cast<P12BagAttrsStandAlone *>(attrs);
}

/* optional flavor used in SecPkcs12Add*() */
static inline P12BagAttrs *P12AttrsCastOpt(
	SecPkcs12AttrsRef attrs)
{
	return reinterpret_cast<P12BagAttrs *>(attrs);
}

#pragma mark --- SecPkcs12CoderRef create/destroy ---

/*
 * Basic SecPkcs12CoderRef create/destroy.
 */
OSStatus SecPkcs12CoderCreate(
	SecPkcs12CoderRef	*coder)		// RETURNED
{
	BEGIN_P12API

	required(coder);
	P12Coder *p12coder = new P12Coder;
	*coder = p12coder;
	
	END_P12API
}

/*
 * Destroy object created in SecPkcs12CoderCreate.
 * This will go away if we make this object a CoreFoundation type.
 */
OSStatus SecPkcs12CoderRelease(
	SecPkcs12CoderRef	coder)
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	delete p12coder;
	
	END_P12API
}

OSStatus SecPkcs12SetMACPassphrase(
	SecPkcs12CoderRef	coder,
	CFStringRef			passphrase)
{
	BEGIN_P12API

	P12Coder *p12coder = P12CoderCast(coder);
	required(passphrase);
	p12coder->setMacPassPhrase(passphrase);
	
	END_P12API
}

OSStatus SecPkcs12SetMACPassKey(
	SecPkcs12CoderRef	coder,
	const CSSM_KEY		*passKey)
{
	BEGIN_P12API

	P12Coder *p12coder = P12CoderCast(coder);
	required(passKey);
	p12coder->setMacPassKey(passKey);
	
	END_P12API
}

/*
 * Specify separate passphrase for encrypt/decrypt.
 */
OSStatus SecPkcs12SetCryptPassphrase(
	SecPkcs12CoderRef	coder,
	CFStringRef			passphrase)
{
	BEGIN_P12API

	P12Coder *p12coder = P12CoderCast(coder);
	required(passphrase);
	p12coder->setEncrPassPhrase(passphrase);
	
	END_P12API
} 

OSStatus SecPkcs12SetCryptPassKey(
	SecPkcs12CoderRef	coder,
	const CSSM_KEY		*passKey)
{
	BEGIN_P12API

	P12Coder *p12coder = P12CoderCast(coder);
	required(passKey);
	p12coder->setEncrPassKey(passKey);
	
	END_P12API
}

	
/*
 * Target location of decoded keys and certs.
 */
OSStatus SecPkcs12SetKeychain(
	SecPkcs12CoderRef		coder,
	SecKeychainRef			keychain)
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	required(keychain);
	p12coder->setKeychain(keychain);
	
	END_P12API
}
	
/* 
 * Required iff SecPkcs12SetKeychain() not called.
 */
OSStatus SecPkcs12SetCspHandle(
	SecPkcs12CoderRef		coder,
	CSSM_CSP_HANDLE			cspHandle)
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	p12coder->setCsp(cspHandle);
	
	END_P12API
}

OSStatus SecPkcs12SetImportToKeychain(
	SecPkcs12CoderRef		coder,
	SecPkcs12ImportFlags	flags)
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	p12coder->importFlags(flags);
	
	END_P12API
}
	
OSStatus SecPkcs12GetImportToKeychain(
	SecPkcs12CoderRef		coder,
	SecPkcs12ImportFlags	*flags)		// RETURNED
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	required(flags);
	*flags = p12coder->importFlags();
	
	END_P12API
}

OSStatus SecPkcs12ExportKeychainItems(
	SecPkcs12CoderRef			coder,
	CFArrayRef					items)
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	required(items);
	p12coder->exportKeychainItems(items);
	
	END_P12API
}

OSStatus SecPkcs12SetAccess(
	SecPkcs12CoderRef		coder,
	SecAccessRef			access)
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	p12coder->setAccess(access);
	
	END_P12API
}

OSStatus SecPkcs12SetKeyUsage(
	SecPkcs12CoderRef		coder,
	CSSM_KEYUSE				keyUsage)
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	p12coder->setKeyUsage(keyUsage);
	
	END_P12API
}
	
OSStatus SecPkcs12SetKeyAttrs(
	SecPkcs12CoderRef		coder,
	CSSM_KEYATTR_FLAGS		keyAttrs)
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	p12coder->setKeyAttrs(keyAttrs);
	
	END_P12API
}	

#pragma mark --- Decoder Functions ---

/*
 * Parse and decode.
 */
OSStatus SecPkcs12Decode(
	SecPkcs12CoderRef		coder,
	CFDataRef				pfx)
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	required(pfx);
	try {
		p12coder->decode(pfx);
	}
	catch(...) {
		/* abort - clean up - delete stored keys */
		p12coder->deleteDecodedItems();
		throw;
	}
	END_P12API
}

/*
 * Subsequent to decoding, obtain the components.
 * These functions can also be used as "getter" functions while encoding.
 *
 * Certificates:
 */
OSStatus SecPkcs12CertificateCount(
	SecPkcs12CoderRef		coder,
	CFIndex					*numCerts)		// RETURNED
{
	BEGIN_P12API

	P12Coder *p12coder = P12CoderCast(coder);
	required(numCerts);
	*numCerts = p12coder->numCerts();
	
	END_P12API
}	

OSStatus SecPkcs12CopyCertificate(
	SecPkcs12CoderRef		coder,
	CFIndex					certNum,
	SecCertificateRef		*secCert,		// RETURNED
	CFStringRef				*friendlyName,	// RETURNED
	CFDataRef				*localKeyId,	// RETURNED
	SecPkcs12AttrsRef		*attrs)			// RETURNED
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	required(secCert);
	/* others are optional - if NULL, we don't return that param */
	P12CertBag *bag = p12coder->getCert(certNum);
	*secCert = bag->getSecCert();
	
	/* now the optional attrs */
	P12BagAttrs *p12Attrs = NULL;
	bag->copyAllAttrs(friendlyName, localKeyId, 
		attrs ? &p12Attrs : NULL);
	if(p12Attrs) {
		*attrs = p12Attrs;
	}
	END_P12API
}

/*
 * CRLs. The might change if a SecCrl type is defined elsewhere.
 * We'll typedef it here to preserve the semantics of this function.
 */
OSStatus SecPkcs12CrlCount(
	SecPkcs12CoderRef		coder,
	CFIndex					*numCrls)		// RETURNED
{
	BEGIN_P12API

	P12Coder *p12coder = P12CoderCast(coder);
	required(numCrls);
	*numCrls = p12coder->numCrls();
	
	END_P12API
}	

OSStatus SecPkcs12CopyCrl(
	SecPkcs12CoderRef		coder,
	CFIndex					crlNum,
	SecCrlRef				*crl,			// RETURNED
	CFStringRef				*friendlyName,	// RETURNED
	CFDataRef				*localKeyId,	// RETURNED
	SecPkcs12AttrsRef		*attrs)			// RETURNED
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	required(crl);
	/* others are optional - if NULL, we don't return that param */
	P12CrlBag *bag = p12coder->getCrl(crlNum);
	*crl = p12CssmDataToCf(bag->crlData());
	
	/* now the optional attrs */
	P12BagAttrs *p12Attrs = NULL;
	bag->copyAllAttrs(friendlyName, localKeyId, 
		attrs ? &p12Attrs : NULL);
	if(p12Attrs) {
		*attrs = p12Attrs;
	}

	END_P12API
}

/*
 * Private keys.
 */
OSStatus SecPkcs12PrivateKeyCount(
	SecPkcs12CoderRef		coder,
	CFIndex					*numKeys)		// RETURNED
{
	BEGIN_P12API

	P12Coder *p12coder = P12CoderCast(coder);
	required(numKeys);
	*numKeys = p12coder->numKeys();
	
	END_P12API
}

OSStatus SecPkcs12CopyPrivateKey(
	SecPkcs12CoderRef		coder,
	CFIndex					keyNum,
	SecKeyRef				*privateKey,	// RETURNED
	CFStringRef				*friendlyName,	// RETURNED
	CFDataRef				*localKeyId,	// RETURNED
	SecPkcs12AttrsRef		*attrs)			// RETURNED
{
	BEGIN_P12API
	/*P12Coder *p12coder = P12CoderCast(coder); */
	return unimpErr;
	END_P12API
}

OSStatus SecPkcs12GetCssmPrivateKey(
	SecPkcs12CoderRef		coder,
	CFIndex					keyNum,
	CSSM_KEY_PTR			*privateKey,	// RETURNED
	CFStringRef				*friendlyName,	// RETURNED
	CFDataRef				*localKeyId,	// RETURNED
	SecPkcs12AttrsRef		*attrs)			// RETURNED
{
	BEGIN_P12API
	P12Coder *p12coder = P12CoderCast(coder);
	required(privateKey);
	/* others are optional - if NULL, we don't return that param */
	P12KeyBag *bag = p12coder->getKey(keyNum);
	*privateKey = bag->key();
	
	/* now the optional attrs */
	P12BagAttrs *p12Attrs = NULL;
	bag->copyAllAttrs(friendlyName, localKeyId, 
		attrs ? &p12Attrs : NULL);
	if(p12Attrs) {
		*attrs = p12Attrs;
	}

	END_P12API
}

/*
 * Catch-all for other components not currently understood
 * or supported by this library. An "opaque blob" component 
 * is identified by an OID and is obtained as an opaque data 
 * blob.
 */
OSStatus SecPkcs12OpaqueBlobCount(
	SecPkcs12CoderRef		coder,
	CFIndex					*numBlobs)		// RETURNED
{
	BEGIN_P12API

	P12Coder *p12coder = P12CoderCast(coder);
	required(numBlobs);
	*numBlobs = p12coder->numOpaqueBlobs();
	
	END_P12API
}

OSStatus SecPkcs12CopyOpaqueBlob(
	SecPkcs12CoderRef		coder,
	CFIndex					blobNum,
	CFDataRef				*blobOid,		// RETURNED
	CFDataRef				*opaqueBlob,	// RETURNED
	CFStringRef				*friendlyName,	// RETURNED
	CFDataRef				*localKeyId,	// RETURNED
	SecPkcs12AttrsRef		*attrs)			// RETURNED
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	required(blobOid);
	required(opaqueBlob);
	
	/* others are optional - if NULL, we don't return that param */
	P12OpaqueBag *bag = p12coder->getOpaque(blobNum);
	*opaqueBlob = p12CssmDataToCf(bag->blob());
	*blobOid    = p12CssmDataToCf(bag->oid());
	
	/* now the optional attrs */
	P12BagAttrs *p12Attrs = NULL;
	bag->copyAllAttrs(friendlyName, localKeyId, 
		attrs ? &p12Attrs : NULL);
	if(p12Attrs) {
		*attrs = p12Attrs;
	}

	END_P12API
}

#pragma mark --- Encoder Functions ---

/* 
 * This the final step to create an encoded PKCS12 PFX blob,
 * after calling some number of SecPkcs12Set* functions below.
 * The result is a DER_encoded PFX in PKCS12 lingo.
 */
OSStatus SecPkcs12Encode(
	SecPkcs12CoderRef		coder,
	CFDataRef				*pfx)			// RETURNED
{
	BEGIN_P12API
	P12Coder *p12coder = P12CoderCast(coder);
	required(pfx);
	p12coder->encode(pfx);
	END_P12API
}

/*
 * Add individual components. "Getter" functions are available
 * as described above (under "Functions used for decoding").
 */
OSStatus SecPkcs12AddCertificate(
	SecPkcs12CoderRef		coder,
	SecCertificateRef		cert,			
	CFStringRef				friendlyName,	// optional
	CFDataRef				localKeyId,		// optional
	SecPkcs12AttrsRef		attrs)			// optional
{
	BEGIN_P12API
	P12Coder *p12coder = P12CoderCast(coder);
	required(cert);
	CSSM_DATA certData;
	OSStatus ortn = SecCertificateGetData(cert, &certData);
	if(ortn) {
		return ortn;
	}
	CSSM_CERT_TYPE certType;
	ortn = SecCertificateGetType(cert, &certType);
	if(ortn) {
		return ortn;
	}
	NSS_P12_CertBagType type;
	switch(certType) {
		case CSSM_CERT_X_509v1:
		case CSSM_CERT_X_509v2:
		case CSSM_CERT_X_509v3:
			type = CT_X509;
			break;
		case CSSM_CERT_SDSIv1:
			type = CT_SDSI;
			break;
		default:
			type = CT_Unknown;
			break;
	}
	P12CertBag *bag = new P12CertBag(type, certData, friendlyName,
		localKeyId, P12AttrsCastOpt(attrs), p12coder->coder());
	p12coder->addCert(bag);
	END_P12API
}

OSStatus SecPkcs12AddCrl(
	SecPkcs12CoderRef		coder,
	SecCrlRef				crl,			
	CFStringRef				friendlyName,	// optional
	CFDataRef				localKeyId,		// optional
	SecPkcs12AttrsRef		attrs)			// optional
{
	BEGIN_P12API
	P12Coder *p12coder = P12CoderCast(coder);
	required(crl);
	P12CrlBag *bag = new P12CrlBag(CRT_X509, crl, friendlyName,
		localKeyId, P12AttrsCastOpt(attrs), p12coder->coder());
	p12coder->addCrl(bag);
	END_P12API
}

OSStatus SecPkcs12AddPrivateKey(
	SecPkcs12CoderRef		coder,
	SecKeyRef				privateKey,			
	CFStringRef				friendlyName,	// optional
	CFDataRef				localKeyId,		// optional
	SecPkcs12AttrsRef		attrs)			// optional
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	required(privateKey);
	const CSSM_KEY *cssmKey;
	OSStatus ortn = SecKeyGetCSSMKey(privateKey, &cssmKey);
	if(ortn) {
		return ortn;
	}
	P12KeyBag *bag = new P12KeyBag(cssmKey, p12coder->cspHand(), 
		friendlyName, localKeyId, P12AttrsCastOpt(attrs), p12coder->coder());
	p12coder->addKey(bag);
	
	END_P12API
}

OSStatus SecPkcs12AddCssmPrivateKey(
	SecPkcs12CoderRef		coder,
	CSSM_KEY_PTR			cssmKey,			
	CFStringRef				friendlyName,	// optional
	CFDataRef				localKeyId,		// optional
	SecPkcs12AttrsRef		attrs)			// optional
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	required(cssmKey);
	P12KeyBag *bag = new P12KeyBag(cssmKey, p12coder->cspHand(), 
		friendlyName, localKeyId, P12AttrsCastOpt(attrs), p12coder->coder());
	p12coder->addKey(bag);
	
	END_P12API
}

OSStatus SecPkcs12AddOpaqueBlob(
	SecPkcs12CoderRef		coder,
	CFDataRef				blobOid,	
	CFDataRef				opaqueBlob,
	CFStringRef				friendlyName,	// optional
	CFDataRef				localKeyId,		// optional
	SecPkcs12AttrsRef		attrs)			// optional
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	required(blobOid);
	required(opaqueBlob);
	P12OpaqueBag *bag = new P12OpaqueBag(blobOid, opaqueBlob, friendlyName,
		localKeyId, P12AttrsCastOpt(attrs), p12coder->coder());
	p12coder->addOpaque(bag);
	
	END_P12API
}

#pragma mark --- Optional Functions ---

/***
 *** SecPkcs12AttrsRef manipulation. Optional and in fact expected to 
 *** be rarely used, if ever. 
 ***/

/*
 * Create/destroy.
 */
OSStatus SecPkcs12AttrsCreate(
	SecPkcs12AttrsRef	*attrs)		// RETURNED
{
	BEGIN_P12API
	
	required(attrs);
	P12BagAttrsStandAlone *bagAttrs = new P12BagAttrsStandAlone;
	*attrs = (SecPkcs12AttrsRef)bagAttrs;

	END_P12API
}

OSStatus SecPkcs12AttrsRelease(
	SecPkcs12AttrsRef	attrs)
{
	BEGIN_P12API
	
	P12BagAttrsStandAlone *bagAttrs = P12AttrsCast(attrs);
	delete bagAttrs;
	
	END_P12API
}

/*
 * Add an OID/value set to an existing SecPkcs12AttrsRef.
 * Values are a CFArray containing an arbitrary number of 
 * CFDataRefs. 
 */
OSStatus SecPkcs12AttrsAddAttr(
	SecPkcs12AttrsRef	attrs,
	CFDataRef			attrOid,
	CFArrayRef			attrValues)
{
	BEGIN_P12API
	
	P12BagAttrsStandAlone *bagAttrs = P12AttrsCast(attrs);
	bagAttrs->addAttr(attrOid, attrValues);

	END_P12API
}

OSStatus SecPkcs12AttrCount(
	SecPkcs12AttrsRef		attrs,
	CFIndex					*numAttrs)		// RETURNED
{
	BEGIN_P12API
	
	P12BagAttrsStandAlone *bagAttrs = P12AttrsCast(attrs);
	required(numAttrs);
	*numAttrs = bagAttrs->numAttrs();

	END_P12API
}

/* 
 * Obtain n'th oid/value set from an existing SecPkcs12AttrsRef.
 */
OSStatus SecPkcs12AttrsGetAttr(
	SecPkcs12AttrsRef	attrs,
	CFIndex				attrNum,
	CFDataRef			*attrOid,		// RETURNED
	CFArrayRef			*attrValues)	// RETURNED
{
	BEGIN_P12API

	P12BagAttrsStandAlone *bagAttrs = P12AttrsCast(attrs);
	required(attrOid);
	required(attrValues);
	bagAttrs->getAttr(attrNum, attrOid, attrValues);
	END_P12API
}

OSStatus SecPkcs12SetIntegrityMode(
	SecPkcs12CoderRef	coder,
	SecPkcs12Mode		mode)
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	p12coder->integrityMode(mode);
	
	END_P12API
}

OSStatus SecPkcs12GetIntegrityMode(
	SecPkcs12CoderRef	coder,
	SecPkcs12Mode		*mode)			// RETURNED
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	required(mode);
	*mode = p12coder->integrityMode();
	
	END_P12API
}

OSStatus SecPkcs12SetPrivacyMode(
	SecPkcs12CoderRef	coder,
	SecPkcs12Mode		mode)
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	p12coder->privacyMode(mode);
	
	END_P12API
}

OSStatus SecPkcs12GetPrivacyMode(
	SecPkcs12CoderRef	coder,
	SecPkcs12Mode		*mode)			// RETURNED
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	required(mode);
	*mode = p12coder->privacyMode();

	END_P12API
}

/***
 *** Encryption algorithms
 ***/
OSStatus SecPkcs12SetKeyEncryptionAlg(
	SecPkcs12CoderRef	coder,
	CFDataRef			encryptionAlg)
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	required(encryptionAlg);
	p12coder->strongEncrAlg(encryptionAlg);
	
	END_P12API
}

OSStatus SecPkcs12SetCertCrlEncryptionAlg(
	SecPkcs12CoderRef	coder,
	CFDataRef			encryptionAlg)
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	required(encryptionAlg);
	p12coder->weakEncrAlg(encryptionAlg);
	
	END_P12API
}

OSStatus SecPkcs12SetKeyEncryptionIterCount(
	SecPkcs12CoderRef	coder,
	unsigned			iterCount)
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	p12coder->strongEncrIterCount(iterCount);
	
	END_P12API
}

OSStatus SecPkcs12SetCertCrlEncryptionIterCount(
	SecPkcs12CoderRef	coder,
	unsigned			iterCount)
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	p12coder->weakEncrIterCount(iterCount);
	
	END_P12API
}

OSStatus SecPkcs12SetMacIterCount(
	SecPkcs12CoderRef	coder,
	unsigned			iterCount)
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	p12coder->macEncrIterCount(iterCount);
	
	END_P12API
}

OSStatus SecPkcs12CopyKeyEncryptionAlg(
	SecPkcs12CoderRef	coder,
	CFDataRef			*encryptionAlg)			// RETURNED
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	required(encryptionAlg);
	*encryptionAlg = p12coder->strongEncrAlg();
	
	END_P12API
}

OSStatus SecPkcs12CopyCertCrlEncryptionAlg(
	SecPkcs12CoderRef	coder,
	CFDataRef			*encryptionAlg)			// RETURNED
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	required(encryptionAlg);
	*encryptionAlg = p12coder->weakEncrAlg();
	
	END_P12API
}

OSStatus SecPkcs12CopyKeyEncryptionIterCount(
	SecPkcs12CoderRef	coder,
	unsigned			*iterCount)				// RETURNED
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	required(iterCount);
	*iterCount = p12coder->strongEncrIterCount();

	END_P12API
}

OSStatus SecPkcs12CopyCertCrlEncryptionIterCount(
	SecPkcs12CoderRef	coder,
	unsigned			*iterCount)				// RETURNED
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	required(iterCount);
	*iterCount = p12coder->weakEncrIterCount();

	END_P12API
}

OSStatus SecPkcs12CopyMacIterCount(
	SecPkcs12CoderRef	coder,
	unsigned			*iterCount)				// RETURNED
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	required(iterCount);
	*iterCount = p12coder->macEncrIterCount();

	END_P12API
}

OSStatus SecPkcs12LimitPrivateKeyImport(
	SecPkcs12CoderRef	coder,
	bool				foundOneKey)
{
	BEGIN_P12API
	
	P12Coder *p12coder = P12CoderCast(coder);
	p12coder->limitPrivKeyImport(foundOneKey);

	END_P12API
}
