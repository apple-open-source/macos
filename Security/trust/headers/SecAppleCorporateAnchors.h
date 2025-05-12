//
//  SecAppleCoporateAnchors.h
//  Security
//
//

#ifndef _SECURITY_SEC_CORPORATE_ANCHORS_H_
#define _SECURITY_SEC_CORPORATE_ANCHORS_H_

#include <CoreFoundation/CFArray.h>

__BEGIN_DECLS

/* Return the Apple Corporate Roots (for use with SecTrustSetAnchorCertificates or SecTrustStore/SecTrustSettings) */
CFArrayRef SecCertificateCopyAppleCorporateRoots(void)
    API_AVAILABLE(macos(14.4), ios(17.4), watchos(10.4), tvos(17.4));

__END_DECLS

#endif /* _SECURITY_SEC_CORPORATE_ANCHORS_H_ */
