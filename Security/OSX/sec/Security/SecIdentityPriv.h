/*
 * Copyright (c) 2007-2008,2010,2012-2013 Apple Inc. All Rights Reserved.
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
	@header SecIdentityPriv
	The functions provided in SecIdentityPriv.h implement a convenient way to
	match private keys with certificates.
*/

#ifndef _SECURITY_SECIDENTITYPRIV_H_
#define _SECURITY_SECIDENTITYPRIV_H_

#include <Security/SecBase.h>
#include <CoreFoundation/CFBase.h>

__BEGIN_DECLS

/*!	@function SecIdentityCreate.
	@abstract create a new identity object from the passed-in certificate
	and private key.
	@discussion this function will most likely become private SPI soon.
*/
SecIdentityRef SecIdentityCreate(CFAllocatorRef allocator,
	SecCertificateRef certificate, SecKeyRef privateKey);

__END_DECLS

#endif /* !_SECURITY_SECIDENTITYPRIV_H_ */
