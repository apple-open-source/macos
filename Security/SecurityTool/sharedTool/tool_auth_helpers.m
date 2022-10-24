//
//  tool_auth_helpers.m
//  Security
//

#import <Foundation/Foundation.h>

#if !TARGET_OS_TV && !TARGET_OS_BRIDGE
#include <readpassphrase.h>
#import <LocalAuthentication/LAContext+Private.h>
#include "debugging.h"
#endif // !TARGET_OS_TV && !TARGET_OS_BRIDGE

#include <os/feature_private.h>

#include "tool_auth_helpers.h"

#if !TARGET_OS_TV && !TARGET_OS_BRIDGE
static bool passphraseIsCorrect(const char * const passphrase) {
    NSData* data = [NSData dataWithBytesNoCopy:(void*)passphrase length:strlen(passphrase) freeWhenDone:NO];
    LAContext* la = [[LAContext alloc] init];
    NSError* error = nil;
    BOOL good = [la setCredential:data type:LACredentialTypePasscode error:&error];
    if (good) {
        return true;
    }

    if (error.code != LAErrorAuthenticationFailed) {
        NSLog(@"error attempting auth: %@", error);
    }
    return false;
}
#endif // !TARGET_OS_TV && !TARGET_OS_BRIDGE

// if bufLen == 0, use strlen to calculate size to wipe
bool checkPassphrase(char* passphrase, rsize_t len) {
#if TARGET_OS_TV || TARGET_OS_BRIDGE
    fprintf(stderr, "Authentication unsupported on this platform! Continuing w/o auth.\n");
    return true;
#else
    bool good = passphrase && passphraseIsCorrect(passphrase);
    if (len == 0) {
        len = passphrase ? strlen(passphrase) : 0;
    }
    memset_s(passphrase, len, 0, len);
    if (!good) {
        fprintf(stderr, "Authentication failed!\n");
    }
    return good;
#endif // TARGET_OS_TV || TARGET_OS_BRIDGE
}

bool promptForAndCheckPassphrase(void) {
#if TARGET_OS_TV || TARGET_OS_BRIDGE
    fprintf(stderr, "Authentication unsupported on this platform! Continuing w/o auth.\n");
    return true;
#else
    char buf[1024];
    char* pw = readpassphrase("Passcode: ", buf, sizeof(buf), RPP_REQUIRE_TTY);
    return checkPassphrase(pw, sizeof(buf));
#endif // !TARGET_OS_TV || TARGET_OS_BRIDGE
}

bool authRequired(void) {
#if TARGET_OS_TV || TARGET_OS_BRIDGE
    return false;
#else
    if (os_feature_enabled(Security, SecSkipSecurityToolAuth)) {
        fprintf(stderr,
                "WARNING! Authentication skipped. It is required starting in iOS 17.0 / macOS 14.0.\n"
                "Please add \"-y\" to be prompted for authentication, or \"-Y passcode\" to specify on the command line.\n");
        if (!os_feature_enabled(Security, SecSkipSecurityToolAuthSimCrash)) {
#if TARGET_OS_IOS
            fprintf(stderr, "A simulated crash has been generated.\n");
#endif
            __security_simulatecrash(CFSTR("security tool: auth skipped"), __sec_exception_code_ToolAuthReqd);
        }
        return false;
    } else {
        fprintf(stderr, "Authentication required!\n");
        return true;
    }
#endif // TARGET_OS_TV || TARGET_OS_BRIDGE
}
