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
 
/*******************************************************************
 * 
 * SecPkcs12.h
 *
 * This module is an implementation of the logic required to create 
 * and parse PKCS12 "blobs", known as PFXs in PKCS12 lingo. The user
 * of this module need not know anything about the details of 
 * PKCS12 PFX construction. All one needs to know at this level
 * is that a PKCS12 PFX is a collection of the following items:
 *
 * -- Zero or more certificates
 * -- Zero or more Certficate Revocation Lists (CRLs)
 * -- Zero or more private keys. (If this number is zero, using this
 *    module is probably not what you want to do)
 * -- Zero or more other opaque types, not understood or parsed
 *    by this module.
 *
 * Each individual component of a PFX contains zero or more
 * attributes; commonly the only two such attributes used in 
 * the PKCS12 world are "FriendlyName", a Unicode string, and 
 * "LocalKeyId", an opaque data blob which serves solely to tie
 * a specific cert to a specific key in the context of this specific
 * PFX.
 * 
 * Individual components of a PKCS12 PFX are typically encrypted with
 * a key derived from a user-supplied passphrase. The entire PFX
 * is protected with a MAC whose key is also derived from a user-
 * supplied passphrase. Typically these two passphrases are identical
 * but they don't have to be. 
 *
 * There are a number of options and modes which, while described in 
 * the PKCS12 spec and provided for in the interface in this file,
 * are rarely if ever used. The following is a description of the 
 * actual, typical, real-world use of this module.
 *
 * Decoding a PKCS12 blob
 * ----------------------
 *
 * 1. App creates a SecPkcs12CoderRef via SecPkcs12CoderCreate().
 * 
 * 2. App specifies supplies a (small) number of options such as
 *    passphrase(s) and SecKeychainRefs. 
 * 
 * 3. App calls SecPkcs12Decode(), providing the raw PKCS12 PFX 
 *    blob which is to be decoded. This performs all of the actual 
 *    decoding and decryption.
 *
 * 4. At this point the app optionally obtains the resulting 
 *    components by a set of calls which return individual 
 *    certs, CRLS, and keys.
 *
 * 5. Also, per the configuration performed in step 2, individual 
 *    components (certs, keys) found in the PFX have been added 
 *    to a specified keychain, rendering step 4 superfluous. 
 *
 *
 * Creating a PKCS12 blob
 * ----------------------
 *
 * 1. App creates a SecPkcs12CoderRef via SecPkcs12CoderCreate().
 *
 * 2. App specifies supplies a (small) number of options such as
 *    passphrase(s).
 * 
 * 3. App makes a set of calls which add individual components such 
 *    as certs, CRLs, and private keys. A high-level call, 
 *    SecPkcs12ExportKeychainItems(), allow the specification of 
 *    all components to be exported at once.
 *
 * 4. App calls SecPkcs12Encode(), which does all of the required
 *    encryption and encoding. The result is an exportable PKCS12
 *    PFX blob. 
 */
 
#ifndef	_SEC_PKCS12_H_
#define _SEC_PKCS12_H_

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Opaque handle for a PKCS12 encoder/decoder.
 */
typedef void 	*SecPkcs12CoderRef;

#pragma mark --- SecPkcs12CoderRef create/destroy ---

/*
 * Basic SecPkcs12CoderRef create/destroy.
 */
OSStatus SecPkcs12CoderCreate(
	SecPkcs12CoderRef	*coder);		// RETURNED
	
/*
 * Destroy object created in SecPkcs12CoderCreate.
 */
OSStatus SecPkcs12CoderRelease(
	SecPkcs12CoderRef	coder);	

#pragma mark --- High-level API ---

/*
 * Keychain associated with encode/decode. 
 * Client must call exactly one of { SecPkcs12SetKeychain(),
 * SecPkcs12SetCspHandle() } for both encoding and decoding. 
 * If SecPkcs12SetCspHandle() is used, components which are
 * obtained during decode are ephemeral (i.e., they are not 
 * stored anywhere and only have a lifetime which is the same as
 * the lifetime of the SecPkcs12CoderRef). 
 */
