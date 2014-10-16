/*
 * Copyright (c) 2003-2007 Apple Inc. All Rights Reserved.
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
 * identPicker.h - Given a keychain, select from possible multiple
 * 				   SecIdentityRefs via stdio UI, and cook up a 
 *				   CFArray containing that identity and all certs needed
 *				   for cert verification by an SSL peer. The resulting
 *				   CFArrayRef is suitable for passing to SSLSetCertificate().
 */
 
#ifndef	_IDENT_PICKER_H_
#define _IDENT_PICKER_H_

#include <Security/Security.h>
#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Get the final term of a keychain's path as a C string. Caller must free() 
 * the result.
 */
char *kcFileName(
	SecKeychainRef kcRef);

/*
 * Obtain the printable name of a SecKeychainItemRef as a C string.
 * Caller must free() the result.
 */
char *kcItemPrintableName(
	SecKeychainItemRef itemRef);

/* 
 * Obtain the final term of a keychain item's keychain path as a C string. 
 * Caller must free() the result.
 * May well return NULL indicating the item has no keychain (e.g. az floating cert).
 */
char *kcItemKcFileName(SecKeychainItemRef itemRef);

/* 
 * Safe gets().
 * -- guaranteed no buffer overflow
 * -- guaranteed NULL-terminated string
 * -- handles empty string (i.e., response is just CR) properly
 */
void getString(
	char *buf,
	unsigned bufSize);

/* 
 * IdentityPicker, returns full cert chain, optionally including root. 
 */
OSStatus sslIdentPicker(
	SecKeychainRef		kc,				// NULL means use default list
	SecCertificateRef	trustedAnchor,	// optional additional trusted anchor
	bool				includeRoot,	// true --> root is appended to outArray
										// false --> root not included
	const CSSM_OID		*vfyPolicy,		// optional - if NULL, use SSL
	CFArrayRef			*outArray);		// created and RETURNED
	
/*
 * Simple version, just returns a SecIdentityRef.
 */
OSStatus sslSimpleIdentPicker(
	SecKeychainRef		kc,				// NULL means use default list
	SecIdentityRef		*ident);		// RETURNED
	
#ifdef __cplusplus
}
#endif

#endif	/* _IDENT_PICKER_H_ */

