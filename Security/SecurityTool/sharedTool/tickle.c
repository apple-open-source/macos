//
//  tickle.c
//  Security
//

#include "tickle.h"

#import <Security/SecItemPriv.h>

int tickle(int argc, char * const *argv) {
    OSStatus status = _SecKeychainForceUpgradeIfNeeded();

    if (status != noErr) {
        fprintf(stderr, "Failed to tickle keychain: %d", (int)status);
        return 1;
    }

    return 0;
}