OSStatus SecPkcs12SetKeychain(
	SecPkcs12CoderRef		coder,
	SecKeychainRef			keychain);
	
/* 
 * Required iff SecPkcs12SetKeychain() is not called.
 */
OSStatus SecPkcs12SetCspHandle(
	SecPkcs12CoderRef		coder,
	CSSM_CSP_HANDLE			cspHandle);


/*
 * PKCS12 allows for separate passphrases for encryption and for
 * verification (via MAC). Typically, in the real world, one
 * passphrase is used for both; we provide the means to set them
 * separately.
 *
 * Passphrases can be specified directly as CFStringRefs, or as 
 * CSSM_KEYs which represent secure passphrases obtained by the 
 * SecurityServer. This latter method is preferred since the 
 * plaintext passphrase never appears in the app's address space.
 * Passphrases expressed in this manner are referred to as 
 * PassKeys.
 *
 * If one passphrase is to be used for both encryption and 
 * verification, use one of these two function to set it.
 */
OSStatus SecPkcs12SetMACPassphrase(
	SecPkcs12CoderRef	coder,
	CFStringRef			passphrase);
	
OSStatus SecPkcs12SetMACPassKey(
	SecPkcs12CoderRef	coder,
	const CSSM_KEY		*passKey);
	
/*
 * Specify separate passphrase for encrypt/decrypt.
 */
OSStatus SecPkcs12SetCryptPassphrase(
	SecPkcs12CoderRef	coder,
	CFStringRef			passphrase);
 
OSStatus SecPkcs12SetCryptPassKey(
	SecPkcs12CoderRef	coder,
	const CSSM_KEY		*passKey);
 
/*
 * Prior to decoding a PFX, client can specify whether individual
 * components (certificates, CRLs, and keys) get stored in the 
 * keychain specified via SecPkcs12SetKeychain().
 */
enum {
	kSecImportCertificates	= 0x0001,
	kSecImportCRLs			= 0x0002,
	kSecImportKeys			= 0x0004,
};

typedef UInt32 SecPkcs12ImportFlags;

OSStatus SecPkcs12SetImportToKeychain(
	SecPkcs12CoderRef			coder,
	SecPkcs12ImportFlags		flags);

OSStatus SecPkcs12GetImportToKeychain(
	SecPkcs12CoderRef			coder,
	SecPkcs12ImportFlags		*flags);		// RETURNED
	
/*
 * Specify individual SecKeychainItemRef to export, prior to encoding.
 * The items argument is a CFArray containing any number of each 
 * of the following SecKeychainItemRef objects:
 *
 *		SecKeyRef
 *		SecCertificateRef
 *		...and others, in the future.
 */
OSStatus SecPkcs12ExportKeychainItems(
	SecPkcs12CoderRef			coder,
	CFArrayRef					items);

/*
 * Specify additional optional imported private key attributes: 
 * -- a SecAccessRef; default is the default ACL. Passing NULL here 
 *    results in private keys being created with no ACL.
 * -- CSSM_KEYUSE; default is CSSM_KEYUSE_ANY. 
 * -- CSSM_KEYATTR_FLAGS; default is CSSM_KEYATTR_RETURN_REF | 
 *    CSSM_KEYATTR_EXTRACTABLE | CSSM_KEYATTR_SENSITIVE, plus
 *    CSSM_KEYATTR_PERMANENT if importing to a keychain
 */
OSStatus SecPkcs12SetAccess(
	SecPkcs12CoderRef		coder,
	SecAccessRef			access);
	
OSStatus SecPkcs12SetKeyUsage(
	SecPkcs12CoderRef		coder,
	CSSM_KEYUSE				keyUsage);
	
OSStatus SecPkcs12SetKeyAttrs(
	SecPkcs12CoderRef		coder,
	CSSM_KEYATTR_FLAGS		keyAttrs);
	
/*
 * Parse and decode.
 */
OSStatus SecPkcs12Decode(
	SecPkcs12CoderRef		coder,
	CFDataRef				pfx);
	
