//
//  si-26-cms-apple-signed-samples.m
//  SharedRegressions
//
//

#import <Foundation/Foundation.h>
#include <Security/Security.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/CMSDecoder.h>
#include <utilities/SecCFRelease.h>

#include "shared_regressions.h"
#include "si-26-cms-apple-signed-samples.h"

static void tests(void)
{
    SecPolicyRef policy = NULL;

    /* Create Mac Provisioning Profile policy instance. */
    isnt(policy = SecPolicyCreateOSXProvisioningProfileSigning(), NULL, "create policy");

    /* Verify signed content with this policy. */
    CMSDecoderRef decoder = NULL;
    CMSSignerStatus signerStatus = kCMSSignerInvalidIndex;
    OSStatus verifyResult = 0;
    ok_status(CMSDecoderCreate(&decoder),
              "create decoder");
    ok_status(CMSDecoderUpdateMessage(decoder, _TestProvisioningProfile, sizeof(_TestProvisioningProfile)),
              "update message");
    ok_status(CMSDecoderFinalizeMessage(decoder),
              "finalize message");
    ok_status(CMSDecoderCopySignerStatus(decoder, 0, policy, true, &signerStatus, NULL, &verifyResult),
              "copy signer status");
    is(signerStatus, kCMSSignerValid, "signer status valid");
    is(verifyResult, errSecSuccess, "verify result valid");

    CFReleaseSafe(decoder);
    CFReleaseSafe(policy);
}

int si_26_cms_apple_signed_samples(int argc, char *const *argv)
{
    plan_tests(7);

    tests();

    return 0;
}
