//
//  SecCertificateFuzzer.c
//  Security
//

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/SecCertificatePriv.h>
#include "SecCertificateFuzzer.h"

extern bool SecCertificateHasOCSPNoCheckMarkerExtension(SecCertificateRef certificate);

int
SecCertificateFuzzer(const void *data, size_t len)
{
    CFDataRef d = CFDataCreateWithBytesNoCopy(NULL, data, len, kCFAllocatorNull);
    if (d) {
        SecCertificateRef cert = SecCertificateCreateWithData(NULL, d);
        CFRelease(d);
        if (cert) {
            CFStringRef summary = SecCertificateCopySubjectSummary(cert);
            if (summary) {
                CFRelease(summary);
            }
            CFArrayRef properties = SecCertificateCopyProperties(cert);
            if (properties) {
                CFRelease(properties);
            }
            CFArrayRef country = SecCertificateCopyCountry(cert);
            if (country) {
                CFRelease(country);
            }
            CFStringRef subject = SecCertificateCopySubjectString(cert);
            if (subject) {
                CFRelease(subject);
            }
            CFDataRef issuer = SecCertificateCopyIssuerSequence(cert);
            if (issuer) {
                CFRelease(issuer);
            }
            CFDataRef precert = SecCertificateCopyPrecertTBS(cert);
            if (precert) {
                CFRelease(precert);
            }
            (void)SecCertificateHasOCSPNoCheckMarkerExtension(cert);
            CFRelease(cert);
        }
    }

    return 0;
}
