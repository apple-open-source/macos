/*
 * Copyright (c) 2002-2004,2011,2014 Apple Inc. All Rights Reserved.
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
	@header SecCertificateBundle
	The functions provided in SecCertificateBundle implement a way to issue a certificate request to a
	certificate authority.
*/

#ifndef _SECURITY_SECCERTIFICATEBUNDLE_H_
#define _SECURITY_SECCERTIFICATEBUNDLE_H_

#include <Security/SecBase.h>
#include <Security/cssmtype.h>
#include <CoreFoundation/CFArray.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*!
	@function SecCertificateBundleImport
	@abstract Imports one or more certificates into a keychain with the specified encoding and bundle type.
    @param keychain The destination keychain for the import. Specify NULL for the default keychain.
    @param bundle A pointer to the bundle data.
    @param type The bundle type as defined in cssmtype.h.
    @param encodingType The bundle encoding type as defined in cssmtype.h.
    @param keychainListToSkipDuplicates A reference to an array of keychains.  These keychains contain certificates that shouldn't be duplicated during the import.    
    @result A result code.  See "Security Error Codes" (SecBase.h).
*/
OSStatus SecCertificateBundleImport(
        SecKeychainRef keychain,
        const CSSM_CERT_BUNDLE* bundle,
        CSSM_CERT_BUNDLE_TYPE type,
        CSSM_CERT_BUNDLE_ENCODING encodingType,
        CFArrayRef keychainListToSkipDuplicates);
        
/*!
	@function SecCertificateBundleExport
	@abstract Exports one or more certificates into a bundle with the specified encoding and bundle type.
    @param certificates An array of certificate and keychain items used to help build the bundle.
    @param type The bundle type as defined in cssmtype.h. If the bundle type is unknown, an attempt will be made to determine the type for you.
    @param encodingType The encoding type as defined in cssmtype.h.
    @param data A pointer to data.  On return, this points to the bundle data.
    @result A result code.  See "Security Error Codes" (SecBase.h).
*/
OSStatus SecCertificateBundleExport(
        CFArrayRef certificates,
        CSSM_CERT_BUNDLE_TYPE type,
        CSSM_CERT_BUNDLE_ENCODING encodingType,
        CSSM_DATA* data);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECCERTIFICATEBUNDLE_H_ */
