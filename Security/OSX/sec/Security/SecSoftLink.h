/*
 * Copyright (c) 2006-2007,2009-2010,2012-2013,2019 Apple Inc. All Rights Reserved.
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
    @header SecSoftLink.h
    Contains declaration of framework and classes/constants softlinked into Security
*/

#ifndef _SECURITY_SECSOFTLINK_H_
#define _SECURITY_SECSOFTLINK_H_

#import <Foundation/Foundation.h>
#import <LocalAuthentication/LocalAuthentication_Private.h>
#import <CryptoTokenKit/CryptoTokenKit_Private.h>
#import <SoftLinking/SoftLinking.h>

SOFT_LINK_OPTIONAL_FRAMEWORK_FOR_HEADER(CryptoTokenKit);
SOFT_LINK_CLASS_FOR_HEADER(CryptoTokenKit, TKClientToken);
SOFT_LINK_CLASS_FOR_HEADER(CryptoTokenKit, TKClientTokenSession);
SOFT_LINK_OBJECT_CONSTANT_FOR_HEADER(CryptoTokenKit, TKErrorDomain);
SOFT_LINK_OPTIONAL_FRAMEWORK_FOR_HEADER(LocalAuthentication);
SOFT_LINK_CLASS_FOR_HEADER(LocalAuthentication, LAContext);
SOFT_LINK_OBJECT_CONSTANT_FOR_HEADER(LocalAuthentication, LAErrorDomain);

#endif /* !_SECURITY_SECSOFTLINK_H_ */
