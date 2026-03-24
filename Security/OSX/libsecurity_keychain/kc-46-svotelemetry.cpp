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

// Have to de-name-mangle all the things in the test headers. Sigh.
#ifdef __cplusplus
extern "C" {
#endif

#include "keychain_regressions.h"
#include "kc-helpers.h"

#ifdef __cplusplus
} // extern "C"
#endif

#include <mach/mig_errors.h>
#include "SVOTelemetry.h"
#include "SecCFRelease.h"

class SVOTelemetryReporter {
public:
    SVOTelemetryReporter() : dict(NULL) {};
    ~SVOTelemetryReporter() { CFReleaseSafe(dict); }

    static const uint kCount_test_validate = 3; // includes ok in ~SVOTelemetryTester
    bool test_validate(SVOTelemetry::InitialKeybagUnlockValue initialKeybagUnlock, kern_return_t machErrorCode, OSStatus indirectUnlockKeyLookup, SVOTelemetry::EntropyUnlockValue cachedEntropyUnlockValue,
                       SVOTelemetry::PasswordUnlockValue passwordUnlockValue, SVOTelemetry::RekeyValue rekeyValue, SVOTelemetry::EntropyUnlockValue derivedEntropyUnlockValue, OSStatus indirectUnlockKeyAssociate) {
        CFMutableDictionaryRef expected = CFDictionaryCreateMutable(NULL, 8, NULL, NULL);
        CFNumberRef n;

        n = CFNumberCreate(NULL, kCFNumberIntType, &cachedEntropyUnlockValue);
        CFDictionaryAddValue(expected, CFSTR("cachedEntropyUnlock"), n);
        CFRelease(n);

        n = CFNumberCreate(NULL, kCFNumberIntType, &derivedEntropyUnlockValue);
        CFDictionaryAddValue(expected, CFSTR("derivedEntropyUnlock"), n);
        CFRelease(n);

        n = CFNumberCreate(NULL, kCFNumberSInt32Type, &indirectUnlockKeyAssociate);
        CFDictionaryAddValue(expected, CFSTR("indirectUnlockKeyAssociate"), n);
        CFRelease(n);

        n = CFNumberCreate(NULL, kCFNumberSInt32Type, &indirectUnlockKeyLookup);
        CFDictionaryAddValue(expected, CFSTR("indirectUnlockKeyLookup"), n);
        CFRelease(n);

        n = CFNumberCreate(NULL, kCFNumberIntType, &initialKeybagUnlock);
        CFDictionaryAddValue(expected, CFSTR("initialKeybagUnlock"), n);
        CFRelease(n);

        n = CFNumberCreate(NULL, kCFNumberIntType, &machErrorCode);
        CFDictionaryAddValue(expected, CFSTR("machErrorCode"), n);
        CFRelease(n);

        n = CFNumberCreate(NULL, kCFNumberIntType, &passwordUnlockValue);
        CFDictionaryAddValue(expected, CFSTR("passwordUnlock"), n);
        CFRelease(n);

        n = CFNumberCreate(NULL, kCFNumberIntType, &rekeyValue);
        CFDictionaryAddValue(expected, CFSTR("rekey"), n);
        CFRelease(n);

        bool retVal =
            ok(dict != NULL, "should have non-null dict") &&
            ok(CFEqual(expected, dict), "unexpected dict contents");

        CFRelease(expected);

        return retVal;
    }

private:
    SVOTelemetryReporter(const SVOTelemetryReporter&) = delete;
    SVOTelemetryReporter& operator=(const SVOTelemetryReporter&) = delete;

    CFDictionaryRef dict;

    friend class SVOTelemetryTester;
};

class SVOTelemetryTester : public SVOTelemetry {
public:
    SVOTelemetryTester(SVOTelemetryReporter& r);
    virtual ~SVOTelemetryTester();
private:
    SVOTelemetryTester(const SVOTelemetryTester&) = delete;
    SVOTelemetryTester& operator=(const SVOTelemetryTester&) = delete;

    SVOTelemetryReporter& r;
};

SVOTelemetryTester::SVOTelemetryTester(SVOTelemetryReporter& r) : r(r) {
    // reset the reporter, as we're reusing the reporter for multiple telemetry objects
    CFReleaseNull(r.dict);
}

SVOTelemetryTester::~SVOTelemetryTester() {
    ok(r.dict == NULL, "Expected NULL dict at start of dtor");
    r.dict = createTelemetryDictionary();
}

