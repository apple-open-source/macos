//
//  SecAppleCorporateAnchors.c
//  Security
//
//

#include <AssertMacros.h>
#include "SecAppleCorporateAnchors.h"
#include "AppleCorporateRootCertificates.h"
#include <Security/SecCertificatePriv.h>

// Assigns NULL to CF. Releases the value stored at CF unless it was NULL.  Always returns NULL, for your convenience
#define CFReleaseNull(CF) ({ __typeof__(CF) *const _pcf = &(CF), _cf = *_pcf; (_cf ? (*_pcf) = ((__typeof__(CF))0), (CFRelease(_cf), ((__typeof__(CF))0)) : _cf); })


// README: See AppleCorporateRootCertificates.h for instructions for adding new corporate roots
CFArrayRef SecCertificateCopyAppleCorporateRoots(void) {
    CFMutableArrayRef result = NULL;
    SecCertificateRef corp1 = NULL, corp2 = NULL, corp3 = NULL;

    require_quiet(corp1= SecCertificateCreateWithBytes(NULL, _AppleCorporateRootCA, sizeof(_AppleCorporateRootCA)),
                  errOut);
    require_quiet(corp2 = SecCertificateCreateWithBytes(NULL, _AppleCorporateRootCA2,
                                                                          sizeof(_AppleCorporateRootCA2)),
                  errOut);
    require_quiet(corp3 = SecCertificateCreateWithBytes(NULL, _AppleCorporateRootCA3,
                                                                          sizeof(_AppleCorporateRootCA3)),
                  errOut);

    require_quiet(result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks), errOut);
    CFArrayAppendValue(result, corp1);
    CFArrayAppendValue(result, corp2);
    CFArrayAppendValue(result, corp3);

errOut:
    CFReleaseNull(corp1);
    CFReleaseNull(corp2);
    CFReleaseNull(corp3);
    return result;
}
