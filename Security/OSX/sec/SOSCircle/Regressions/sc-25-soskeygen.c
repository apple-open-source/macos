//
//  sc-25-soskeygen.c
//  sec
//
//  Created by Richard Murphy on 6/1/15.
//
//

/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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



#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>

#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSUserKeygen.h>

#include <utilities/SecCFWrappers.h>

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <unistd.h>
#include "SOSCircle_regressions.h"

#include "SOSRegressionUtilities.h"


#if TARGET_OS_WATCH
#define NPARMS 3
#define NKEYS 3
#else
#define NPARMS 10
#define NKEYS 10
#endif

static int kTestTestCount = (NKEYS*(4+NPARMS*4));


static SecKeyRef createTestKey(CFDataRef cfpassword, CFDataRef parameters, CFErrorRef *error) {
    SecKeyRef user_privkey = SOSUserKeygen(cfpassword, parameters, error);
    ok(user_privkey, "No key!");
    ok(*error == NULL, "Error: (%@)", *error);
    CFReleaseNull(*error);
    return user_privkey;
}

static void tests(void) {
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    
    for(int j=0; j < NPARMS; j++) {
        CFDataRef parameters = SOSUserKeyCreateGenerateParameters(&error);
        ok(parameters, "No parameters!");
        ok(error == NULL, "Error: (%@)", error);
        CFReleaseNull(error);

        SecKeyRef baseline_privkey = createTestKey(cfpassword, parameters, &error);
        if(baseline_privkey) {
            SecKeyRef baseline_pubkey = SecKeyCreatePublicFromPrivate(baseline_privkey);

            for(int i = 0; i < NKEYS; i++) {
                SecKeyRef user_privkey = createTestKey(cfpassword, parameters, &error);
                SecKeyRef user_pubkey = SecKeyCreatePublicFromPrivate(user_privkey);
                ok(CFEqualSafe(baseline_privkey, user_privkey), "Private Keys Don't Match");
                ok(CFEqualSafe(baseline_pubkey, user_pubkey), "Public Keys Don't Match");
                CFReleaseNull(error);
                CFReleaseNull(user_privkey);
                CFReleaseNull(user_pubkey);
            }
            CFReleaseNull(baseline_pubkey);
        }
        CFReleaseNull(baseline_privkey);
        CFReleaseNull(parameters);
    }
    CFReleaseNull(cfpassword);
}

int sc_25_soskeygen(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    tests();
    
    return 0;
}
