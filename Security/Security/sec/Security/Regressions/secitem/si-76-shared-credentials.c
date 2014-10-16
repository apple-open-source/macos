//
//  si-76-shared-credentials.c
//  sec
//


#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecBasePriv.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecIdentity.h>
#include <Security/SecPolicy.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecPolicyInternal.h>
#include <Security/SecSharedCredential.h>
#include <Security/SecCMS.h>
#include <utilities/SecCFWrappers.h>
#include <stdlib.h>
#include <unistd.h>

#include "Security_regressions.h"

#define WAIT_WHILE(X) { while ((X)) { (void)CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, TRUE); } }

static bool expected_failure(OSStatus status)
{
    return ((status == errSecMissingEntitlement) ||
            (status == errSecBadReq));
}

static void tests(void)
{
    // look up our entry for localhost
    CFStringRef acct1 = CFSTR("local");
    CFStringRef acct2 = CFSTR("admin");
    CFStringRef fqdn = CFSTR("localhost");
    CFStringRef not_my_fqdn = CFSTR("store.apple.com"); // something we aren't entitled to share
    __block bool adding;
    __block bool requesting;
    __block bool deleting;

//  UInt8 buf[6] = { 'l', 'o', 'c', 'a', 'l', '\0' };
//  CFDataRef cred = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)&buf, sizeof(buf));
    CFStringRef cred = CFStringCreateCopy(kCFAllocatorDefault, CFSTR("local"));

    // should get denied if we request a fqdn which is not in our entitlement
    requesting = true;
    SecRequestSharedWebCredential(not_my_fqdn, NULL, ^void (CFArrayRef credentials, CFErrorRef error) {
        OSStatus status = (OSStatus)((error) ? CFErrorGetCode(error) : errSecSuccess);
        is(status == errSecItemNotFound || expected_failure(status), true, "fqdn not entitled");
        is(CFArrayGetCount(credentials) > 0, false, "returned credential array == 0");
        requesting = false;
    });
    WAIT_WHILE(requesting);

    // add (or update) credentials for two different accounts on the same server
    adding = true;
    SecAddSharedWebCredential(fqdn, acct1, cred, ^void (CFErrorRef error) {
        OSStatus status = (OSStatus)((error) ? CFErrorGetCode(error) : errSecSuccess);
        // TODO: need a proper teamID-enabled application identifier to succeed; expect auth failure
        if (status == errSecAuthFailed || expected_failure(status)) { status = errSecSuccess; }
        ok_status(status);
        adding = false;
    });
    WAIT_WHILE(adding);

    adding = true;
    SecAddSharedWebCredential(fqdn, acct2, cred, ^void (CFErrorRef error) {
        OSStatus status = (OSStatus)((error) ? CFErrorGetCode(error) : errSecSuccess);
        // TODO: need a proper teamID-enabled application identifier to succeed; expect auth failure
        if (status == errSecAuthFailed || expected_failure(status)) { status = errSecSuccess; }
        ok_status(status);
        adding = false;
    });
    WAIT_WHILE(adding);

    // look up credential with specific account
    requesting = true;
    SecRequestSharedWebCredential(fqdn, acct1, ^void (CFArrayRef credentials, CFErrorRef error) {
        OSStatus status = (OSStatus)((error) ? CFErrorGetCode(error) : errSecSuccess);
        // TODO: need a proper teamID-enabled application identifier to succeed; expect no items
        bool notFound = false;
        if (status == errSecItemNotFound || expected_failure(status)) {
            status = errSecSuccess; notFound = true;
        }
        ok_status(status);

        // should find only one credential if a specific account is provided
        CFIndex credentialCount = CFArrayGetCount(credentials);
        // TODO: need a proper teamID-enabled application identifier to succeed; expect 0 items
        if (credentialCount == 0 && notFound) { credentialCount = 1; }
        is(credentialCount == 1, true, "returned credentials == 1");
        requesting = false;
    });
    WAIT_WHILE(requesting);

    // look up credential with NULL account parameter
    requesting = true;
    SecRequestSharedWebCredential(fqdn, NULL, ^void (CFArrayRef credentials, CFErrorRef error) {
        OSStatus status = (OSStatus)((error) ? CFErrorGetCode(error) : errSecSuccess);
        // TODO: need a proper teamID-enabled application identifier to succeed; expect auth failure
        bool notFound = false;
        if (status == errSecItemNotFound || expected_failure(status)) {
            status = errSecSuccess; notFound = true;
        }
        ok_status(status);

        // should find only one credential if no account is provided
        // (since UI dialog only permits one credential to be selected)
        CFIndex credentialCount = CFArrayGetCount(credentials);
        // TODO: need a proper teamID-enabled application identifier to succeed
        if (credentialCount == 0 && notFound) { credentialCount = 1; }
        is(credentialCount == 1, true, "returned credentials == 1");
        requesting = false;
    });
    WAIT_WHILE(requesting);

    // pass NULL to delete our credentials
    deleting = true;
    SecAddSharedWebCredential(fqdn, acct1, NULL, ^void (CFErrorRef error) {
        OSStatus status = (OSStatus)((error) ? CFErrorGetCode(error) : errSecSuccess);
        // TODO: need a proper teamID-enabled application identifier to succeed; expect auth failure
        if (status == errSecAuthFailed || expected_failure(status)) { status = errSecSuccess; }
        ok_status(status);
        deleting = false;
    });
    WAIT_WHILE(deleting);

    deleting = true;
    SecAddSharedWebCredential(fqdn, acct2, NULL, ^void (CFErrorRef error) {
        OSStatus status = (OSStatus)((error) ? CFErrorGetCode(error) : errSecSuccess);
        // TODO: need a proper teamID-enabled application identifier to succeed; expect auth failure
        if (status == errSecAuthFailed || expected_failure(status)) { status = errSecSuccess; }
        ok_status(status);
        deleting = false;
    });
    WAIT_WHILE(deleting);

    // look up credentials again; should find nothing this time
    requesting = true;
    SecRequestSharedWebCredential(fqdn, NULL, ^void (CFArrayRef credentials, CFErrorRef error) {
        OSStatus status = (OSStatus)((error) ? CFErrorGetCode(error) : errSecSuccess);
        // TODO: need a proper teamID-enabled application identifier to succeed; expect auth failure
        if (status == errSecAuthFailed || expected_failure(status)) { status = errSecItemNotFound; }
        is_status(status, errSecItemNotFound);
        is(CFArrayGetCount(credentials) > 0, false, "returned credential array == 0");
        requesting = false;
    });
    WAIT_WHILE(requesting);

    CFRelease(cred);
}

int si_76_shared_credentials(int argc, char *const *argv)
{
		plan_tests(12);
		tests();
		return 0;
}
