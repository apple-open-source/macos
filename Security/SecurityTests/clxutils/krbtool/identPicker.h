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
 * Pick an identity, cook up complete cert chain based upon it. The first
 * element in the returned array is a SecIdentityRef; the remaining items
 * are SecCertificateRefs. 
 */
OSStatus identPicker(
    SecKeychainRef	kc,		// NULL means use default list
    SecCertificateRef	trustedAnchor,	// optional additional trusted anchor
    bool		includeRoot,	// true --> root is appended to outArray
					// false --> root not included
    CFArrayRef		*outArray);	// created and RETURNED
	
/*
 * Simple version, just returns a SecIdentityRef.
 */
OSStatus simpleIdentPicker(
    SecKeychainRef	kc,		// NULL means use default list
    SecIdentityRef	*ident);	// RETURNED
	
#ifdef __cplusplus
}
#endif

#endif	/* _IDENT_PICKER_H_ */

