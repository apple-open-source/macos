// Copyright (c) 2026 Apple Inc. All Rights Reserved.
//
// The contents of this file constitute Original Code as defined in and are
// subject to the Apple Public Source License Version 1.2 (the 'License').
// You may not use this file except in compliance with the License. Please obtain
// a copy of the License at http://www.apple.com/publicsource and read it before
// using this file.
//
// This Original Code and all software distributed under the License are
// distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
// OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
// LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
// specific language governing rights and limitations under the License.

//
// SVOTelemetry - Telemetry class for SVO/dp_login behavior
//

#include "SVOTelemetry.h"
#include "SecBase.h"

SVOTelemetry::SVOTelemetry() : initialKeybagUnlock(initialKeybagUnlock_otherError), initialKeybagUnlockErrorCode(0), indirectUnlockKeyLookup(0),
    cachedEntropyUnlockValue(entropyUnlock_notapplicable), passwordUnlockValue(passwordUnlock_notapplicable), rekeyValue(rekey_notapplicable),
    derivedEntropyUnlockValue(entropyUnlock_notapplicable), indirectUnlockKeyAssociate(errSecUnimplemented) {
}

SVOTelemetry::~SVOTelemetry() {
}

CFDictionaryRef SVOTelemetry::createTelemetryDictionary() {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 8, NULL, NULL);

    CFNumberRef n;

    n = CFNumberCreate(NULL, kCFNumberIntType, &cachedEntropyUnlockValue);
    CFDictionaryAddValue(dict, CFSTR("cachedEntropyUnlock"), n);
    CFRelease(n);

    n = CFNumberCreate(NULL, kCFNumberIntType, &derivedEntropyUnlockValue);
    CFDictionaryAddValue(dict, CFSTR("derivedEntropyUnlock"), n);
    CFRelease(n);

    n = CFNumberCreate(NULL, kCFNumberSInt32Type, &indirectUnlockKeyAssociate);
    CFDictionaryAddValue(dict, CFSTR("indirectUnlockKeyAssociate"), n);
    CFRelease(n);

    n = CFNumberCreate(NULL, kCFNumberSInt32Type, &indirectUnlockKeyLookup);
    CFDictionaryAddValue(dict, CFSTR("indirectUnlockKeyLookup"), n);
    CFRelease(n);

    n = CFNumberCreate(NULL, kCFNumberIntType, &initialKeybagUnlock);
    CFDictionaryAddValue(dict, CFSTR("initialKeybagUnlock"), n);
    CFRelease(n);

    n = CFNumberCreate(NULL, kCFNumberIntType, &initialKeybagUnlockErrorCode);
    CFDictionaryAddValue(dict, CFSTR("machErrorCode"), n);
    CFRelease(n);

    n = CFNumberCreate(NULL, kCFNumberIntType, &passwordUnlockValue);
    CFDictionaryAddValue(dict, CFSTR("passwordUnlock"), n);
    CFRelease(n);

    n = CFNumberCreate(NULL, kCFNumberIntType, &rekeyValue);
    CFDictionaryAddValue(dict, CFSTR("rekey"), n);
    CFRelease(n);

    return dict;
}
