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

#ifndef SVOTelemetry_h
#define SVOTelemetry_h

#include <sys/types.h>
#include <mach/kern_return.h>
#include <MacTypes.h>
#include <CoreFoundation/CoreFoundation.h>

/// Telemetry class for SVO/dp_login behavior
class SVOTelemetry {
public:
    virtual ~SVOTelemetry();

    enum InitialKeybagUnlockValue {
        initialKeybagUnlock_unlocked = 0,
        initialKeybagUnlock_machError = 1,
        initialKeybagUnlock_otherError = 2,
    };
    InitialKeybagUnlockValue initialKeybagUnlock;
    kern_return_t initialKeybagUnlockErrorCode;

    OSStatus indirectUnlockKeyLookup;

    enum EntropyUnlockValue {
        entropyUnlock_success = 0,
        entropyUnlock_failure = 1,
        entropyUnlock_notapplicable = 2,
    };
    EntropyUnlockValue cachedEntropyUnlockValue;

    enum PasswordUnlockValue {
        passwordUnlock_success = 0,
        passwordUnlock_failure = 1,
        passwordUnlock_notapplicable = 2,
    };
    PasswordUnlockValue passwordUnlockValue;

    enum RekeyValue {
        rekey_success = 0,
        rekey_failure = 1,
        rekey_notapplicable = 2,
    };
    RekeyValue rekeyValue;

    EntropyUnlockValue derivedEntropyUnlockValue;

    OSStatus indirectUnlockKeyAssociate;

protected:
    SVOTelemetry();  // only for subclasses
    CFDictionaryRef createTelemetryDictionary();

private:
    SVOTelemetry(const SVOTelemetry&) = delete;             // no copy ctor
    SVOTelemetry& operator=(const SVOTelemetry&) = delete;  // no op=
};

#endif // SVOTelemetry_h
