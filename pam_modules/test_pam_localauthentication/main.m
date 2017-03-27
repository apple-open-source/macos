//
//  main.m
//  test_pam_localauthentication
//
//  Created by Jiri Margaritov on 25/11/15.
//
//

#import <Foundation/Foundation.h>

#include <security/pam_appl.h>
#include <security/openpam.h>
#import <LocalAuthentication/LAContext+Private.h>

int main(int argc, const char * argv[]) {
    int pam_res = PAM_SYSTEM_ERR;

    @autoreleasepool {
        pam_handle_t *pamh = NULL;
        struct pam_conv pamc = { openpam_nullconv, NULL };
        LAContext *context = nil;
        CFDataRef econtext = NULL;
        const char *username = NULL;

        if (argc > 1) {
            username = argv[1];
        }

        if (!username)
            goto cleanup;

        if (PAM_SUCCESS != (pam_res = pam_start("localauthentication", username, &pamc, &pamh)))
            goto cleanup;

        context = [LAContext new];
        econtext = CFDataCreate(kCFAllocatorDefault, context.externalizedContext.bytes, context.externalizedContext.length);

        if (!econtext)
            goto cleanup;

        if (PAM_SUCCESS != (pam_res = pam_set_data(pamh, "token_la", (void *)&econtext, NULL)))
            goto cleanup;

        if (PAM_SUCCESS != (pam_res = pam_authenticate(pamh, 0)))
            goto cleanup;
        
        if (PAM_SUCCESS != (pam_res = pam_acct_mgmt(pamh, 0)))
            goto cleanup;

cleanup:
        if (pamh) {
            pam_end(pamh, pam_res);
        }
        
        if (econtext) {
            CFRelease(econtext);
        }
    }

    return pam_res;
}
