//
//  secd-67-prefixedKeyIDs.c
//  Security
//
//  Created by Richard Murphy on 11/1/16.
//
//

#include <stdio.h>
//
//  secd-66-account-recovery.c
//  Security
//
//  Created by Richard Murphy on 10/5/16.
//
//

#include <stdio.h>

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
#include <CoreFoundation/CFDictionary.h>
#include "SOSKeyedPubKeyIdentifier.h"

#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"

#include <utilities/SecCFWrappers.h>
#include <Security/SecKeyPriv.h>



#include "SOSAccountTesting.h"
static void tests() {
    CFErrorRef error = NULL;
    int keySizeInBits = 256;
    CFNumberRef kzib = CFNumberCreate(NULL, kCFNumberIntType, &keySizeInBits);

    CFDictionaryRef keyattributes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                                 kSecAttrKeyType, kSecAttrKeyTypeEC,
                                                                 kSecAttrKeySizeInBits, kzib,
                                                                 NULL);
    CFReleaseNull(kzib);


    SecKeyRef key = SecKeyCreateRandomKey(keyattributes, &error);
    CFReleaseNull(keyattributes);
    SecKeyRef pubKey = SecKeyCreatePublicFromPrivate(key);
    CFDataRef pubKeyData = NULL;
    SecKeyCopyPublicBytes(pubKey, &pubKeyData);
    CFStringRef properPref = CFSTR("RK");
    CFStringRef shortPref = CFSTR("R");
    CFStringRef longPref = CFSTR("RKR");
    
    ok(key, "Made private key");
    ok(pubKey, "Made public key");
    ok(pubKeyData, "Made public key data");
    
    CFStringRef pkidseckey = SOSKeyedPubKeyIdentifierCreateWithSecKey(properPref, pubKey);
    ok(pkidseckey, "made string");
    CFStringRef pkidseckeyshort = SOSKeyedPubKeyIdentifierCreateWithSecKey(shortPref, pubKey);
    ok(!pkidseckeyshort, "didn't make string");
    CFStringRef pkidseckeylong = SOSKeyedPubKeyIdentifierCreateWithSecKey(longPref, pubKey);
    ok(!pkidseckeylong, "didn't make string");
    
    ok(SOSKeyedPubKeyIdentifierIsPrefixed(pkidseckey), "properly prefixed string was made");
    CFStringRef retPref = SOSKeyedPubKeyIdentifierCopyPrefix(pkidseckey);
    ok(retPref, "got prefix");
    ok(CFEqualSafe(retPref, properPref), "prefix matches");
    CFReleaseNull(retPref);
    CFStringRef retHpub = SOSKeyedPubKeyIdentifierCopyHpub(pkidseckey);
    ok(retHpub, "got hash of pubkey");
    
    
    CFStringRef pkiddata = SOSKeyedPubKeyIdentifierCreateWithData(properPref, pubKeyData);
    ok(pkiddata, "made string");
    ok(CFEqualSafe(pkiddata, pkidseckey), "strings match");
    
    diag("pkiddata %@", pkiddata);
    diag("retPref %@", retPref);
    CFReleaseNull(retHpub);
    CFReleaseNull(key);
    CFReleaseNull(pubKey);
    CFReleaseNull(pubKeyData);
    CFReleaseNull(pkidseckey);
    CFReleaseNull(pkidseckeyshort);
    CFReleaseNull(pkidseckeylong);
    CFReleaseNull(pkiddata);


}

int secd_67_prefixedKeyIDs(int argc, char *const *argv) {
    plan_tests(12);

    tests();
    return 0;
}

