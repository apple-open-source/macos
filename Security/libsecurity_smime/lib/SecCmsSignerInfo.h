/*
 *  Copyright (c) 2004,2008,2010,2013 Apple Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*!
    @header SecCmsSignerInfo.h
    @Copyright (c) 2004,2008,2010,2013 Apple Inc. All Rights Reserved.

    @availability 10.4 and later
    @abstract Interfaces of the CMS implementation.
    @discussion The functions here implement functions for encoding
                and decoding Cryptographic Message Syntax (CMS) objects
                as described in rfc3369.
 */

#ifndef _SECURITY_SECCMSSIGNERINFO_H_
#define _SECURITY_SECCMSSIGNERINFO_H_  1

#include <Security/SecCmsBase.h>

#include <Security/SecTrust.h>


#if defined(__cplusplus)
extern "C" {
#endif

/*!
    @function
 */
extern SecCmsSignerInfoRef
SecCmsSignerInfoCreate(SecCmsSignedDataRef sigd, SecIdentityRef identity, SECOidTag digestalgtag);

/*!
    @function
 */
extern SecCmsSignerInfoRef
SecCmsSignerInfoCreateWithSubjKeyID(SecCmsSignedDataRef sigd, const SecAsn1Item *subjKeyID, SecPublicKeyRef pubKey, SecPrivateKeyRef signingKey, SECOidTag digestalgtag);

/*!
    @function
 */
extern SecCmsVerificationStatus
SecCmsSignerInfoGetVerificationStatus(SecCmsSignerInfoRef signerinfo);

/*!
    @function
 */
extern SECOidData *
SecCmsSignerInfoGetDigestAlg(SecCmsSignerInfoRef signerinfo);

/*!
    @function
 */
extern SECOidTag
SecCmsSignerInfoGetDigestAlgTag(SecCmsSignerInfoRef signerinfo);

/*!
    @function
 */
extern CFArrayRef
SecCmsSignerInfoGetCertList(SecCmsSignerInfoRef signerinfo);

/*!
    @function
    @abstract Return the signing time, in UTCTime format, of a CMS signerInfo.
    @param sinfo SignerInfo data for this signer.
    @discussion Returns a pointer to XXXX (what?)
    @result A return value of NULL is an error.
 */
extern OSStatus
SecCmsSignerInfoGetSigningTime(SecCmsSignerInfoRef sinfo, CFAbsoluteTime *stime);

/*!
    @function
    @abstract Return the signing cert of a CMS signerInfo.
    @discussion The certs in the enclosing SignedData must have been imported already.
 */
extern SecCertificateRef
SecCmsSignerInfoGetSigningCertificate(SecCmsSignerInfoRef signerinfo, SecKeychainRef keychainOrArray);

/*!
    @function
    @abstract Return the common name of the signer.
    @param sinfo SignerInfo data for this signer.
    @discussion Returns a CFStringRef containing the common name of the signer.
    @result A return value of NULL is an error.
 */
extern CF_RETURNS_RETAINED CFStringRef
SecCmsSignerInfoGetSignerCommonName(SecCmsSignerInfoRef sinfo);

/*!
    @function
    @abstract Return the email address of the signer
    @param sinfo SignerInfo data for this signer.
    @discussion Returns a CFStringRef containing the name of the signer.
    @result A return value of NULL is an error.
 */
extern CF_RETURNS_RETAINED CFStringRef
SecCmsSignerInfoGetSignerEmailAddress(SecCmsSignerInfoRef sinfo);

/*!
    @function
    @abstract Add the signing time to the authenticated (i.e. signed) attributes of "signerinfo". 
    @discussion This is expected to be included in outgoing signed
                messages for email (S/MIME) but is likely useful in other situations.

                This should only be added once; a second call will do nothing.

                XXX This will probably just shove the current time into "signerinfo"
                but it will not actually get signed until the entire item is
                processed for encoding.  Is this (expected to be small) delay okay?
 */
extern OSStatus
SecCmsSignerInfoAddSigningTime(SecCmsSignerInfoRef signerinfo, CFAbsoluteTime t);

/*!
    @function
    @abstract Add a SMIMECapabilities attribute to the authenticated (i.e. signed) attributes of "signerinfo".
    @discussion This is expected to be included in outgoing signed messages for email (S/MIME).
 */
extern OSStatus
SecCmsSignerInfoAddSMIMECaps(SecCmsSignerInfoRef signerinfo);

/*!
    @function
    @abstract Add a SMIMEEncryptionKeyPreferences attribute to the authenticated (i.e. signed) attributes of "signerinfo".
    @discussion This is expected to be included in outgoing signed messages for email (S/MIME).
 */
OSStatus
SecCmsSignerInfoAddSMIMEEncKeyPrefs(SecCmsSignerInfoRef signerinfo, SecCertificateRef cert, SecKeychainRef keychainOrArray);

/*!
    @function
    @abstract Add a SMIMEEncryptionKeyPreferences attribute to the authenticated (i.e. signed) attributes of "signerinfo", using the OID prefered by Microsoft.
    @discussion This is expected to be included in outgoing signed messages for email (S/MIME), if compatibility with Microsoft mail clients is wanted.
 */
OSStatus
SecCmsSignerInfoAddMSSMIMEEncKeyPrefs(SecCmsSignerInfoRef signerinfo, SecCertificateRef cert, SecKeychainRef keychainOrArray);

/*!
    @function
    @abstract Countersign a signerinfo.
 */
extern OSStatus
SecCmsSignerInfoAddCounterSignature(SecCmsSignerInfoRef signerinfo,
				    SECOidTag digestalg, SecIdentityRef identity);

/*!
    @function
    @abstract The following needs to be done in the S/MIME layer code after signature of a signerinfo has been verified.
    @param signerinfo The SecCmsSignerInfo object for which we verified the signature.
    @result The preferred encryption certificate of the user who signed this message will be added to the users default Keychain and it will be marked as the preferred certificate to use when sending that person messages from now on.
 */
extern OSStatus
SecCmsSignerInfoSaveSMIMEProfile(SecCmsSignerInfoRef signerinfo);

/*!
    @function
    @abstract Set cert chain inclusion mode for this signer.
 */
extern OSStatus
SecCmsSignerInfoIncludeCerts(SecCmsSignerInfoRef signerinfo, SecCmsCertChainMode cm, SECCertUsage usage);

/*! @functiongroup CMS misc utility functions */
/*!
    @function
    Convert a SecCmsVerificationStatus to a human readable string.
 */
extern const char *
SecCmsUtilVerificationStatusToString(SecCmsVerificationStatus vs);


#if defined(__cplusplus)
}
#endif

#endif /* _SECURITY_SECCMSSIGNERINFO_H_ */
