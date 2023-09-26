//
//  app-id.c
//  Security
//

#import "app-id.h"

#import <stdio.h>
#import <CoreFoundation/CFDictionary.h>
#import <CoreFoundation/CFString.h>
#import <Security/SecItem.h>

int test_application_identifier(int argc, char * const *argv) {

    const void *keys[] = {kSecClass, kSecAttrService, kSecUseDataProtectionKeychain};
    const void *values[] = {kSecClassGenericPassword, NULL, kCFBooleanTrue};

    values[1] = CFStringCreateWithCStringNoCopy(NULL, "should-not-exist-testing-only", kCFStringEncodingUTF8, kCFAllocatorNull);

    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values, 3,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    OSStatus status = SecItemCopyMatching(query, NULL);

    fprintf(stderr, "%d\n", (int)status);

    return status == errSecSuccess;
}
