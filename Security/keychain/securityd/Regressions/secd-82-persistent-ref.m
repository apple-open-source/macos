//
//  secd-82-persistent-ref.c
//  sec
//
//  Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
//
//

#include <Security/Security.h>
#include <Security/SecItemPriv.h>
#include <utilities/SecCFWrappers.h>

#include "secd_regressions.h"
#include "SecdTestKeychainUtilities.h"

int secd_82_persistent_ref(int argc, char *const *argv)
{
    plan_tests(5);

    /* custom keychain dir */
    secd_test_setup_temp_keychain("secd_82_persistent_ref", NULL);

    CFMutableDictionaryRef		attrs = NULL;
    CFMutableDictionaryRef		query = NULL;
    CFDataRef					data = NULL;
    CFTypeRef					result = NULL;
    CFArrayRef					array = NULL;
    CFIndex						i, n;
    CFDictionaryRef				item = NULL;

    attrs = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
    CFDictionarySetValue( attrs, kSecClass, kSecClassGenericPassword );
    CFDictionarySetValue( attrs, kSecAttrAccessible, kSecAttrAccessibleAlwaysPrivate );
    CFDictionarySetValue( attrs, kSecAttrLabel, CFSTR( "TestLabel" ) );
    CFDictionarySetValue( attrs, kSecAttrDescription, CFSTR( "TestDescription" ) );
    CFDictionarySetValue( attrs, kSecAttrAccount, CFSTR( "TestAccount" ) );
    CFDictionarySetValue( attrs, kSecAttrService, CFSTR( "TestService" ) );
    data = CFDataCreate( NULL, (const uint8_t *) "\x00\x01\x02", 3 );
    CFDictionarySetValue( attrs, kSecValueData, data );
    CFReleaseNull( data );

    is(SecItemAdd(attrs, NULL), errSecSuccess);
    CFReleaseNull( attrs );

    query = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
    CFDictionarySetValue( query, kSecClass, kSecClassGenericPassword );
    CFDictionarySetValue( query, kSecAttrSynchronizable, kSecAttrSynchronizableAny );;
    CFDictionarySetValue( query, kSecAttrAccount, CFSTR( "TestAccount" ) );
    CFDictionarySetValue( query, kSecReturnAttributes, kCFBooleanTrue );
    CFDictionarySetValue( query, kSecReturnPersistentRef, kCFBooleanTrue );
    CFDictionarySetValue( query, kSecMatchLimit, kSecMatchLimitAll );

    is(SecItemCopyMatching(query, &result), errSecSuccess);
    CFReleaseNull( query );
    array = (CFArrayRef) result;

    SKIP: {
        skip("No results from SecItemCopyMatching", 2, array && (CFArrayGetTypeID() == CFGetTypeID(array)));
        n = CFArrayGetCount( array );
        is(n, 1);
        for( i = 0; i < n; ++i )
        {
            item = (CFDictionaryRef) CFArrayGetValueAtIndex(array, i);

            ok((CFDataRef) CFDictionaryGetValue(item, kSecValuePersistentRef));
        }
    }
    CFReleaseNull( result );

    secd_test_teardown_delete_temp_keychain("secd_82_persistent_ref");

    return 0;
}