/* 
 * This the final step to create an encoded PKCS12 PFX blob.
 * This called after initial configuration of the SecPkcs12CoderRef,
 * and either specifying items to export via either 
 * SecPkcs12ExportKeychainItems() or some number of SecPkcs12Add* 
 * function calls, described below.
 *
 * The result is a DER-encoded PFX in PKCS12 lingo.
 */
OSStatus SecPkcs12Encode(
	SecPkcs12CoderRef		coder,
	CFDataRef				*pfx);			// RETURNED


/*
 * Opaque handle for optional attributes associated with any 
 * component of a SecPkcs12CoderRef.
 *
 * The use of SecPkcs12AttrsRefs is optional and in fact, in the real 
 * world, rare. Their appearance in this API is just for completeness
 * and to allow access to all "legal" PKCS12 options. 
 *
 * We define the type here to allow use elsewhere in this
 * interface; actual SecPkcs12AttrsRef manipulation functions
 * are described later in this header. 
 */
typedef void 	*SecPkcs12AttrsRef;

#pragma mark --- Decoder Functions ---

/*
 * Subsequent to decoding, obtain the components.
 * These functions can also be used as "getter" functions while encoding.
 *
 * Certificates:
 */
OSStatus SecPkcs12CertificateCount(
	SecPkcs12CoderRef		coder,
	CFIndex					*numCerts);		// RETURNED
	
OSStatus SecPkcs12CopyCertificate(
	SecPkcs12CoderRef		coder,
	CFIndex					certNum,
	SecCertificateRef		*cert,			// RETURNED
	CFStringRef				*friendlyName,	// optional, RETURNED
	CFDataRef				*localKeyId,	// optional, RETURNED
	SecPkcs12AttrsRef		*attrs);		// optional, RETURNED
	
/*
 * CRLs. The might change if a SecCrl type is defined elsewhere.
 * We'll typedef it here to preserve the semantics of this function.
 */
typedef CFDataRef	SecCrlRef;

OSStatus SecPkcs12CrlCount(
	SecPkcs12CoderRef		coder,
	CFIndex					*numCrls);		// RETURNED
	
OSStatus SecPkcs12CopyCrl(
	SecPkcs12CoderRef		coder,
	CFIndex					crlNum,
	SecCrlRef				*crl,			// RETURNED
	CFStringRef				*friendlyName,	// optional, RETURNED
	CFDataRef				*localKeyId,	// optional, RETURNED
	SecPkcs12AttrsRef		*attrs);		// optional, RETURNED
 
/*
 * Private keys.
 */
OSStatus SecPkcs12PrivateKeyCount(
	SecPkcs12CoderRef		coder,
	CFIndex					*numKeys);		// RETURNED
	
/* currently not implemented : use SecPkcs12GetCssmPrivateKey() */
OSStatus SecPkcs12CopyPrivateKey(
	SecPkcs12CoderRef		coder,
	CFIndex					keyNum,
	SecKeyRef				*privateKey,	// RETURNED
	CFStringRef				*friendlyName,	// optional, RETURNED
	CFDataRef				*localKeyId,	// optional, RETURNED
	SecPkcs12AttrsRef		*attrs);		// optional, RETURNED

/*
 * The CSSM_KEY_PTR returned by this function has a lifetime 
 * which is the same as the SecPkcs12CoderRef which created it.
 */
OSStatus SecPkcs12GetCssmPrivateKey(
	SecPkcs12CoderRef		coder,
	CFIndex					keyNum,
	CSSM_KEY_PTR			*privateKey,	// RETURNED
	CFStringRef				*friendlyName,	// optional, RETURNED
	CFDataRef				*localKeyId,	// optional, RETURNED
	SecPkcs12AttrsRef		*attrs);		// optional, RETURNED

/*
 * Catch-all for other components not currently understood
 * or supported by this library. An "opaque blob" component 
 * is identified by an OID and is obtained as an opaque data 
 * blob.
 */
OSStatus SecPkcs12OpaqueBlobCount(
	SecPkcs12CoderRef		coder,
	CFIndex					*numBlobs);		// RETURNED
	
