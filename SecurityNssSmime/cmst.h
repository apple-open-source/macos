/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * Header for CMS types.
 */

#ifndef _CMST_H_
#define _CMST_H_

#include <SecurityNssAsn1/seccomon.h>
#include <SecurityNssSmime/secoidt.h>
#include <CoreFoundation/CFArray.h>
#include <Security/SecKey.h>

/* @@@ This should probably move to SecKey.h */
typedef SecKeyRef SecSymmetricKeyRef;
typedef SecKeyRef SecPublicKeyRef;
typedef SecKeyRef SecPrivateKeyRef;

typedef void(*PK11PasswordFunc)(void);

/* Non-opaque objects.  NOTE, though: I want them to be treated as
 * opaque as much as possible.  If I could hide them completely,
 * I would.  (I tried, but ran into trouble that was taking me too
 * much time to get out of.)  I still intend to try to do so.
 * In fact, the only type that "outsiders" should even *name* is
 * SecCmsMessage, and they should not reference its fields.
 */

typedef struct SecCmsMessageStr SecCmsMessage;

typedef struct SecCmsContentInfoStr SecCmsContentInfo;

typedef struct SecCmsSignedDataStr SecCmsSignedData;
typedef struct SecCmsIssuerAndSNStr SecCmsIssuerAndSN;
typedef struct SecCmsSignerInfoStr SecCmsSignerInfo;

typedef struct SecCmsEnvelopedDataStr SecCmsEnvelopedData;
typedef struct SecCmsOriginatorInfoStr SecCmsOriginatorInfo;
typedef struct SecCmsRecipientInfoStr SecCmsRecipientInfo;

typedef struct SecCmsDigestedDataStr SecCmsDigestedData;
typedef struct SecCmsEncryptedDataStr SecCmsEncryptedData;

typedef struct SecCmsAttributeStr SecCmsAttribute;

typedef struct SecCmsDecoderContextStr SecCmsDecoderContext;
typedef struct SecCmsEncoderContextStr SecCmsEncoderContext;

typedef struct SecCmsDigestContextStr SecCmsDigestContext;


/*
 * Type of function passed to SecCmsDecode or SecCmsDecoderStart.
 * If specified, this is where the content bytes (only) will be "sent"
 * as they are recovered during the decoding.
 * And:
 * Type of function passed to SecCmsEncode or SecCmsEncoderStart.
 * This is where the DER-encoded bytes will be "sent".
 *
 * XXX Should just combine this with SecCmsEncoderContentCallback type
 * and use a simpler, common name.
 */
typedef void (*SecCmsContentCallback)(void *arg, const char *buf, unsigned long len);

/*
 * Type of function passed to SecCmsDecode or SecCmsDecoderStart
 * to retrieve the decryption key.  This function is intended to be
 * used for EncryptedData content info's which do not have a key available
 * in a certificate, etc.
 */
typedef SecSymmetricKeyRef(*SecCmsGetDecryptKeyCallback)(void *arg, SECAlgorithmID *algid);

typedef enum {
    SecCmsVSUnverified = 0,
    SecCmsVSGoodSignature = 1,
    SecCmsVSBadSignature = 2,
    SecCmsVSDigestMismatch = 3,
    SecCmsVSSigningCertNotFound = 4,
    SecCmsVSSigningCertNotTrusted = 5,
    SecCmsVSSignatureAlgorithmUnknown = 6,
    SecCmsVSSignatureAlgorithmUnsupported = 7,
    SecCmsVSMalformedSignature = 8,
    SecCmsVSProcessingError = 9
} SecCmsVerificationStatus;

typedef enum {
    SecCmsCMNone = 0,
    SecCmsCMCertOnly = 1,
    SecCmsCMCertChain = 2,
    SecCmsCMCertChainWithRoot = 3
} SecCmsCertChainMode;

// @@@ This should be replaced with SecPolicyRefs
typedef enum SECCertUsageEnum {
    certUsageSSLClient = 0,
    certUsageSSLServer = 1,
    certUsageSSLServerWithStepUp = 2,
    certUsageSSLCA = 3,
    certUsageEmailSigner = 4,
    certUsageEmailRecipient = 5,
    certUsageObjectSigner = 6,
    certUsageUserCertImport = 7,
    certUsageVerifyCA = 8,
    certUsageProtectedObjectSigner = 9,
    certUsageStatusResponder = 10,
    certUsageAnyCA = 11
} SECCertUsage;


#endif /* _CMST_H_ */
