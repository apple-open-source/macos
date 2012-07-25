/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
 * SecImportExportCrypto.h - low-level crypto routines for wrapping and unwrapping
 *							 keys.
 */
 
 
#ifndef	_SECURITY_SEC_IMPORT_EXPORT_CRYPTO_H_
#define _SECURITY_SEC_IMPORT_EXPORT_CRYPTO_H_

#include <Security/cssmtype.h>
#include <Security/SecAccess.h>
#include <Security/SecKeychain.h>
#include <Security/SecImportExport.h>
#include <CoreFoundation/CoreFoundation.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <stdint.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* 
 * Post notification of a "new key added" event.
 * If you know of another way to do this, other than a dlclient-based lookup of the
 * existing key in order to get a KeychainCore::Item, by all means have at it. 
 */
OSStatus impExpKeyNotify(
	SecKeychainRef	importKeychain,
	const CssmData	&keyLabel,		// stored with this, we use it to do a lookup
	const CSSM_KEY	&cssmKey);		// unwrapped key in CSSM format

/*
 * Attempt to import a raw key. This can be used as a lightweight
 * "guess" evaluator if a handle to the raw CSP is passed in (with 
 * no keychaain), or as the real thing which does full keychain import.
 */
OSStatus impExpImportRawKey(
	CFDataRef							inData,
	SecExternalFormat					externForm,
	SecExternalItemType					itemType,
	CSSM_ALGORITHMS						keyAlg,
	SecKeychainRef						importKeychain, // optional
	CSSM_CSP_HANDLE						cspHand,		// optional
	SecItemImportExportFlags			flags,
	const SecKeyImportExportParameters	*keyParams,		// optional 
	const char							*printName,		// optional
	CFMutableArrayRef					outArray);		// optional, append here 

/*
 * Auxiliary encryption parameters associated with a key unwrap.
 * Most of these are usually zero (meaning "tell the CSP to take the default").
 */
typedef struct {
	CSSM_ALGORITHMS			encrAlg;		// 0 ==> null unwrap
	CSSM_ENCRYPT_MODE		encrMode;
	CSSM_KEY_PTR			unwrappingKey;  // NULL ==> null unwrap
	CSSM_PADDING			encrPad;
	CSSM_DATA				iv;
	
	/* weird RC2/RC5 params */
	uint32					effectiveKeySizeInBits; // RC2 
	uint32					blockSizeInBits;		// RC5 
	uint32					rounds;					// RC5 
} impExpKeyUnwrapParams;

/*
 * Common code to unwrap a key, used for raw keys (which do a NULL unwrap) and 
 * wrapped keys.
 */
OSStatus impExpImportKeyCommon(
	const CSSM_KEY					*wrappedKey,
	SecKeychainRef					importKeychain, // optional
	CSSM_CSP_HANDLE					cspHand,		// optional
	SecItemImportExportFlags		flags,
	const SecKeyImportExportParameters *keyParams,  // optional 
	const impExpKeyUnwrapParams		*unwrapParams,
	const char						*printName,		// optional
	CFMutableArrayRef				outArray);		// optional, append here 

/* 
 * Common code to wrap a key. NULL unwraps don't use this (yet?). 
 */
CSSM_RETURN impExpExportKeyCommon(
	CSSM_CSP_HANDLE		cspHand,		// for all three keys
	SecKeyRef			secKey,
	CSSM_KEY_PTR		wrappingKey,
	CSSM_KEY_PTR		wrappedKey,		// RETURNED
	CSSM_ALGORITHMS		wrapAlg,
	CSSM_ENCRYPT_MODE   wrapMode,
	CSSM_PADDING		wrapPad,
	CSSM_KEYBLOB_FORMAT	wrapFormat,		// NONE, PKCS7, PKCS8
	CSSM_ATTRIBUTE_TYPE blobAttrType,	// optional raw key format attr
	CSSM_KEYBLOB_FORMAT blobForm,		// ditto
	const CSSM_DATA		*descData,		// optional descriptive data
	const CSSM_DATA		*iv);
	
#ifdef	__cplusplus
}
#endif

#endif  /* _SECURITY_SEC_IMPORT_EXPORT_CRYPTO_H_ */
