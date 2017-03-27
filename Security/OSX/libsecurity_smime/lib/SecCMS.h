/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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


#ifndef libsecurity_smime_SecCMS_h
#define libsecurity_smime_SecCMS_h

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecBase.h>
#include <Security/SecTrust.h>

extern const void * kSecCMSSignDigest;
extern const void * kSecCMSSignDetached;
extern const void * kSecCMSSignHashAlgorithm;
extern const void * kSecCMSCertChainMode;
extern const void * kSecCMSAdditionalCerts;
extern const void * kSecCMSSignedAttributes;
extern const void * kSecCMSSignDate;
extern const void * kSecCMSAllCerts;

extern const void * kSecCMSHashingAlgorithmSHA1;
extern const void * kSecCMSHashingAlgorithmSHA256;
extern const void * kSecCMSHashingAlgorithmSHA384;
extern const void * kSecCMSHashingAlgorithmSHA512;

extern const void * kSecCMSBulkEncryptionAlgorithm;
extern const void * kSecCMSEncryptionAlgorithmDESCBC;
extern const void * kSecCMSEncryptionAlgorithmAESCBC;

/* Return an array of certificates contained in message, if message is of the
 type SignedData and has no signers, return NULL otherwise.   Not that if
 the message is properly formed but has no certificates an empty array will
 be returned. 
 Designed to match the sec submodule implementation available for iOS
 */
CFArrayRef SecCMSCertificatesOnlyMessageCopyCertificates(CFDataRef message);

/* Create a degenerate PKCS#7 containing a cert or a CFArray of certs. */
CFDataRef SecCMSCreateCertificatesOnlyMessage(CFTypeRef cert_or_array_thereof);
CFDataRef SecCMSCreateCertificatesOnlyMessageIAP(SecCertificateRef cert);

/*!
 @function SecCMSVerifyCopyDataAndAttributes
 @abstract verify a signed data cms blob.
 @param message the cms message to be parsed
 @param detached_contents to pass detached contents (optional)
 @param policy specifies policy or array thereof should be used (optional).
 if none is passed the blob will **not** be verified and only
 the attached contents will be returned.
 @param trustref (output/optional) if specified, the trust chain built during
    verification will not be evaluated but returned to the caller to do so.
 @param attached_contents (output/optional) return a copy of the attached
 contents.
 @param signed_attributes (output/optional) return a copy of the signed
    attributes as a CFDictionary from oids (CFData) to values
    (CFArray of CFData).
 @result A result code.  See "Security Error Codes" (SecBase.h).
    errSecDecode not a CMS message we can parse,
    errSecAuthFailed bad signature, or untrusted signer if caller doesn't
    ask for trustref,
    errSecParam garbage in, garbage out.
 */
OSStatus SecCMSVerifyCopyDataAndAttributes(CFDataRef message, CFDataRef detached_contents,
                                           CFTypeRef policy, SecTrustRef *trustref,
                                           CFDataRef *attached_contents, CFDictionaryRef *signed_attributes);

/*!
 @function SecCMSVerify
 @abstract same as SecCMSVerifyCopyDataAndAttributes, for binary compatibility.
 */
OSStatus SecCMSVerify(CFDataRef message, CFDataRef detached_contents,
                      CFTypeRef policy, SecTrustRef *trustref, CFDataRef *attached_contents);

OSStatus SecCMSVerifySignedData(CFDataRef message, CFDataRef detached_contents,
                                CFTypeRef policy, SecTrustRef *trustref, CFArrayRef additional_certificates,
                                CFDataRef *attached_contents, CFDictionaryRef *message_attributes);

/*!
	@function SecCMSCreateSignedData
 @abstract create a signed data cms blob.
 @param identity signer
 @param data SHA-1 digest or message to be signed
 @param parameters (input/optional) specify algorithm, detached, digest
 @param signed_attributes (input/optional) signed attributes to insert
 as a CFDictionary from oids (CFData) to value (CFData).
 @param signed_data (output) return signed message.
 @result A result code.  See "Security Error Codes" (SecBase.h).
 errSecParam garbage in, garbage out.
 */
OSStatus SecCMSCreateSignedData(SecIdentityRef identity, CFDataRef data,
                                CFDictionaryRef parameters, CFDictionaryRef signed_attributes,
                                CFMutableDataRef signed_data);

/*!
 @function SecCMSCreateEnvelopedData
 @abstract create a enveloped cms blob for recipients
 @param recipient_or_cfarray_thereof SecCertificateRef for each recipient
 @param params CFDictionaryRef with encryption parameters
 @param data Data to be encrypted
 @param enveloped_data (output) return enveloped message.
 @result A result code.  See "Security Error Codes" (SecBase.h).
 errSecParam garbage in, garbage out.
 */
OSStatus SecCMSCreateEnvelopedData(CFTypeRef recipient_or_cfarray_thereof,
                                   CFDictionaryRef params, CFDataRef data, CFMutableDataRef enveloped_data);


/*!
 @function SecCMSDecryptEnvelopedData
 @abstract open an enveloped cms blob. expects recipients identity in keychain.
 @param message Eveloped message
 @param data (output) return decrypted message.
 @param recipient (output/optional) return addressed recipient
 @result A result code.  See "Security Error Codes" (SecBase.h).
 errSecParam garbage in, garbage out.
 */
OSStatus SecCMSDecryptEnvelopedData(CFDataRef message,
                                    CFMutableDataRef data, SecCertificateRef *recipient);

#endif
