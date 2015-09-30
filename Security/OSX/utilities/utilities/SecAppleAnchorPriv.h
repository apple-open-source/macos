//
//  utilities
//
//  Copyright © 2015 Apple Inc. All rights reserved.
//

#ifndef SecAppleAnchor_c
#define SecAppleAnchor_c

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

typedef CF_OPTIONS(uint32_t, SecAppleTrustAnchorFlags) {
    kSecAppleTrustAnchorFlagsIncludeTestAnchors    = 1 << 0,
};

/*
 * Return true if the certificate is an the Apple Trust anchor.
 */
bool
SecIsAppleTrustAnchor(SecCertificateRef cert,
                      SecAppleTrustAnchorFlags flags);

bool
SecIsAppleTrustAnchorData(CFDataRef cert,
			  SecAppleTrustAnchorFlags flags);

__END_DECLS


#endif /* SecAppleAnchor */