OSStatus SecPkcs12CopyOpaqueBlob(
	SecPkcs12CoderRef		coder,
	CFIndex					blobNum,
	CFDataRef				*blobOid,		// RETURNED
	CFDataRef				*opaqueBlob,	// RETURNED
	CFStringRef				*friendlyName,	// optional, RETURNED
	CFDataRef				*localKeyId,	// optional, RETURNED
	SecPkcs12AttrsRef		*attrs);		// optional, RETURNED

#pragma mark --- Encoder Functions ---

/*
 * Add individual components. "Getter" functions are available
 * as described above (under "Functions used for decoding").
 */
OSStatus SecPkcs12AddCertificate(
	SecPkcs12CoderRef		coder,
	SecCertificateRef		cert,			
	CFStringRef				friendlyName,	// optional
	CFDataRef				localKeyId,		// optional
	SecPkcs12AttrsRef		attrs);			// optional
	
OSStatus SecPkcs12AddCrl(
	SecPkcs12CoderRef		coder,
	SecCrlRef				crl,			
	CFStringRef				friendlyName,	// optional
	CFDataRef				localKeyId,		// optional
	SecPkcs12AttrsRef		attrs);			// optional
	
OSStatus SecPkcs12AddPrivateKey(
	SecPkcs12CoderRef		coder,
	SecKeyRef				privateKey,			
	CFStringRef				friendlyName,	// optional
	CFDataRef				localKeyId,		// optional
	SecPkcs12AttrsRef		attrs);			// optional

OSStatus SecPkcs12AddOpaqueBlob(
	SecPkcs12CoderRef		coder,
	CFDataRef				blobOid,	
	CFDataRef				opaqueBlob,
	CFStringRef				friendlyName,	// optional
	CFDataRef				localKeyId,		// optional
	SecPkcs12AttrsRef		attrs);			// optional


#pragma mark --- Optional Functions ---

/************************************************************
 *** Optional, rarely used SecPkcs12CoderRef manipulation ***
 ************************************************************/
 
/***
 *** SecPkcs12AttrsRef manipulation. Optional and in fact expected to 
 *** be rarely used, if ever. 
 ***/
 
/*
 * A SecPkcs12AttrsRef is an opaque handle referring to an aribtrary
 * collection of OID/value pairs which can be attached to any 
 * component of a SecPkcs12CoderRef. OIDs and values are expressed
 * as CFDataRefs. Each OID can have associated with it an arbitrary 
 * number of values. 
 */
 
/*
 * Create/destroy.
 */
OSStatus SecPkcs12AttrsCreate(
	SecPkcs12AttrsRef	*attrs);		// RETURNED

OSStatus SecPkcs12AttrsRelease(
	SecPkcs12AttrsRef	attrs);

/*
 * Add an OID/value set to an existing SecPkcs12AttrsRef.
 * Values are a CFArray containing an arbitrary number of 
 * CFDataRefs. 
 */
OSStatus SecPkcs12AttrsAddAttr(
	SecPkcs12AttrsRef	attrs,
	CFDataRef			attrOid,
	CFArrayRef			attrValues);	// an array of CFDataRefs
	
OSStatus SecPkcs12AttrCount(
	SecPkcs12AttrsRef	attrs,
	CFIndex				*numAttrs);		// RETURNED

/* 
 * Obtain n'th oid/value set from an existing SecPkcs12AttrsRef.
 */
OSStatus SecPkcs12AttrsGetAttr(
	SecPkcs12AttrsRef	attrs,
	CFIndex				attrNum,
	CFDataRef			*attrOid,		// RETURNED
	CFArrayRef			*attrValues);	// RETURNED
	
/***
 *** Integrity and Privacy Modes
 ***/
 
/*
 * PKCS12 allows for two different modes for each of {privacy,
 * integrity}. Each of these can be implemented via password
 * or public key. Per the PKCS12 spec, all four combinations
 * of these modes are legal. In the current version of this
 * library, only password privacy and integrity modes are 
 * implemented. These functions are defined here for the 
 * completeness of the API and need never be called by users of
 * the current implementation.
 */