int kc_46_svotelemetry(int argc, char *const *argv)
{
    plan_tests(/* cases */ 10 * (/* test count per case */ SVOTelemetryReporter::kCount_test_validate + /* ok for the case */ 1)
               + 12 /* enum checks */);

    initializeKeychainTests(__FUNCTION__);

    // double check that there aren't any typos in the enums
    is(SVOTelemetry::initialKeybagUnlock_unlocked, 0);
    is(SVOTelemetry::initialKeybagUnlock_machError, 1);
    is(SVOTelemetry::initialKeybagUnlock_otherError, 2);
    is(SVOTelemetry::entropyUnlock_success, 0);
    is(SVOTelemetry::entropyUnlock_failure, 1);
    is(SVOTelemetry::entropyUnlock_notapplicable, 2);
    is(SVOTelemetry::passwordUnlock_success, 0);
    is(SVOTelemetry::passwordUnlock_failure, 1);
    is(SVOTelemetry::passwordUnlock_notapplicable, 2);
    is(SVOTelemetry::rekey_success, 0);
    is(SVOTelemetry::rekey_failure, 1);
    is(SVOTelemetry::rekey_notapplicable, 2);

    SVOTelemetryReporter r;

    {
        SVOTelemetryTester t(r);
    }
    ok(r.test_validate(SVOTelemetry::initialKeybagUnlock_otherError, 0, 0, SVOTelemetry::entropyUnlock_notapplicable, SVOTelemetry::passwordUnlock_notapplicable, SVOTelemetry::rekey_notapplicable, SVOTelemetry::entropyUnlock_notapplicable, errSecUnimplemented), "case: nothing set explicitly");

    {
        SVOTelemetryTester t(r);
        t.initialKeybagUnlock = SVOTelemetry::initialKeybagUnlock_machError;
        t.initialKeybagUnlockErrorCode = MIG_SERVER_DIED;
    }
    ok(r.test_validate(SVOTelemetry::initialKeybagUnlock_machError, MIG_SERVER_DIED, 0, SVOTelemetry::entropyUnlock_notapplicable, SVOTelemetry::passwordUnlock_notapplicable, SVOTelemetry::rekey_notapplicable, SVOTelemetry::entropyUnlock_notapplicable, errSecUnimplemented), "case: mig error MIG_SERVER_DIED encountered");

    {
        SVOTelemetryTester t(r);
        t.initialKeybagUnlock = SVOTelemetry::initialKeybagUnlock_unlocked;
    }
    ok(r.test_validate(SVOTelemetry::initialKeybagUnlock_unlocked, 0, 0, SVOTelemetry::entropyUnlock_notapplicable, SVOTelemetry::passwordUnlock_notapplicable, SVOTelemetry::rekey_notapplicable, SVOTelemetry::entropyUnlock_notapplicable, errSecUnimplemented), "case: no error unlocking the keybag");

    {
        SVOTelemetryTester t(r);
        t.initialKeybagUnlock = SVOTelemetry::initialKeybagUnlock_unlocked;
        t.cachedEntropyUnlockValue = SVOTelemetry::entropyUnlock_success;
    }
    ok(r.test_validate(SVOTelemetry::initialKeybagUnlock_unlocked, 0, 0, SVOTelemetry::entropyUnlock_success, SVOTelemetry::passwordUnlock_notapplicable, SVOTelemetry::rekey_notapplicable, SVOTelemetry::entropyUnlock_notapplicable, errSecUnimplemented), "case: no error unlocking using the cached entropy");

    // chrooted environment case: keybag unlock fails with MIG_BAD_ID, then unlocking with password succeeds
    {
        SVOTelemetryTester t(r);
        t.initialKeybagUnlock = SVOTelemetry::initialKeybagUnlock_machError;
        t.initialKeybagUnlockErrorCode = MIG_BAD_ID;
        t.passwordUnlockValue = SVOTelemetry::passwordUnlock_success;
    }
    ok(r.test_validate(SVOTelemetry::initialKeybagUnlock_machError, MIG_BAD_ID, 0, SVOTelemetry::entropyUnlock_notapplicable, SVOTelemetry::passwordUnlock_success, SVOTelemetry::rekey_notapplicable, SVOTelemetry::entropyUnlock_notapplicable, errSecUnimplemented), "case: keybag unlock throws mig bad id, but no error unlocking using the cached entropy");

    // this is the case of first installing this version: no cached entropy, password succeeds, rekey succeeds, associate succeeds
    {
        SVOTelemetryTester t(r);
        t.initialKeybagUnlock = SVOTelemetry::initialKeybagUnlock_unlocked;
        t.indirectUnlockKeyLookup = errSecItemNotFound;
        t.passwordUnlockValue = SVOTelemetry::passwordUnlock_success;
        t.rekeyValue = SVOTelemetry::rekey_success;
        t.indirectUnlockKeyAssociate = errSecSuccess;
    }
    ok(r.test_validate(SVOTelemetry::initialKeybagUnlock_unlocked, 0, errSecItemNotFound, SVOTelemetry::entropyUnlock_notapplicable, SVOTelemetry::passwordUnlock_success, SVOTelemetry::rekey_success, SVOTelemetry::entropyUnlock_notapplicable, errSecSuccess), "case: cached entropy not found, password succeeded, rekey succeeded, associate succeeded");

    // this is the post mac-to-mac migration case: no cached entropy, password fails, re-derive & unlock, associate succeeds
    {
        SVOTelemetryTester t(r);
        t.initialKeybagUnlock = SVOTelemetry::initialKeybagUnlock_unlocked;
        t.indirectUnlockKeyLookup = errSecItemNotFound;
        t.passwordUnlockValue = SVOTelemetry::passwordUnlock_failure;
        t.derivedEntropyUnlockValue = SVOTelemetry::entropyUnlock_success;
        t.indirectUnlockKeyAssociate = errSecSuccess;
    }
    ok(r.test_validate(SVOTelemetry::initialKeybagUnlock_unlocked, 0, errSecItemNotFound, SVOTelemetry::entropyUnlock_notapplicable, SVOTelemetry::passwordUnlock_failure, SVOTelemetry::rekey_notapplicable, SVOTelemetry::entropyUnlock_success, errSecSuccess), "case: cached entropy not found, password failed, unlock with newly derived entropy succeeded, associate succeeded");

    // case where dp keychain is not working, so lookup & associate fail
    {
        SVOTelemetryTester t(r);
        t.initialKeybagUnlock = SVOTelemetry::initialKeybagUnlock_unlocked;
        t.indirectUnlockKeyLookup = errSecInteractionNotAllowed;
        t.passwordUnlockValue = SVOTelemetry::passwordUnlock_failure;
        t.derivedEntropyUnlockValue = SVOTelemetry::entropyUnlock_success;
        t.indirectUnlockKeyAssociate = errSecInteractionNotAllowed;
    }
    ok(r.test_validate(SVOTelemetry::initialKeybagUnlock_unlocked, 0, errSecInteractionNotAllowed, SVOTelemetry::entropyUnlock_notapplicable, SVOTelemetry::passwordUnlock_failure, SVOTelemetry::rekey_notapplicable, SVOTelemetry::entropyUnlock_success, errSecInteractionNotAllowed), "case: dp lookup failed, unlock with newly derived entropy succeeded, associate failed");

    // case where the cached entropy is bad, the password fails to unlock the keychain, but rederivation succeeds
    {
        SVOTelemetryTester t(r);
        t.initialKeybagUnlock = SVOTelemetry::initialKeybagUnlock_unlocked;
        t.indirectUnlockKeyLookup = errSecSuccess;
        t.cachedEntropyUnlockValue = SVOTelemetry::entropyUnlock_failure;
        t.passwordUnlockValue = SVOTelemetry::passwordUnlock_failure;
        t.derivedEntropyUnlockValue = SVOTelemetry::entropyUnlock_success;
        t.indirectUnlockKeyAssociate = errSecSuccess;
    }
    ok(r.test_validate(SVOTelemetry::initialKeybagUnlock_unlocked, 0, 0, SVOTelemetry::entropyUnlock_failure, SVOTelemetry::passwordUnlock_failure, SVOTelemetry::rekey_notapplicable, SVOTelemetry::entropyUnlock_success, 0), "case: cached entropy bad, password fails, rederivation succeeds");

    // horrible hard failure case where rekeying fails
    {
        SVOTelemetryTester t(r);
        t.initialKeybagUnlock = SVOTelemetry::initialKeybagUnlock_unlocked;
        t.indirectUnlockKeyLookup = errSecItemNotFound;
        t.passwordUnlockValue = SVOTelemetry::passwordUnlock_success;
        t.rekeyValue = SVOTelemetry::rekey_failure;
    }
    ok(r.test_validate(SVOTelemetry::initialKeybagUnlock_unlocked, 0, errSecItemNotFound, SVOTelemetry::entropyUnlock_notapplicable, SVOTelemetry::passwordUnlock_success, SVOTelemetry::rekey_failure, SVOTelemetry::entropyUnlock_notapplicable, errSecUnimplemented), "case: rekeying fails");

    deleteTestFiles();
    return 0;
}
