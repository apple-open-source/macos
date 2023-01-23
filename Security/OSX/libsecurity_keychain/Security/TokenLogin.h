/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#ifndef TokenLogin_h
#define TokenLogin_h

#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

OSStatus TokenLoginGetContext(const void *base64TokenLoginData, UInt32 base64TokenLoginDataLength, CFDictionaryRef *context);
OSStatus TokenLoginGetLoginData(CFDictionaryRef context, CFDictionaryRef *loginData);
OSStatus TokenLoginGetPin(CFDictionaryRef context, CFStringRef *pin);

OSStatus TokenLoginCreateLoginData(CFStringRef tokenId, CFDataRef pubKeyHash, CFDataRef pubKeyHashWrap, CFDataRef unlockKey, CFDataRef scBlob);
OSStatus TokenLoginUpdateUnlockData(CFDictionaryRef context, CFStringRef password);
OSStatus TokenLoginStoreUnlockData(CFDictionaryRef context, CFDictionaryRef loginData);
OSStatus TokenLoginDeleteUnlockData(CFDataRef pubKeyHash);

OSStatus TokenLoginGetUnlockKey(CFDictionaryRef context, CFDataRef *unlockKey);
OSStatus TokenLoginGetScBlob(CFDataRef pubKeyHash, CFStringRef tokenId, CFStringRef password, CFDataRef *scBlob);
OSStatus TokenLoginUnlockKeybag(CFDictionaryRef context, CFDictionaryRef loginData);

#ifdef __cplusplus
}
#endif

#endif /* TokenLogin_h */
