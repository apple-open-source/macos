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

/*!
	@header SecTrustedApplicationPriv
	Not (yet?) public functions related to SecTrustedApplicationRef objects
*/

#ifndef _SECURITY_SECTRUSTEDAPPLICATIONPRIV_H_
#define _SECURITY_SECTRUSTEDAPPLICATIONPRIV_H_

#include <Security/SecTrustedApplication.h>


#if defined(__cplusplus)
extern "C" {
#endif


/*
 * Determine whether the application at path satisfies the trust expressed in appRef.
 */
OSStatus
SecTrustedApplicationValidateWithPath(SecTrustedApplicationRef appRef, const char *path);


/*
 * Administrative editing of the system's application equivalence database
 */
enum {
	kSecApplicationFlagSystemwide =			0x1,
	kSecApplicationValidFlags =				kSecApplicationFlagSystemwide
};

OSStatus
SecTrustedApplicationMakeEquivalent(SecTrustedApplicationRef oldRef,
	SecTrustedApplicationRef newRef, UInt32 flags);

OSStatus
SecTrustedApplicationRemoveEquivalence(SecTrustedApplicationRef appRef, UInt32 flags);


/*
 * Check to see if an application at a given path is a candidate for
 * pre-emptive code equivalency establishment
 */
OSStatus
SecTrustedApplicationIsUpdateCandidate(const char *installroot, const char *path);


/*
 * Point the system at another system root for equivalence use.
 * This is for system update installers (only)!
 */
OSStatus
SecTrustedApplicationUseAlternateSystem(const char *systemRoot);


#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECTRUSTEDAPPLICATIONPRIV_H_ */
