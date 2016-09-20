//
//  SecServerEncryptionSupport.h
//
//

#ifndef _SECURITY_SECSERVERENCRYPTIONSUPPORT_H_
#define _SECURITY_SECSERVERENCRYPTIONSUPPORT_H_

#include <Availability.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecKey.h>
#include <Security/SecTrust.h>

CFDataRef SecCopyEncryptedToServer(SecTrustRef trustedEvaluation, CFDataRef dataToEncrypt, CFErrorRef *error)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_8_0);

//
// For testing
//
/* Caution: These functions take an iOS SecKeyRef. Careful use is required on OS X. */
CFDataRef SecCopyDecryptedForServer(SecKeyRef serverFullKey, CFDataRef encryptedData, CFErrorRef* error)
    __OSX_AVAILABLE_STARTING(__MAC_NA, __IPHONE_8_0);

CFDataRef SecCopyEncryptedToServerKey(SecKeyRef publicKey, CFDataRef dataToEncrypt, CFErrorRef *error)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_8_0);

#endif
