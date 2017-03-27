/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

/*
 * SecTrustOSXEntryPoints - Interface for unified SecTrust into OS X Security
 * Framework.
 */

#include "SecTrustOSXEntryPoints.h"

#include <Security/Security.h>
#include <Security/cssmtype.h>
#include <Security/SecKeychain.h>
#include <Security/SecItemPriv.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/SecCertificate.h>
#include <Security/SecImportExport.h>
#include <security_keychain/SecImportExportPem.h>
#include <security_utilities/debugging.h>
#include <Security/SecItemInternal.h>

#include <security_ocspd/ocspdClient.h>
#include <security_ocspd/ocspdUtils.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRunLoop.h>
#include <dispatch/dispatch.h>
#include <AssertMacros.h>
#include <pthread.h>
#include <notify.h>

/*
 * MARK: CFRunloop
 */

static void *SecTrustOSXCFRunloop(__unused void *unused) {
    CFRunLoopTimerRef timer = CFRunLoopTimerCreateWithHandler(kCFAllocatorDefault, (CFTimeInterval) UINT_MAX, 0, 0, 0, ^(__unused CFRunLoopTimerRef _timer) {
        /* do nothing */
    });

    /* add a timer to force the runloop to stay running */
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
    /* Register for CertificateTrustNotification */

    int out_token = 0;
    notify_register_dispatch(kSecServerCertificateTrustNotification, &out_token,
                             dispatch_get_main_queue(),
                             ^(int token __unused) {
                                 // Purge keychain parent cache
                                 SecItemParentCachePurge();
                                 // Purge unrestricted roots cache
                                 SecTrustSettingsPurgeUserAdminCertsCache();

                             });

    try {
        CFRunLoopRun();
    }
    catch (...) {
        /* An exception was rethrown from the runloop. Since we can't reliably
         * obtain info about changes to keychains or trust settings anymore,
         * just exit and respawn the process when needed. */

        secerror("Exception occurred in CFRunLoopRun; exiting");
        exit(0);
    }
    CFRelease(timer);
    return NULL;
}

void SecTrustLegacySourcesEventRunloopCreate(void) {
    /* A runloop is currently necessary to receive notifications about changes in the
     * legacy keychains and trust settings. */
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        pthread_attr_t attrs;
        pthread_t thread;

        pthread_attr_init(&attrs);
        pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);

        /* we do this with traditional pthread to avoid impacting our 512 WQ thread limit since this is a parked thread */
        pthread_create(&thread, &attrs, SecTrustOSXCFRunloop, NULL);
    });
}

/*
 * MARK: ocspd CRL Interface
 */
/* lengths of time strings without trailing NULL */
#define CSSM_TIME_STRLEN			14		/* no trailing 'Z' */
#define GENERALIZED_TIME_STRLEN		15

OSStatus SecTrustLegacyCRLStatus(SecCertificateRef cert, CFArrayRef chain, CFURLRef currCRLDP);
OSStatus SecTrustLegacyCRLFetch(CFURLRef currCRLDP, CFAbsoluteTime verifyTime);

static OSStatus cssmReturnToOSStatus(CSSM_RETURN crtn) {
    OSStatus status = errSecInternalComponent;

    switch (crtn) {
        case CSSM_OK:
            status = errSecSuccess;
            break;
        case CSSMERR_TP_CERT_REVOKED:
            status = errSecCertificateRevoked;
            break;
        case CSSMERR_APPLETP_NETWORK_FAILURE:
            status = errSecNetworkFailure;
            break;
        case CSSMERR_APPLETP_CRL_NOT_FOUND:
            status = errSecCRLNotFound;
            break;
        default:
            status = errSecInternalComponent;
    }
    return status;
}

#define PEM_STRING_X509		"CERTIFICATE"
static CFDataRef CF_RETURNS_RETAINED serializedPathToPemSequences(CFArrayRef certs) {
    CFMutableDataRef result = NULL;
    CFIndex certIX, certCount;
    require_quiet(certs, out);
    certCount = CFArrayGetCount(certs);
    require_quiet(certCount > 0, out);
    require_quiet(result = CFDataCreateMutable(NULL, 0), out);
    for (certIX = 0; certIX < certCount; certIX++) {
        CFDataRef certData = (CFDataRef)CFArrayGetValueAtIndex(certs, certIX);
        require_noerr_quiet(impExpPemEncodeExportRep(certData, PEM_STRING_X509,
                                                     NULL, result), out);
    }
out:
    return result;
}