typedef enum {
	kSecPkcs12ModeUnknown,		// uninitialized
	kSecPkcs12ModePassword,
	kSecPkcs12ModePublicKey
} SecPkcs12Mode;
 
OSStatus SecPkcs12SetIntegrityMode(
	SecPkcs12CoderRef	coder,
	SecPkcs12Mode		mode);
	
OSStatus SecPkcs12GetIntegrityMode(
	SecPkcs12CoderRef	coder,
	SecPkcs12Mode		*mode);			// RETURNED
	
OSStatus SecPkcs12SetPrivacyMode(
	SecPkcs12CoderRef	coder,
	SecPkcs12Mode		mode);
	
OSStatus SecPkcs12GetPrivacyMode(
	SecPkcs12CoderRef	coder,
	SecPkcs12Mode		*mode);			// RETURNED
	
/***
 *** Encryption algorithms
 ***/

/*
 * Each individual component of a PKCS12 PFX can be encrypted with
 * a different encryption algorithm. Typically, Certs and CRLs are 
 * all encrypted with one weak algorithm, and private keys are 
 * encrypted with a stronger algorithm. 
 *
 * The following functions allow the app to specify, during encoding,
 * the encryption algorithms to use for the different kinds of 
 * components. These are optional; this library provides appropriate
 * defaults for these algorithms.
 */
OSStatus SecPkcs12SetKeyEncryptionAlg(
	SecPkcs12CoderRef	coder,
	CFDataRef			encryptionAlg);
	
OSStatus SecPkcs12SetCertCrlEncryptionAlg(
	SecPkcs12CoderRef	coder,
	CFDataRef			encryptionAlg);
	
/*
 * Along with an encryption algorithm is an iteration count used for
 * deriving keys. All of these are optional; reasonable defaults
 * are provided. 
 *
 * NOTE: salt is not visible at this API. During encoding, 
 * random values of salt are generated by this module.
 */
OSStatus SecPkcs12SetKeyEncryptionIterCount(
	SecPkcs12CoderRef	coder,
	unsigned			iterCount);

OSStatus SecPkcs12SetCertCrlEncryptionIterCount(
	SecPkcs12CoderRef	coder,
	unsigned			iterCount);

OSStatus SecPkcs12SetMacIterCount(
	SecPkcs12CoderRef	coder,
	unsigned			iterCount);

/*
 * "Getter" versions of the above. During decryption, the values
 * returned here refer to the *first* such element found (e.g.,
 * the encryption algorithm for the first key). 
 */
OSStatus SecPkcs12CopyKeyEncryptionAlg(
	SecPkcs12CoderRef	coder,
	CFDataRef			*encryptionAlg);		// RETURNED
	
OSStatus SecPkcs12CopyCertCrlEncryptionAlg(
	SecPkcs12CoderRef	coder,
	CFDataRef			*encryptionAlg);		// RETURNED
	
OSStatus SecPkcs12CopyKeyEncryptionIterCount(
	SecPkcs12CoderRef	coder,
	unsigned			*iterCount);			// RETURNED

OSStatus SecPkcs12CopyCertCrlEncryptionIterCount(
	SecPkcs12CoderRef	coder,
	unsigned			*iterCount);			// RETURNED
	
OSStatus SecPkcs12CopyMacIterCount(
	SecPkcs12CoderRef	coder,
	unsigned			*iterCount);			// RETURNED
	
/*
 * Avoid importing multiple private keys. Primarily for use by 
 * SecKeychainItemImport(). Behavior depends on the foundOneKey
 * argument, which indicates whether the current high-level import
 * has already imported at least one key. If foundOneKey is true,
 * SecPkcs12Decode() will return errSecMultiplePrivKeys upon
 * the detection of *any* private keys in the incoming PFX.
 * If foundOneKey is false, SecPkcs12Decode() will return 
 * errSecMultiplePrivKeys if more than one private key is 
 * found in the incoming PFX.
 */
OSStatus SecPkcs12LimitPrivateKeyImport(
	SecPkcs12CoderRef	coder,
	bool				foundOneKey);
	
#ifdef __cplusplus
}
#endif

#endif	/* _SEC_PKCS12_H_ */

