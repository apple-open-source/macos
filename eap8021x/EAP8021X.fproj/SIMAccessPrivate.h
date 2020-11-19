/*
 * Copyright (c) 2018-2019 Apple Inc. All rights reserved.
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

#ifndef _EAP8021X_SIMACCESS_PRIVATE_H
#define _EAP8021X_SIMACCESS_PRIVATE_H

#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFRunLoop.h>
#include "EAPSIMAKA.h"

#if TARGET_OS_IPHONE

CFStringRef
_SIMCopyIMSI(CFDictionaryRef properties);

CFStringRef
_SIMCopyRealm(CFDictionaryRef properties);

CFDictionaryRef
_SIMCopyEncryptedIMSIInfo(EAPType type);

Boolean
_SIMIsOOBPseudonymSupported(Boolean *isSupported);

CFStringRef
_SIMCopyOOBPseudonym(void);

void
_SIMReportDecryptionError(CFDataRef encryptedIdentity);

CFDictionaryRef
_SIMCreateAuthResponse(CFStringRef slotUUID, CFDictionaryRef auth_params);

typedef void (*SIMAccessConnectionCallback)(CFTypeRef connection, CFStringRef status, void* info);

CFTypeRef
_SIMAccessConnectionCreate(void);

void
_SIMAccessConnectionRegisterForNotification(CFTypeRef connection, SIMAccessConnectionCallback callback, void *info, CFRunLoopRef runLoop,
					    CFStringRef runLoopMode);

#endif /* TARGET_OS_IPHONE */

#endif /* _EAP8021X_SIMACCESS_PRIVATE_H */
