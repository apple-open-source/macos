//
//  SecCertificateFuzzer.c
//  Security
//

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include "SecCertificateFuzzer.h"

int
SecCertificateFuzzer(const void *data, size_t len)
{
    CFDataRef d = CFDataCreateWithBytesNoCopy(NULL, data, len, kCFAllocatorNull);
    if (d) {
        SecCertificateRef cert = SecCertificateCreateWithData(NULL, d);
        CFRelease(d);
        if (cert) {
            CFRelease(cert);
        }
    }

    return 0;
}
