//
//  SecCMSInternal.h
//  Security
//
//  WARNING: This header contains the shim functions for SecCMS using MessageSecurity.
//  It will be removed when the legacy implementations are removed.

#ifndef _SECURITY_SECCMS_INTERNAL_H_
#define _SECURITY_SECCMS_INTERNAL_H_

#include <Security/SecCMS.h>

__BEGIN_DECLS

/* Return an array of certificates contained in message, if message is of the
   type SignedData and has no signers, return NULL otherwise. */
CF_RETURNS_RETAINED CFArrayRef
MS_SecCMSCertificatesOnlyMessageCopyCertificates(CFDataRef message);

OSStatus MS_SecCMSVerifySignedData_internal(CFDataRef message, CFDataRef detached_contents,
                                            CFTypeRef policy, SecTrustRef CF_RETURNS_RETAINED *trustref, CFArrayRef additional_certs,
                                            CFDataRef CF_RETURNS_RETAINED *attached_contents, CFDictionaryRef CF_RETURNS_RETAINED *signed_attributes);

OSStatus MS_SecCMSDecodeSignedData(CFDataRef message,
                                   CFDataRef *attached_contents, CFDictionaryRef *signed_attributes);

OSStatus MS_SecCMSDecryptEnvelopedData(CFDataRef message, CFMutableDataRef data,
                                       SecCertificateRef *recipient);

__END_DECLS

#endif /* _SECURITY_SECCMS_INTERNAL_H_ */
