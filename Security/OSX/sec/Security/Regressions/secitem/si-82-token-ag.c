//
//  si-82-token-ag.c
//  Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
//
//

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecBase.h>
#include <utilities/array_size.h>
#include <utilities/SecCFWrappers.h>

#include "Security_regressions.h"

static void tests(void) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(dict, kSecClass, kSecClassGenericPassword);
    CFDictionaryAddValue(dict, kSecAttrService, CFSTR("test"));
    CFDictionaryAddValue(dict, kSecAttrAccessGroup, kSecAttrAccessGroupToken);

    ok_status(SecItemAdd(dict, NULL));
    ok_status(SecItemDelete(dict));

    CFRelease(dict);
}

int si_82_token_ag(int argc, char *const *argv) {

    plan_tests(2);
    tests();
    return 0;
}
