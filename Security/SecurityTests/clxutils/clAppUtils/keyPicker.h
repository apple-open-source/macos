/*
 * Copyright (c) 2004-2006 Apple Computer, Inc. All Rights Reserved.
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
 * keyPicker.h - select a key pair from a keychain
 */
 
#ifndef	_KEY_PICKER_H_
#define _KEY_PICKER_H_

#include <Security/Security.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Obtain either public key hash or PrintName for a given SecKeychainItem. Works on public keys,
 * private keys, identities, and certs. Caller must release the returned result. 
 */
typedef enum {
	WA_Hash,
	WA_PrintName
} WhichAttr;

OSStatus getKcItemAttr(
	SecKeychainItemRef kcItem,
	WhichAttr whichAttr,
	CFDataRef *rtnAttr);		// RETURNED 

OSStatus keyPicker(
	SecKeychainRef  kcRef,		// NULL means the default list
	SecKeyRef		*pubKey,	// RETURNED
	SecKeyRef		*privKey);  // RETURNED


#ifdef __cplusplus
}
#endif

#endif	/* _KEY_PICKER_H_ */

