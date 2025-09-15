//
//  IndirectUnlock.c
//  security2tool_macos
//

#import "IndirectUnlock.h"
#import <stdio.h>
#import <CoreFoundation/CoreFoundation.h>
#import <Security/SecItemPriv.h>
#import <Security/SecKeychainPriv.h>

int lookup_indirect_unlock_key(int argc, char * const *argv) {
    uint32_t handle = 0;
    CFStringRef ident = NULL;

    if (argc > 1) {
        ident = CFStringCreateWithCString(NULL, argv[1], kCFStringEncodingUTF8);
    } else {
        fprintf(stderr, "Must specify identifier\n");
        return 1;
    }

    OSStatus status = _SecLookupIndirectUnlockKey(ident, &handle);
    CFRelease(ident);

    if (status == noErr) {
        printf("handle: %u\n", handle);
    } else {
        fprintf(stderr, "Failed to lookup indirect unlock key: %d\n", (int)status);
        return 1;
    }

    return 0;
}

int release_indirect_unlock_key_handle(int argc, char * const *argv) {
    uint32_t handle = 0;

    if (argc > 1) {
        handle = atoi(argv[1]);
    } else {
        fprintf(stderr, "Must specify handle\n");
        return 1;
    }

    OSStatus status = SecKeychainReleaseIndirectUnlockHandle(handle);

    if (status != noErr) {
        fprintf(stderr, "Failed to release: %d\n", (int)status);
        return 1;
    }

    return 0;
}