OSStatus SecTrustLegacyCRLStatus(SecCertificateRef cert, CFArrayRef chain, CFURLRef currCRLDP) {
    OSStatus result = errSecParam;
    CSSM_RETURN crtn = CSSMERR_TP_INTERNAL_ERROR;
    CFDataRef serialData = NULL, pemIssuers = NULL, crlDP = NULL;
    CFMutableArrayRef issuersArray = NULL;

    if (!cert || !chain) {
        return result;
    }

    /* serialNumber is a CSSM_DATA with the value from the TBS Certificate. */
    CSSM_DATA serialNumber = { 0, NULL };
    serialData = SecCertificateCopySerialNumber(cert, NULL);
    if (serialData) {
        serialNumber.Data = (uint8_t *)CFDataGetBytePtr(serialData);
        serialNumber.Length = CFDataGetLength(serialData);
    }

    /* issuers is CSSM_DATA containing pem sequence of all issuers in the chain */
    CSSM_DATA issuers = { 0, NULL };
    issuersArray = CFArrayCreateMutableCopy(NULL, 0, chain);
    if (issuersArray) {
        CFArrayRemoveValueAtIndex(issuersArray, 0);
        pemIssuers = serializedPathToPemSequences(issuersArray);
    }
    if (pemIssuers) {
        issuers.Data = (uint8_t *)CFDataGetBytePtr(pemIssuers);
        issuers.Length = CFDataGetLength(pemIssuers);
    }

    /* crlUrl is CSSM_DATA with the CRLDP url*/
    CSSM_DATA crlUrl = { 0, NULL };
    crlDP = CFURLCreateData(NULL, currCRLDP, kCFStringEncodingASCII, true);
    if (crlDP) {
        crlUrl.Data = (uint8_t *)CFDataGetBytePtr(crlDP);
        crlUrl.Length = CFDataGetLength(crlDP);
    }

    if (serialNumber.Data && issuers.Data && crlUrl.Data) {
        crtn = ocspdCRLStatus(serialNumber, issuers, NULL, &crlUrl);
    }

    result = cssmReturnToOSStatus(crtn);

    if (serialData) { CFRelease(serialData); }
    if (issuersArray) { CFRelease(issuersArray); }
    if (pemIssuers) { CFRelease(pemIssuers); }
    if (crlDP) { CFRelease(crlDP); }
    return result;
}

static CSSM_RETURN ocspdCRLFetchToCache(const CSSM_DATA		&crlURL,
                                 CSSM_TIMESTRING 	verifyTime) {
    Allocator &alloc(Allocator::standard(Allocator::normal));
    CSSM_DATA crlData  = { 0, NULL };
    CSSM_RETURN crtn;

    crtn = ocspdCRLFetch(alloc, crlURL, NULL, true, true, verifyTime, crlData);
    if (crlData.Data) { alloc.free(crlData.Data); }
    return crtn;
}

static OSStatus fetchCRL(CFURLRef currCRLDP, CFAbsoluteTime verifyTime) {
    OSStatus result = errSecParam;
    CSSM_RETURN crtn = CSSMERR_TP_INTERNAL_ERROR;
    CFDataRef crlDP = NULL;
    char *cssmTime = NULL, *genTime = NULL;

    if (!currCRLDP) {
        return result;
    }

    /* crlUrl is CSSM_DATA with the CRLDP url*/
    CSSM_DATA crlUrl = { 0, NULL };
    crlDP = CFURLCreateData(NULL, currCRLDP, kCFStringEncodingASCII, true);
    if (crlDP) {
        crlUrl.Data = (uint8_t *)CFDataGetBytePtr(crlDP);
        crlUrl.Length = CFDataGetLength(crlDP);
    }

    /* determine verification time */
    cssmTime = (char *)malloc(CSSM_TIME_STRLEN + 1);
    genTime = (char *)malloc(GENERAL_TIME_STRLEN + 1);
    if (cssmTime && genTime) {
        if (verifyTime != 0.0) {
            cfAbsTimeToGgenTime(verifyTime, genTime);
        } else {
            cfAbsTimeToGgenTime(CFAbsoluteTimeGetCurrent(), genTime);
        }
        memmove(cssmTime, genTime, GENERAL_TIME_STRLEN - 1);    // don't copy the Z
        cssmTime[CSSM_TIME_STRLEN] = '\0';
    }

    if (crlUrl.Data && cssmTime) {
       crtn = ocspdCRLFetchToCache(crlUrl, (CSSM_TIMESTRING)cssmTime);
    }

    result = cssmReturnToOSStatus(crtn);

    if (crlDP) { CFRelease(crlDP); }
    if (cssmTime) { free(cssmTime); }
    if (genTime) { free(genTime); }
    return result;
}

/*
 * MARK: async_ocspd methods
 */
static void async_ocspd_complete(async_ocspd_t *ocspd) {
    if (ocspd->completed) {
        ocspd->completed(ocspd);
    }
}

/* Return true, iff we didn't schedule any work, return false if we did. */
bool SecTrustLegacyCRLFetch(async_ocspd_t *ocspd,
                       CFURLRef currCRLDP, CFAbsoluteTime verifyTime,
                       SecCertificateRef cert, CFArrayRef chain) {
    dispatch_async(ocspd->queue, ^ {
        OSStatus status = fetchCRL(currCRLDP, verifyTime);
        switch (status) {
            case errSecSuccess:
                ocspd->response= SecTrustLegacyCRLStatus(cert, chain, currCRLDP);
                break;
            default:
                ocspd->response = status;
                break;
        }
        async_ocspd_complete(ocspd);
        if (chain) { CFRelease(chain); }
    });

    return false; /* false -> something was scheduled. */
}
