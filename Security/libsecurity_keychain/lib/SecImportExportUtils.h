/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
 * SecImportExportUtils.h - misc. utilities for import/export module
 */

#ifndef	_SECURITY_SEC_IMPORT_EXPORT_UTILS_H_
#define _SECURITY_SEC_IMPORT_EXPORT_UTILS_H_

#include "SecImportExport.h"
#include "SecKeychainPriv.h"
#include "SecBasePriv.h"
#include <security_utilities/debugging.h>
#include <security_utilities/errors.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Macros for begin/end of public functions. Although the import/export
 * module does not intentionally throw C++ exceptions which are intended to 
 * be caught like this, we catch possible exceptions which can conceivably
 * be thrown here or below. 
 */
 
#define BEGIN_IMP_EXP_SECAPI \
	try {

#define END_IMP_EXP_SECAPI \
	} \
	catch (const MacOSError &err) { return err.osStatus(); } \
	catch (const CommonError &err) { return SecKeychainErrFromOSStatus(err.osStatus()); } \
	catch (const std::bad_alloc &) { return errSecAllocate; }\
	catch (...) { return errSecInternalComponent; }

/* 
 * Debug support.
 */
 
#ifdef  NDEBUG

#define impExpExtFormatStr(f)
#define impExpExtItemTypeStr(t)

#else

extern const char *impExpExtFormatStr(SecExternalFormat format);
extern const char *impExpExtItemTypeStr(SecExternalItemType itemType);

#endif  /* NDEBUG */

#define SecImpExpDbg(args...)	secdebug("SecImpExp", ## args)
#define SecImpInferDbg(args...)	secdebug("SecImpInfer", ## args)

/* 
 * Parse file extension and attempt to map it to format and type. Returns true 
 * on success. 
 */
bool impExpImportParseFileExten(
	CFStringRef			fstr,
	SecExternalFormat   *inputFormat,   // RETURNED
	SecExternalItemType	*itemType);		// RETURNED

/* do a [NSString stringByDeletingPathExtension] equivalent */
CFStringRef impExpImportDeleteExtension(
	CFStringRef			fileStr);

/*
 * map {algorithm, class, SecExternalFormat} to a CSSM_KEYBLOB_FORMAT.
 * Returns errSecUnsupportedFormat in the rare appropriate case.
 */
OSStatus impExpKeyForm(
	SecExternalFormat		externForm,
	SecExternalItemType		itemType,
	CSSM_ALGORITHMS			alg,
	CSSM_KEYBLOB_FORMAT		*cssmForm,		// RETURNED
	CSSM_KEYCLASS			*cssmClass);	// RETRUNED

/*
 * Guess an incoming blob's type, format and (for keys only) algorithm
 * by examining its contents. Returns true on success, in which case 
 * *inputFormat and *itemType, and *keyAlg are valid. Caller optionally 
 * passes in valid values any number of these as a clue.
 */
bool impExpImportGuessByExamination(
	CFDataRef			inData,
	SecExternalFormat   *inputFormat,	// may be kSecFormatUnknown on entry 
	SecExternalItemType	*itemType,		// may be kSecItemTypeUnknown on entry 
	CSSM_ALGORITHMS		*keyAlg);		// CSSM_ALGID_NONE - unknown

/*
 * Obtain the CDSA-layer CSSM_RESOURCE_CONTROL_CONTEXT given a SecAccessRef.
 */
extern OSStatus impExpAccessToRcc(
	SecAccessRef	accessRef,
	const CSSM_RESOURCE_CONTROL_CONTEXT **rcc);



/* low-level crypto/CSP support */

/*
 * Given a context specified via a CSSM_CC_HANDLE, add a new
 * CSSM_CONTEXT_ATTRIBUTE to the context as specified by AttributeType,
 * AttributeLength, and an untyped pointer.
 */
CSSM_RETURN impExpAddContextAttribute(CSSM_CC_HANDLE CCHandle,
	uint32 AttributeType,
	uint32 AttributeLength,
	const void *AttributePtr);

/*
 * Free memory via specified plugin's app-level allocator
 */
void impExpFreeCssmMemory(
	CSSM_HANDLE		hand,
	void 			*p);

/* 
 * Calculate digest of any CSSM_KEY. Unlike older implementations
 * of this logic, you can actually calculate the public key hash 
 * on any class of key, any format, raw CSP or CSPDL.
 *
 * Caller must free keyDigest->Data using impExpFreeCssmMemory() since 
 * this is allocated by the CSP's app-specified allocator.
 */
CSSM_RETURN impExpKeyDigest(
	CSSM_CSP_HANDLE cspHand,
	CSSM_KEY_PTR	key,
	CSSM_DATA_PTR   keyDigest);		// contents allocd and RETURNED

/*
 * Given a CFTypeRef passphrase which may be a CFDataRef or a CFStringRef,
 * return a refcounted CFStringRef suitable for use with the PKCS12 library.
 * PKCS12 passphrases in CFData format must be UTF8 encoded. 
 */
OSStatus impExpPassphraseToCFString(
	CFTypeRef   passin,
	CFStringRef *passout);	// may be the same as passin, but refcounted

/*
 * Given a CFTypeRef passphrase which may be a CFDataRef or a CFStringRef,
 * return a refcounted CFDataRef whose bytes are suitable for use with 
 * PKCS5 (v1.5 and v2.0) key derivation.
 */
OSStatus impExpPassphraseToCFData(
	CFTypeRef   passin,
	CFDataRef   *passout);	// may be the same as passin, but refcounted

/*
 * Obtain passphrase, given a SecKeyImportExportParameters. 
 *
 * Passphrase comes from one of two places: app-specified, in 
 * SecKeyImportExportParameters.passphrase (either as CFStringRef
 * or CFDataRef); or via the secure passphrase mechanism.
 *
 * Passphrase is returned in one of two forms:
 *
 * -- Secure passphrase is returned as a CSSM_KEY_PTR, which the 
 *    caller must CSSM_FreeKey later. THe CSSM_KEY_PTR must also 
 *    be free()d. 
 *
 * -- CFTypeRef for app-supplied passphrases. This can be one of 
 *    two types:
 *
 *    -- CFStringRef, for use with P12
 *    -- CFDataRef, for more general use (e.g. for PKCS5). 
 *   
 *    In either case the caller must CFRelease the result.    
 */
typedef enum {
	SPF_String,			// CFStringRef, P12
	SPF_Data			// CFDataRef, PKCS5
} impExpPassphraseForm;

typedef enum {
	VP_Export,			// verify passphrase
	VP_Import			// no verify
} impExpVerifyPhrase;

OSStatus impExpPassphraseCommon(
	const SecKeyImportExportParameters *keyParams,
	CSSM_CSP_HANDLE			cspHand,		// MUST be CSPDL, for passKey generation
	impExpPassphraseForm	phraseForm,
	impExpVerifyPhrase		verifyPhrase,   // for secure passphrase
	CFTypeRef				*phrase,		// RETURNED, or
	CSSM_KEY_PTR			*passKey);		// mallocd and RETURNED
	
CSSM_KEYATTR_FLAGS ConvertArrayToKeyAttributes(SecKeyRef aKey, CFArrayRef usage);

Boolean ConvertSecKeyImportExportParametersToSecImportExportKeyParameters(SecKeyRef aKey,
	const SecItemImportExportKeyParameters* newPtr, SecKeyImportExportParameters* oldPtr);

#ifdef	__cplusplus
}
#endif

#endif  /* _SECURITY_SEC_IMPORT_EXPORT_UTILS_H_ */

