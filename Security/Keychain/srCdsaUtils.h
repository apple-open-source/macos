/*
	File:		 srCdsaUtils.h
	
	Description: common CDSA access utilities

	Author:		dmitch

	Copyright: 	© Copyright 2001 Apple Computer, Inc. All rights reserved.
	
	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple 
	            Computer, Inc. ("Apple") in consideration of your agreement to 
				the following terms, and your use, installation, modification 
				or redistribution of this Apple software constitutes acceptance 
				of these terms.  If you do not agree with these terms, please 
				do not use, install, modify or redistribute this Apple software.

				In consideration of your agreement to abide by the following 
				terms, and subject to these terms, Apple grants you a personal, 
				non-exclusive license, under Apple's copyrights in this 
				original Apple software (the "Apple Software"), to use, 
				reproduce, modify and redistribute the Apple Software, with 
				or without modifications, in source and/or binary forms; 
				provided that if you redistribute the Apple Software in 
				its entirety and without modifications, you must retain
				this notice and the following text and disclaimers in all 
				such redistributions of the Apple Software.  Neither the 
				name, trademarks, service marks or logos of Apple Computer, 
				Inc. may be used to endorse or promote products derived from the
				Apple Software without specific prior written permission from 
				Apple.  Except as expressly stated in this notice, no other 
				rights or licenses, express or implied, are granted by Apple 
				herein, including but not limited to any patent rights that
				may be infringed by your derivative works or by other works 
				in which the Apple Software may be incorporated.

				The Apple Software is provided by Apple on an "AS IS" basis.  
				APPLE MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING 
				WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT,
				MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, 
				REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE 
				OR IN COMBINATION WITH YOUR PRODUCTS.

				IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, 
				INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
				LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
				LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
				ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION 
				AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED 
				AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING 
				NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE 
				HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
extern void * srAppMalloc (uint32 size, void *allocRef);
extern void srAppFree (void *mem_ptr, void *allocRef);
extern void * srAppRealloc (void *ptr, uint32 size, void *allocRef);
extern void * srAppCalloc (uint32 num, uint32 size, void *allocRef);

#define APP_MALLOC(s)		srAppMalloc(s, NULL)
#define APP_FREE(p)			srAppFree(p, NULL)
#define APP_REALLOC(p, s)	srAppRealloc(p, s, NULL)
#define APP_CALLOC(n, s)	srAppRealloc(n, s, NULL)

extern CSSM_BOOL srCompareCssmData(
	const CSSM_DATA *d1,
	const CSSM_DATA *d2);
	
/* OID flavor of same, which will break when an OID is not a CSSM_DATA */
#define srCompareOid(o1, o2)	srCompareCssmData(o1, o2)

void srPrintError(char *op, CSSM_RETURN err);

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