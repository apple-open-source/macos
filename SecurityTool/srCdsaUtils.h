/*
 * Copyright (c) 2001,2003-2011 Apple, Inc. All Rights Reserved.
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
 *
 * srCdsaUtils.h -- common CDSA access utilities
 */

#ifndef	_COMMON_CDSA_UTILS_H_
#define _COMMON_CDSA_UTILS_H_

#include <Security/cssm.h>
#include <Security/SecKeychain.h>
#include <CoreFoundation/CFString.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* common memory allocators shared by app and CSSM */
extern void * srAppMalloc (CSSM_SIZE size, void *allocRef);
extern void srAppFree (void *mem_ptr, void *allocRef);
extern void * srAppRealloc (void *ptr, CSSM_SIZE size, void *allocRef);
extern void * srAppCalloc (uint32 num, CSSM_SIZE size, void *allocRef);

#define APP_MALLOC(s)		srAppMalloc(s, NULL)
#define APP_FREE(p)			srAppFree(p, NULL)
#define APP_REALLOC(p, s)	srAppRealloc(p, s, NULL)
#define APP_CALLOC(n, s)	srAppRealloc(n, s, NULL)

extern CSSM_BOOL srCompareCssmData(
	const CSSM_DATA *d1,
	const CSSM_DATA *d2);
	
/* OID flavor of same, which will break when an OID is not a CSSM_DATA */
#define srCompareOid(o1, o2)	srCompareCssmData(o1, o2)

void srPrintError(const char *op, CSSM_RETURN err);

/* Init CSSM; returns CSSM_FALSE on error. Reusable. */
extern CSSM_BOOL srCssmStartup();

/* Attach to CSP. Returns zero on error. */
extern CSSM_CSP_HANDLE srCspStartup(
	CSSM_BOOL bareCsp);					// true ==> CSP, false ==> CSP/DL

/* Attach to DL side of CSPDL. */
extern CSSM_DL_HANDLE srDlStartup();

/* Attach to CL, TP */
extern CSSM_CL_HANDLE srClStartup();
extern CSSM_TP_HANDLE srTpStartup();

/*
 * Derive symmetric key using PBE.
 */
extern CSSM_RETURN srCspDeriveKey(CSSM_CSP_HANDLE cspHand,
		uint32				keyAlg,			// CSSM_ALGID_RC5, etc.
		const char 			*keyLabel,
		unsigned 			keyLabelLen,
		uint32 				keyUsage,		// CSSM_KEYUSE_ENCRYPT, etc.
		uint32 				keySizeInBits,
		CSSM_DATA_PTR		password,		// in PKCS-5 lingo
		CSSM_DATA_PTR		salt,			// ditto
		uint32				iterationCnt,	// ditto
		CSSM_KEY_PTR		key);

/*
 * Generate key pair of arbitrary algorithm. 
 */
extern CSSM_RETURN srCspGenKeyPair(CSSM_CSP_HANDLE cspHand,
	CSSM_DL_DB_HANDLE *dlDbHand,	// optional
	uint32 algorithm,
	const char *keyLabel,
	unsigned keyLabelLen,
	uint32 keySize,					// in bits
	CSSM_KEY_PTR pubKey,			// mallocd by caller
	CSSM_KEYUSE pubKeyUsage,		// CSSM_KEYUSE_ENCRYPT, etc.
	CSSM_KEYATTR_FLAGS pubAttrs,	// CSSM_KEYATTR_EXTRACTABLE, etc. 
	CSSM_KEY_PTR privKey,			// mallocd by caller
	CSSM_KEYUSE privKeyUsage,		// CSSM_KEYUSE_DECRYPT, etc.
	CSSM_KEYATTR_FLAGS privAttrs);	// CSSM_KEYATTR_EXTRACTABLE, etc. 

/* Convert a reference key to a raw key. */
CSSM_RETURN srRefKeyToRaw(CSSM_CSP_HANDLE cspHand,
	const CSSM_KEY			*refKey,	
	CSSM_KEY_PTR			rawKey);		// RETURNED

/*
 * Add a certificate to a keychain.
 */
CSSM_RETURN srAddCertToKC(
	SecKeychainRef		keychain,
	const CSSM_DATA		*cert,
	CSSM_CERT_TYPE		certType,
	CSSM_CERT_ENCODING	certEncoding,
	const char			*printName,		// C string
	const CSSM_DATA		*keyLabel);		// ??

/*
 * Convert a CSSM_DATA_PTR, referring to a DER-encoded int, to an
 * unsigned.
 */
unsigned srDER_ToInt(
	const CSSM_DATA 	*DER_Data);
	
char *srCfStrToCString(
	CFStringRef cfStr);

#ifdef	__cplusplus
}
#endif

#endif	/* _COMMON_CDSA_UTILS_H_ */
