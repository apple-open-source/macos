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
 * CMS signerInfo methods.
 */

#include <Security/SecCmsSignerInfo.h>
#include "SecSMIMEPriv.h"

#include "cmslocal.h"

#include "cert.h"
#include "cryptohi.h"
#include "secitem.h"
#include "secoid.h"

#include <AssertMacros.h>
#include <CoreFoundation/CFTimeZone.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecIdentity.h>
#include <Security/SecItem.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecKeychain.h>
#include <Security/SecPolicyPriv.h>
#include <libDER/asn1Types.h>
#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>
#include <security_asn1/SecAsn1TimeUtils.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>

#include "tsaSupport.h"
#include "tsaSupportPriv.h"

#include <syslog.h>

#ifndef NDEBUG
#define SIGINFO_DEBUG 1
#endif

#if SIGINFO_DEBUG
#define dprintf(args...) fprintf(stderr, args)
#else
#define dprintf(args...)
#endif

#if RELEASECOUNTDEBUG
#define dprintfRC(args...) dprintf(args)
#else
#define dprintfRC(args...)
#endif

/* =============================================================================
 * SIGNERINFO
 */
SecCmsSignerInfoRef nss_cmssignerinfo_create(SecCmsMessageRef cmsg,
                                             SecCmsSignerIDSelector type,
                                             SecCertificateRef cert,
                                             CSSM_DATA_PTR subjKeyID,
                                             SecPublicKeyRef pubKey,
                                             SecPrivateKeyRef signingKey,
                                             SECOidTag digestalgtag);

SecCmsSignerInfoRef SecCmsSignerInfoCreateWithSubjKeyID(SecCmsMessageRef cmsg,
                                                        CSSM_DATA_PTR subjKeyID,
                                                        SecPublicKeyRef pubKey,
                                                        SecPrivateKeyRef signingKey,
                                                        SECOidTag digestalgtag)
{
    return nss_cmssignerinfo_create(
                                    cmsg, SecCmsSignerIDSubjectKeyID, NULL, subjKeyID, pubKey, signingKey, digestalgtag);
}

SecCmsSignerInfoRef SecCmsSignerInfoCreate(SecCmsMessageRef cmsg, SecIdentityRef identity, SECOidTag digestalgtag)
{
    SecCmsSignerInfoRef signerInfo = NULL;
    SecCertificateRef cert = NULL;
    SecPrivateKeyRef signingKey = NULL;
    CFDictionaryRef keyAttrs = NULL;

    if (SecIdentityCopyCertificate(identity, &cert)) {
        goto loser;
    }
    if (SecIdentityCopyPrivateKey(identity, &signingKey)) {
        goto loser;
    }

    /* In some situations, the "Private Key" in the identity is actually a public key. */
    keyAttrs = SecKeyCopyAttributes(signingKey);
    if (!keyAttrs) {
        goto loser;
    }
    CFTypeRef class = CFDictionaryGetValue(keyAttrs, kSecAttrKeyClass);
    if (!class || (CFGetTypeID(class) != CFStringGetTypeID()) ||
        !CFEqual(class, kSecAttrKeyClassPrivate)) {
        goto loser;
    }


    signerInfo = nss_cmssignerinfo_create(
                                          cmsg, SecCmsSignerIDIssuerSN, cert, NULL, NULL, signingKey, digestalgtag);

loser:
    CFReleaseNull(cert);
    CFReleaseNull(signingKey);
    CFReleaseNull(keyAttrs);

    return signerInfo;
}

SecCmsSignerInfoRef nss_cmssignerinfo_create(SecCmsMessageRef cmsg,
                                             SecCmsSignerIDSelector type,
                                             SecCertificateRef cert,
                                             CSSM_DATA_PTR subjKeyID,
                                             SecPublicKeyRef pubKey,
                                             SecPrivateKeyRef signingKey,
                                             SECOidTag digestalgtag)
{
    void* mark;
    SecCmsSignerInfoRef signerinfo;
    int version;
    PLArenaPool* poolp;

    poolp = cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    signerinfo = (SecCmsSignerInfoRef)PORT_ArenaZAlloc(poolp, sizeof(SecCmsSignerInfo));
    if (signerinfo == NULL) {
        PORT_ArenaRelease(poolp, mark);
        return NULL;
    }


    signerinfo->cmsg = cmsg;

    switch (type) {
        case SecCmsSignerIDIssuerSN:
            signerinfo->signerIdentifier.identifierType = SecCmsSignerIDIssuerSN;
            if ((signerinfo->cert = CERT_DupCertificate(cert)) == NULL)
                goto loser;
            if ((signerinfo->signerIdentifier.id.issuerAndSN =
                 CERT_GetCertIssuerAndSN(poolp, cert)) == NULL)
                goto loser;
            dprintfRC("nss_cmssignerinfo_create: SecCmsSignerIDIssuerSN: cert.rc %d\n",
                      (int)CFGetRetainCount(signerinfo->cert));
            break;
        case SecCmsSignerIDSubjectKeyID:
            signerinfo->signerIdentifier.identifierType = SecCmsSignerIDSubjectKeyID;
            PORT_Assert(subjKeyID);
            if (!subjKeyID) {
                goto loser;
            }
            signerinfo->signerIdentifier.id.subjectKeyID = PORT_ArenaNew(poolp, CSSM_DATA);
            if (SECITEM_CopyItem(poolp, signerinfo->signerIdentifier.id.subjectKeyID, subjKeyID)) {
                goto loser;
            }
            signerinfo->pubKey = SECKEY_CopyPublicKey(pubKey);
            if (!signerinfo->pubKey)
                goto loser;
            break;
        default:
            goto loser;
    }

    if (!signingKey)
        goto loser;

    signerinfo->signingKey = SECKEY_CopyPrivateKey(signingKey);
    if (!signerinfo->signingKey)
        goto loser;

    /* set version right now */
    version = SEC_CMS_SIGNER_INFO_VERSION_ISSUERSN;
    /* RFC2630 5.3 "version is the syntax version number. If the .... " */
    if (signerinfo->signerIdentifier.identifierType == SecCmsSignerIDSubjectKeyID)
        version = SEC_CMS_SIGNER_INFO_VERSION_SUBJKEY;
    (void)SEC_ASN1EncodeInteger(poolp, &(signerinfo->version), (long)version);

    if (SECOID_SetAlgorithmID(poolp, &signerinfo->digestAlg, digestalgtag, NULL) != SECSuccess)
        goto loser;

    PORT_ArenaUnmark(poolp, mark);
    return signerinfo;

loser:
    PORT_ArenaRelease(poolp, mark);
    return NULL;
}

/*
 * SecCmsSignerInfoDestroy - destroy a SignerInfo data structure
 */
void SecCmsSignerInfoDestroy(SecCmsSignerInfoRef si)
{
    if (si->cert != NULL) {
        dprintfRC("SecCmsSignerInfoDestroy top: certp %p cert.rc %d\n",
                  si->cert,
                  (int)CFGetRetainCount(si->cert));
        CERT_DestroyCertificate(si->cert);
    }
    CFReleaseNull(si->certList);
    CFReleaseNull(si->timestampCertList);
    CFReleaseNull(si->timestampCert);
    CFReleaseNull(si->hashAgilityAttrValue);
    CFReleaseNull(si->hashAgilityV2AttrValues);
    /* XXX storage ??? */
}

/*
 * SecCmsSignerInfoSign - sign something
 *
 */
OSStatus SecCmsSignerInfoSign(SecCmsSignerInfoRef signerinfo, CSSM_DATA_PTR digest, CSSM_DATA_PTR contentType)
{
    SecCertificateRef cert;
    SecPrivateKeyRef privkey = NULL;
    SECOidTag digestalgtag;
    SECOidTag pubkAlgTag;
    CSSM_DATA signature = {0};
    OSStatus rv;
    PLArenaPool *poolp, *tmppoolp = NULL;
    const SECAlgorithmID* algID;
    SECAlgorithmID freeAlgID;
    //CERTSubjectPublicKeyInfo *spki;

    PORT_Assert(digest != NULL);

    poolp = signerinfo->cmsg->poolp;

    switch (signerinfo->signerIdentifier.identifierType) {
        case SecCmsSignerIDIssuerSN:
            privkey = signerinfo->signingKey;
            signerinfo->signingKey = NULL;
            cert = signerinfo->cert;
            if (SecCertificateGetAlgorithmID(cert, &algID)) {
                PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
                goto loser;
            }
            break;
        case SecCmsSignerIDSubjectKeyID:
            privkey = signerinfo->signingKey;
            signerinfo->signingKey = NULL;

            if (SecKeyGetAlgorithmID(signerinfo->pubKey, &algID)) {
                PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
                goto loser;
            }
            CFReleaseNull(signerinfo->pubKey);
            break;
        default:
            PORT_SetError(SEC_ERROR_UNSUPPORTED_MESSAGE_TYPE);
            goto loser;
    }
    digestalgtag = SecCmsSignerInfoGetDigestAlgTag(signerinfo);
    /*
     * XXX I think there should be a cert-level interface for this,
     * so that I do not have to know about subjectPublicKeyInfo...
     */
    pubkAlgTag = SECOID_GetAlgorithmTag(algID);
    if (signerinfo->signerIdentifier.identifierType == SecCmsSignerIDSubjectKeyID) {
        SECOID_DestroyAlgorithmID(&freeAlgID, PR_FALSE);
    }

    if (signerinfo->authAttr != NULL) {
        CSSM_DATA encoded_attrs;

        /* find and fill in the message digest attribute. */
        rv = SecCmsAttributeArraySetAttr(poolp, &(signerinfo->authAttr), SEC_OID_PKCS9_MESSAGE_DIGEST, digest, PR_FALSE);
        if (rv != SECSuccess) {
            goto loser;
        }

        if (contentType != NULL) {
            /* if the caller wants us to, find and fill in the content type attribute. */
            rv = SecCmsAttributeArraySetAttr(poolp, &(signerinfo->authAttr), SEC_OID_PKCS9_CONTENT_TYPE, contentType, PR_FALSE);
            if (rv != SECSuccess) {
                goto loser;
            }
        }

        if ((tmppoolp = PORT_NewArena(1024)) == NULL) {
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            goto loser;
        }

        /*
         * Before encoding, reorder the attributes so that when they
         * are encoded, they will be conforming DER, which is required
         * to have a specific order and that is what must be used for
         * the hash/signature.  We do this here, rather than building
         * it into EncodeAttributes, because we do not want to do
         * such reordering on incoming messages (which also uses
         * EncodeAttributes) or our old signatures (and other "broken"
         * implementations) will not verify.  So, we want to guarantee
         * that we send out good DER encodings of attributes, but not
         * to expect to receive them.
         */
        if (SecCmsAttributeArrayReorder(signerinfo->authAttr) != SECSuccess) {
            goto loser;
        }

        encoded_attrs.Data = NULL;
        encoded_attrs.Length = 0;
        if (SecCmsAttributeArrayEncode(tmppoolp, &(signerinfo->authAttr), &encoded_attrs) == NULL) {
            goto loser;
        }

        rv = SEC_SignData(
                          &signature, encoded_attrs.Data, (int)encoded_attrs.Length, privkey, digestalgtag, pubkAlgTag);
        PORT_FreeArena(tmppoolp, PR_FALSE); /* awkward memory management :-( */
        tmppoolp = 0;
    } else {
        rv = SGN_Digest(privkey, digestalgtag, pubkAlgTag, &signature, digest);
    }
    SECKEY_DestroyPrivateKey(privkey);
    privkey = NULL;

    if (rv != SECSuccess) {
        goto loser;
    }

    if (SECITEM_CopyItem(poolp, &(signerinfo->encDigest), &signature) != SECSuccess) {
        goto loser;
    }

    SECITEM_FreeItem(&signature, PR_FALSE);

    SECOidTag sigAlgTag = SecCmsUtilMakeSignatureAlgorithm(digestalgtag, pubkAlgTag);
    if (pubkAlgTag == SEC_OID_EC_PUBLIC_KEY && SecCmsMsEcdsaCompatMode()) {
        /*
         * RFC 3278 section section 2.1.1 states that the signatureAlgorithm
         * field contains the full ecdsa-with-SHA1 OID, not plain old ecPublicKey
         * as would appear in other forms of signed datas. However Microsoft Entourage didn't
         * do this, it puts ecPublicKey there, and if we put ecdsa-with-SHA1 there,
         * MS can't verify - presumably because it takes the digest of the digest
         * before feeding it to ECDSA.
         * We handle this with a preference; default if it's not there is OFF.
         */
        sigAlgTag = SEC_OID_EC_PUBLIC_KEY;
    }

    if (SECOID_SetAlgorithmID(poolp, &(signerinfo->digestEncAlg), sigAlgTag, NULL) != SECSuccess) {
        goto loser;
    }

    return SECSuccess;

loser:
    if (signature.Length != 0) {
        SECITEM_FreeItem(&signature, PR_FALSE);
    }
    if (privkey) {
        SECKEY_DestroyPrivateKey(privkey);
    }
    if (tmppoolp) {
        PORT_FreeArena(tmppoolp, PR_FALSE);
    }
    return SECFailure;
}

OSStatus SecCmsSignerInfoVerifyCertificate(SecCmsSignerInfoRef signerinfo,
                                           SecKeychainRef keychainOrArray,
                                           CFTypeRef policies,
                                           SecTrustRef* trustRef)
{
    SecCertificateRef cert;
    CFAbsoluteTime stime;
    OSStatus rv;
    CSSM_DATA_PTR* otherCerts;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if ((cert = SecCmsSignerInfoGetSigningCertificate(signerinfo, keychainOrArray)) == NULL) {
        dprintf("SecCmsSignerInfoVerifyCertificate: no signing cert\n");
        signerinfo->verificationStatus = SecCmsVSSigningCertNotFound;
        return SECFailure;
    }
#pragma clang diagnostic pop

    /*
     * Get and convert the signing time; if available, it will be used
     * both on the cert verification and for importing the sender
     * email profile.
     */
    CFTypeRef timeStampPolicies = SecPolicyCreateAppleTimeStampingAndRevocationPolicies(policies);
    if (SecCmsSignerInfoGetTimestampTimeWithPolicy(signerinfo, timeStampPolicies, &stime) != SECSuccess)
        if (SecCmsSignerInfoGetSigningTime(signerinfo, &stime) != SECSuccess)
            stime = CFAbsoluteTimeGetCurrent();
    CFReleaseSafe(timeStampPolicies);

    rv = SecCmsSignedDataRawCerts(signerinfo->sigd, &otherCerts);
    if (rv) {
        return rv;
    }
    rv = CERT_VerifyCert(keychainOrArray, cert, otherCerts, policies, stime, trustRef);
    dprintfRC("SecCmsSignerInfoVerifyCertificate after vfy: certp %p cert.rc %d\n",
              cert,
              (int)CFGetRetainCount(cert));
    if (rv || !trustRef) {
        if (PORT_GetError() == SEC_ERROR_UNTRUSTED_CERT) {
            /* Signature or digest level verificationStatus errors should supercede certificate level errors, so only change the verificationStatus if the status was GoodSignature. */
            if (signerinfo->verificationStatus == SecCmsVSGoodSignature)
                signerinfo->verificationStatus = SecCmsVSSigningCertNotTrusted;
        }
    }
    /* FIXME isn't this leaking the cert? */
    dprintf("SecCmsSignerInfoVerifyCertificate: CertVerify rtn %d\n", (int)rv);
    return rv;
}

static void debugShowSigningCertificate(SecCmsSignerInfoRef signerinfo)
{
#if SIGINFO_DEBUG
    CFStringRef cn = SecCmsSignerInfoGetSignerCommonName(signerinfo);
    if (cn) {
        char* ccn = cfStringToChar(cn);
        if (ccn) {
            dprintf("SecCmsSignerInfoVerify: cn: %s\n", ccn);
            free(ccn);
        }
        CFReleaseNull(cn);
    }
#endif
}

/*
 * SecCmsSignerInfoVerify - verify the signature of a single SignerInfo
 *
 * Just verifies the signature. The assumption is that verification of the certificate
 * is done already.
 */
OSStatus SecCmsSignerInfoVerify(SecCmsSignerInfoRef signerinfo, CSSM_DATA_PTR digest, CSSM_DATA_PTR contentType)
{
    return SecCmsSignerInfoVerifyWithPolicy(signerinfo, NULL, digest, contentType);
}

OSStatus SecCmsSignerInfoVerifyWithPolicy(SecCmsSignerInfoRef signerinfo,
                                          CFTypeRef timeStampPolicy,
                                          CSSM_DATA_PTR digest,
                                          CSSM_DATA_PTR contentType)
{
    SecPublicKeyRef publickey = NULL;
    SecCmsAttribute* attr = NULL;
    CSSM_DATA encoded_attrs;
    SecCertificateRef cert = NULL;
    SecCmsVerificationStatus vs = SecCmsVSUnverified;
    PLArenaPool* poolp = NULL;
    SECOidTag digestAlgTag, digestEncAlgTag;

    if (signerinfo == NULL) {
        return SECFailure;
    }

    /* SecCmsSignerInfoGetSigningCertificate will fail if 2nd parm is NULL and */
    /* cert has not been verified */
    if ((cert = SecCmsSignerInfoGetSigningCert(signerinfo)) == NULL) {
        dprintf("SecCmsSignerInfoVerify: no signing cert\n");
        vs = SecCmsVSSigningCertNotFound;
        goto loser;
    }

    dprintfRC("SecCmsSignerInfoVerify top: cert %p cert.rc %d\n", cert, (int)CFGetRetainCount(cert));

    debugShowSigningCertificate(signerinfo);

    if (NULL == (publickey = SecCertificateCopyKey(cert))) {
        vs = SecCmsVSProcessingError;
        goto loser;
    }

    digestAlgTag = SECOID_GetAlgorithmTag(&(signerinfo->digestAlg));
    digestEncAlgTag = SECOID_GetAlgorithmTag(&(signerinfo->digestEncAlg));

    if (!SecCmsArrayIsEmpty((void**)signerinfo->authAttr)) {
        if (contentType) {
            /*
             * Check content type
             *
             * RFC2630 sez that if there are any authenticated attributes,
             * then there must be one for content type which matches the
             * content type of the content being signed, and there must
             * be one for message digest which matches our message digest.
             * So check these things first.
             */
            if ((attr = SecCmsAttributeArrayFindAttrByOidTag(signerinfo->authAttr, SEC_OID_PKCS9_CONTENT_TYPE, PR_TRUE)) == NULL) {
                vs = SecCmsVSMalformedSignature;
                goto loser;
            }

            if (SecCmsAttributeCompareValue(attr, contentType) == PR_FALSE) {
                vs = SecCmsVSMalformedSignature;
                goto loser;
            }
        }

        /*
         * Check digest
         */
        if ((attr = SecCmsAttributeArrayFindAttrByOidTag(signerinfo->authAttr, SEC_OID_PKCS9_MESSAGE_DIGEST, PR_TRUE)) == NULL) {
            vs = SecCmsVSMalformedSignature;
            goto loser;
        }
        if (SecCmsAttributeCompareValue(attr, digest) == PR_FALSE) {
            vs = SecCmsVSDigestMismatch;
            goto loser;
        }

        if ((poolp = PORT_NewArena(1024)) == NULL) {
            vs = SecCmsVSProcessingError;
            goto loser;
        }

        /*
         * Check signature
         *
         * The signature is based on a digest of the DER-encoded authenticated
         * attributes.  So, first we encode and then we digest/verify.
         * we trust the decoder to have the attributes in the right (sorted) order
         */
        encoded_attrs.Data = NULL;
        encoded_attrs.Length = 0;

        if (SecCmsAttributeArrayEncode(poolp, &(signerinfo->authAttr), &encoded_attrs) == NULL ||
            encoded_attrs.Data == NULL || encoded_attrs.Length == 0) {
            vs = SecCmsVSProcessingError;
            goto loser;
        }

        vs = (SECSuccess != VFY_VerifyData(encoded_attrs.Data,
                                           (int)encoded_attrs.Length,
                                           publickey,
                                           &(signerinfo->encDigest),
                                           digestAlgTag,
                                           digestEncAlgTag,
                                           signerinfo->cmsg->pwfn_arg))
                ? SecCmsVSBadSignature : SecCmsVSGoodSignature;

        dprintf("VFY_VerifyData (authenticated attributes): %s\n",
                (vs == SecCmsVSGoodSignature) ? "SecCmsVSGoodSignature" : "SecCmsVSBadSignature");

        PORT_FreeArena(poolp, PR_FALSE); /* awkward memory management :-( */

    } else {
        CSSM_DATA_PTR sig;

        /* No authenticated attributes. The signature is based on the plain message digest. */
        sig = &(signerinfo->encDigest);
        if (sig->Length == 0) {
            goto loser;
        }

        vs = (SECSuccess != VFY_VerifyDigest(digest, publickey, sig, digestAlgTag, digestEncAlgTag, signerinfo->cmsg->pwfn_arg))
                ? SecCmsVSBadSignature : SecCmsVSGoodSignature;

        dprintf("VFY_VerifyData (plain message digest): %s\n",
                (vs == SecCmsVSGoodSignature) ? "SecCmsVSGoodSignature" : "SecCmsVSBadSignature");
    }

    if (!SecCmsArrayIsEmpty((void**)signerinfo->unAuthAttr)) {
        dprintf("found an unAuthAttr\n");
        OSStatus rux = SecCmsSignerInfoVerifyUnAuthAttrsWithPolicy(signerinfo, timeStampPolicy);
        dprintf("SecCmsSignerInfoVerifyUnAuthAttrs Status: %ld\n", (long)rux);
        if (rux) {
            goto loser;
        }
    }

    if (vs == SecCmsVSBadSignature) {
        /*
         * XXX Change the generic error into our specific one, because
         * in that case we get a better explanation out of the Security
         * Advisor.  This is really a bug in our error strings (the
         * "generic" error has a lousy/wrong message associated with it
         * which assumes the signature verification was done for the
         * purposes of checking the issuer signature on a certificate)
         * but this is at least an easy workaround and/or in the
         * Security Advisor, which specifically checks for the error
         * SEC_ERROR_PKCS7_BAD_SIGNATURE and gives more explanation
         * in that case but does not similarly check for
         * SEC_ERROR_BAD_SIGNATURE.  It probably should, but then would
         * probably say the wrong thing in the case that it *was* the
         * certificate signature check that failed during the cert
         * verification done above.  Our error handling is really a mess.
         */
        if (PORT_GetError() == SEC_ERROR_BAD_SIGNATURE)
            PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
    }

    CFReleaseNull(publickey);

    signerinfo->verificationStatus = vs;
    dprintfRC("SecCmsSignerInfoVerify end: cerp %p cert.rc %d\n", cert, (int)CFGetRetainCount(cert));

    dprintf("verificationStatus: %d\n", vs);

    return (vs == SecCmsVSGoodSignature) ? SECSuccess : SECFailure;

loser:
    if (publickey != NULL)
        SECKEY_DestroyPublicKey(publickey);

    dprintf("verificationStatus2: %d\n", vs);
    signerinfo->verificationStatus = vs;

    PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
    return SECFailure;
}

OSStatus SecCmsSignerInfoVerifyUnAuthAttrs(SecCmsSignerInfoRef signerinfo)
{
    return SecCmsSignerInfoVerifyUnAuthAttrsWithPolicy(signerinfo, NULL);
}

OSStatus SecCmsSignerInfoVerifyUnAuthAttrsWithPolicy(SecCmsSignerInfoRef signerinfo, CFTypeRef timeStampPolicy)
{
    /*
     unAuthAttr is an array of attributes; we expect to
     see just one: the timestamp blob. If we have an unAuthAttr,
     but don't see a timestamp, return an error since we have
     no other cases where this would be present.
     */

    SecCmsAttribute* attr = NULL;
    OSStatus status = SECFailure;

    require(signerinfo, xit);
    attr = SecCmsAttributeArrayFindAttrByOidTag(
                                                signerinfo->unAuthAttr, SEC_OID_PKCS9_TIMESTAMP_TOKEN, PR_TRUE);
    if (attr == NULL) {
        status = errSecTimestampMissing;
        goto xit;
    }

    dprintf("found an id-ct-TSTInfo\n");
    // Don't check the nonce in this case
    status = decodeTimeStampTokenWithPolicy(
                                            signerinfo, timeStampPolicy, (attr->values)[0], &signerinfo->encDigest, 0);
    if (status != errSecSuccess) {
        secerror("timestamp verification failed: %d", (int)status);
    }

xit:
    return status;
}

CSSM_DATA* SecCmsSignerInfoGetEncDigest(SecCmsSignerInfoRef signerinfo)
{
    return &signerinfo->encDigest;
}

SecCmsVerificationStatus SecCmsSignerInfoGetVerificationStatus(SecCmsSignerInfoRef signerinfo)
{
    return signerinfo->verificationStatus;
}

SECOidData* SecCmsSignerInfoGetDigestAlg(SecCmsSignerInfoRef signerinfo)
{
    return SECOID_FindOID(&(signerinfo->digestAlg.algorithm));
}

SECOidTag SecCmsSignerInfoGetDigestAlgTag(SecCmsSignerInfoRef signerinfo)
{
    SECOidData* algdata;

    algdata = SECOID_FindOID(&(signerinfo->digestAlg.algorithm));
    if (algdata != NULL)
        return algdata->offset;
    else
        return SEC_OID_UNKNOWN;
}

CFArrayRef SecCmsSignerInfoGetCertList(SecCmsSignerInfoRef signerinfo)
{
    dprintfRC("SecCmsSignerInfoGetCertList: certList.rc %d\n",
              (int)CFGetRetainCount(signerinfo->certList));
    return signerinfo->certList;
}

CFArrayRef SecCmsSignerInfoGetTimestampCertList(SecCmsSignerInfoRef signerinfo)
{
    dprintfRC("SecCmsSignerInfoGetTimestampCertList: timestampCertList.rc %d\n",
              (int)CFGetRetainCount(signerinfo->timestampCertList));
    return signerinfo->timestampCertList;
}

SecCertificateRef SecCmsSignerInfoGetTimestampSigningCert(SecCmsSignerInfoRef signerinfo)
{
    dprintfRC("SecCmsSignerInfoGetTimestampSigningCert: timestampCert.rc %d\n",
              (int)CFGetRetainCount(signerinfo->timestampCert));
    return signerinfo->timestampCert;
}

int SecCmsSignerInfoGetVersion(SecCmsSignerInfoRef signerinfo)
{
    unsigned long version;

    /* always take apart the CSSM_DATA */
    if (SEC_ASN1DecodeInteger(&(signerinfo->version), &version) != SECSuccess)
        return 0;
    else
        return (int)version;
}

/*
 * SecCmsSignerInfoGetSigningTime - return the signing time,
 *				      in UTCTime format, of a CMS signerInfo.
 *
 * sinfo - signerInfo data for this signer
 *
 * Returns a pointer to XXXX (what?)
 * A return value of NULL is an error.
 */
OSStatus SecCmsSignerInfoGetSigningTime(SecCmsSignerInfoRef sinfo, CFAbsoluteTime* stime)
{
    SecCmsAttribute* attr;
    CSSM_DATA_PTR value;

    if (sinfo == NULL)
        return paramErr;

    if (sinfo->signingTime != 0) {
        *stime = sinfo->signingTime; /* cached copy */
        return SECSuccess;
    }

    attr = SecCmsAttributeArrayFindAttrByOidTag(sinfo->authAttr, SEC_OID_PKCS9_SIGNING_TIME, PR_TRUE);
    /* XXXX multi-valued attributes NIH */
    if (attr == NULL || (value = SecCmsAttributeGetValue(attr)) == NULL) {
        return errSecSigningTimeMissing;
    }
    if (SecAsn1DecodeTime(value, stime) != SECSuccess)
        return errSecSigningTimeMissing;
    sinfo->signingTime = *stime; /* make cached copy */
    return SECSuccess;
}

OSStatus SecCmsSignerInfoGetTimestampTime(SecCmsSignerInfoRef sinfo, CFAbsoluteTime* stime)
{
    return SecCmsSignerInfoGetTimestampTimeWithPolicy(sinfo, NULL, stime);
}

OSStatus SecCmsSignerInfoGetTimestampTimeWithPolicy(SecCmsSignerInfoRef sinfo,
                                                    CFTypeRef timeStampPolicy,
                                                    CFAbsoluteTime* stime)
{
    OSStatus status = paramErr;

    require(sinfo && stime, xit);

    if (sinfo->timestampTime != 0) {
        *stime = sinfo->timestampTime; /* cached copy */
        return noErr;
    }

    // A bit heavyweight if haven't already called verify
    status = SecCmsSignerInfoVerifyUnAuthAttrsWithPolicy(sinfo, timeStampPolicy);
    *stime = sinfo->timestampTime;
xit:
    return status;
}

/*!
 @function
 @abstract Return the data in the signed Codesigning Hash Agility attribute.
 @param sinfo SignerInfo data for this signer, pointer to a CFDataRef for attribute value
 @discussion Returns a CFDataRef containing the value of the attribute
 @result A return value of errSecInternal is an error trying to look up the oid.
 A status value of success with null result data indicates the attribute was not present.
 */
OSStatus SecCmsSignerInfoGetAppleCodesigningHashAgility(SecCmsSignerInfoRef sinfo, CFDataRef* sdata)
{
    SecCmsAttribute* attr;
    CSSM_DATA_PTR value;

    if (sinfo == NULL || sdata == NULL) {
        return paramErr;
    }

    *sdata = NULL;

    if (sinfo->hashAgilityAttrValue != NULL) {
        *sdata = sinfo->hashAgilityAttrValue; /* cached copy */
        return SECSuccess;
    }

    attr = SecCmsAttributeArrayFindAttrByOidTag(sinfo->authAttr, SEC_OID_APPLE_HASH_AGILITY, PR_TRUE);

    /* attribute not found */
    if (attr == NULL || (value = SecCmsAttributeGetValue(attr)) == NULL) {
        return SECSuccess;
    }

    if (value->Length > LONG_MAX) {
        return errSecAllocate;
    }
    sinfo->hashAgilityAttrValue = CFDataCreate(NULL, value->Data, (CFIndex)value->Length); /* make cached copy */
    if (sinfo->hashAgilityAttrValue) {
        *sdata = sinfo->hashAgilityAttrValue;
        return SECSuccess;
    }
    return errSecAllocate;
}

/* AgileHash ::= SEQUENCE {
 hashType OBJECT IDENTIFIER,
 hashValues OCTET STRING }
 */
typedef struct {
    SecAsn1Item digestOID;
    SecAsn1Item digestValue;
} CMSAppleAgileHash;

static const SecAsn1Template CMSAppleAgileHashTemplate[] = {
    {SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CMSAppleAgileHash)},
    {
        SEC_ASN1_OBJECT_ID,
        offsetof(CMSAppleAgileHash, digestOID),
    },
    {
        SEC_ASN1_OCTET_STRING,
        offsetof(CMSAppleAgileHash, digestValue),
    },
    {
        0,
    }};

static OSStatus CMSAddAgileHashToDictionary(CFMutableDictionaryRef dictionary, SecAsn1Item* DERAgileHash)
{
    PLArenaPool* tmppoolp = NULL;
    OSStatus status = errSecSuccess;
    CMSAppleAgileHash agileHash;
    CFDataRef digestValue = NULL;
    CFNumberRef digestTag = NULL;

    tmppoolp = PORT_NewArena(1024);
    if (tmppoolp == NULL) {
        return errSecAllocate;
    }

    if ((status = SEC_ASN1DecodeItem(tmppoolp, &agileHash, CMSAppleAgileHashTemplate, DERAgileHash)) !=
        errSecSuccess) {
        goto loser;
    }

    if (agileHash.digestValue.Length > LONG_MAX) {
        status = errSecAllocate;
        goto loser;
    }

    int64_t tag = SECOID_FindOIDTag(&agileHash.digestOID);
    digestTag = CFNumberCreate(NULL, kCFNumberSInt64Type, &tag);
    digestValue = CFDataCreate(NULL, agileHash.digestValue.Data, (CFIndex)agileHash.digestValue.Length);
    CFDictionaryAddValue(dictionary, digestTag, digestValue);

loser:
    CFReleaseNull(digestValue);
    CFReleaseNull(digestTag);
    if (tmppoolp) {
        PORT_FreeArena(tmppoolp, PR_FALSE);
    }
    return status;
}

/*!
 @function
 @abstract Return the data in the signed Codesigning Hash Agility V2 attribute.
 @param sinfo SignerInfo data for this signer, pointer to a CFDictionaryRef for attribute values
 @discussion Returns a CFDictionaryRef containing the values of the attribute
 @result A return value of errSecInternal is an error trying to look up the oid.
 A status value of success with null result data indicates the attribute was not present.
 */
OSStatus SecCmsSignerInfoGetAppleCodesigningHashAgilityV2(SecCmsSignerInfoRef sinfo, CFDictionaryRef* sdict)
{
    SecCmsAttribute* attr;

    if (sinfo == NULL || sdict == NULL) {
        return errSecParam;
    }

    *sdict = NULL;

    if (sinfo->hashAgilityV2AttrValues != NULL) {
        *sdict = sinfo->hashAgilityV2AttrValues; /* cached copy */
        return SECSuccess;
    }

    attr = SecCmsAttributeArrayFindAttrByOidTag(sinfo->authAttr, SEC_OID_APPLE_HASH_AGILITY_V2, PR_TRUE);

    /* attribute not found */
    if (attr == NULL) {
        return SECSuccess;
    }

    /* attrValues SET OF AttributeValue
     * AttributeValue ::= ANY
     */
    CSSM_DATA_PTR* values = attr->values;
    if (values == NULL) { /* There must be values */
        return errSecDecode;
    }

    CFMutableDictionaryRef agileHashValues = CFDictionaryCreateMutable(NULL, SecCmsArrayCount((void**)values), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    while (*values != NULL) {
        (void)CMSAddAgileHashToDictionary(agileHashValues, *values++);
    }
    if (CFDictionaryGetCount(agileHashValues) != SecCmsArrayCount((void**)attr->values)) {
        CFReleaseNull(agileHashValues);
        return errSecDecode;
    }

    sinfo->hashAgilityV2AttrValues = agileHashValues; /* make cached copy */
    if (sinfo->hashAgilityV2AttrValues) {
        *sdict = sinfo->hashAgilityV2AttrValues;
        return SECSuccess;
    }
    return errSecAllocate;
}

/*
 * SecCmsSignerInfoGetAppleExpirationTime - return the expiration time,
 *                      in UTCTime format, of a CMS signerInfo.
 *
 * sinfo - signerInfo data for this signer
 *
 * Returns a pointer to XXXX (what?)
 * A return value of NULL is an error.
 */
OSStatus SecCmsSignerInfoGetAppleExpirationTime(SecCmsSignerInfoRef sinfo, CFAbsoluteTime* etime)
{
    SecCmsAttribute* attr = NULL;
    SecAsn1Item* value = NULL;

    if (sinfo == NULL || etime == NULL) {
        return SECFailure;
    }

    if (sinfo->expirationTime != 0) {
        *etime = sinfo->expirationTime; /* cached copy */
        return SECSuccess;
    }

    attr = SecCmsAttributeArrayFindAttrByOidTag(sinfo->authAttr, SEC_OID_APPLE_EXPIRATION_TIME, PR_TRUE);
    if (attr == NULL || (value = SecCmsAttributeGetValue(attr)) == NULL) {
        return SECFailure;
    }
    if (SecAsn1DecodeTime(value, etime) != SECSuccess) {
        return SECFailure;
    }
    sinfo->expirationTime = *etime; /* make cached copy */
    return SECSuccess;
}

/*
 * Return the signing cert of a CMS signerInfo.
 *
 * the certs in the enclosing SignedData must have been imported already
 */
static SecCertificateRef SecCmsSignerInfoGetSigningCertificate_internal(SecCmsSignerInfoRef signerinfo, SecKeychainRef keychainOrArray)
{
    SecCertificateRef cert;
    SecCmsSignerIdentifier* sid;
    OSStatus ortn;
    CSSM_DATA_PTR* rawCerts;

    if (signerinfo->cert != NULL) {
        dprintfRC("SecCmsSignerInfoGetSigningCertificate top: cert %p cert.rc %d\n",
                  signerinfo->cert,
                  (int)CFGetRetainCount(signerinfo->cert));
        return signerinfo->cert;
    }
    ortn = SecCmsSignedDataRawCerts(signerinfo->sigd, &rawCerts);
    if (ortn) {
        return NULL;
    }
    dprintf("SecCmsSignerInfoGetSigningCertificate: numRawCerts %d\n",
            SecCmsArrayCount((void**)rawCerts));

    /*
     * This cert will also need to be freed, but since we save it
     * in signerinfo for later, we do not want to destroy it when
     * we leave this function -- we let the clean-up of the entire
     * cinfo structure later do the destroy of this cert.
     */
    sid = &signerinfo->signerIdentifier;
    switch (sid->identifierType) {
        case SecCmsSignerIDIssuerSN:
            cert = CERT_FindCertByIssuerAndSN(keychainOrArray,
                                              rawCerts,
                                              signerinfo->sigd->certs,
                                              signerinfo->cmsg->poolp,
                                              sid->id.issuerAndSN);
            break;
        case SecCmsSignerIDSubjectKeyID:
            cert = CERT_FindCertBySubjectKeyID(
                                               keychainOrArray, rawCerts, signerinfo->sigd->certs, sid->id.subjectKeyID);
            break;
        default:
            cert = NULL;
            break;
    }

    /* cert can be NULL at that point */
    signerinfo->cert = cert; /* earmark it */
    dprintfRC("SecCmsSignerInfoGetSigningCertificate end: certp %p cert.rc %d\n",
              signerinfo->cert,
              (int)CFGetRetainCount(signerinfo->cert));

    return cert;
}

SecCertificateRef SecCmsSignerInfoGetSigningCert(SecCmsSignerInfoRef signerinfo)
{
    return SecCmsSignerInfoGetSigningCertificate_internal(signerinfo, NULL);
}

SecCertificateRef SecCmsSignerInfoGetSigningCertificate(SecCmsSignerInfoRef signerinfo,
                                                        SecKeychainRef keychainOrArray)
{
    return SecCmsSignerInfoGetSigningCertificate_internal(signerinfo, keychainOrArray);
}

/*
 * SecCmsSignerInfoGetSignerCommonName - return the common name of the signer
 *
 * sinfo - signerInfo data for this signer
 *
 * Returns a CFStringRef containing the common name of the signer.
 * A return value of NULL is an error.
 */
CFStringRef SecCmsSignerInfoGetSignerCommonName(SecCmsSignerInfoRef sinfo)
{
    SecCertificateRef signercert;
    CFStringRef commonName = NULL;

    /* will fail if cert is not verified */
    if ((signercert = SecCmsSignerInfoGetSigningCert(sinfo)) == NULL)
        return NULL;

    if (errSecSuccess != SecCertificateCopyCommonName(signercert, &commonName)) {
        return NULL;
    }

    return commonName;
}

/*
 * SecCmsSignerInfoGetSignerEmailAddress - return the email address of the signer
 *
 * sinfo - signerInfo data for this signer
 *
 * Returns a CFStringRef containing the name of the signer.
 * A return value of NULL is an error.
 */
CFStringRef SecCmsSignerInfoGetSignerEmailAddress(SecCmsSignerInfoRef sinfo)
{
    SecCertificateRef signercert;
    CFStringRef emailAddress = NULL;

    if ((signercert = SecCmsSignerInfoGetSigningCert(sinfo)) == NULL) {
        return NULL;
    }

    CFArrayRef names = SecCertificateCopyRFC822Names(signercert);
    if (names) {
        if (CFArrayGetCount(names) > 0) {
            emailAddress = (CFStringRef)CFArrayGetValueAtIndex(names, 0);
        }
        CFRetainSafe(emailAddress);
        CFReleaseNull(names);
    }
    return emailAddress;
}


/*
 * SecCmsSignerInfoAddAuthAttr - add an attribute to the
 * authenticated (i.e. signed) attributes of "signerinfo".
 */
OSStatus SecCmsSignerInfoAddAuthAttr(SecCmsSignerInfoRef signerinfo, SecCmsAttribute* attr)
{
    return SecCmsAttributeArrayAddAttr(signerinfo->cmsg->poolp, &(signerinfo->authAttr), attr);
}

/*
 * SecCmsSignerInfoAddUnauthAttr - add an attribute to the
 * unauthenticated attributes of "signerinfo".
 */
OSStatus SecCmsSignerInfoAddUnauthAttr(SecCmsSignerInfoRef signerinfo, SecCmsAttribute* attr)
{
    return SecCmsAttributeArrayAddAttr(signerinfo->cmsg->poolp, &(signerinfo->unAuthAttr), attr);
}

/*
 * SecCmsSignerInfoAddSigningTime - add the signing time to the
 * authenticated (i.e. signed) attributes of "signerinfo".
 *
 * This is expected to be included in outgoing signed
 * messages for email (S/MIME) but is likely useful in other situations.
 *
 * This should only be added once; a second call will do nothing.
 *
 * XXX This will probably just shove the current time into "signerinfo"
 * but it will not actually get signed until the entire item is
 * processed for encoding.  Is this (expected to be small) delay okay?
 */
OSStatus SecCmsSignerInfoAddSigningTime(SecCmsSignerInfoRef signerinfo, CFAbsoluteTime t)
{
    SecCmsAttribute* attr;
    SecAsn1Item stime = { .Data = NULL, .Length = 0 };
    void* mark;
    PLArenaPool* poolp;
    OSStatus status = errSecInternal;

    poolp = signerinfo->cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    /* create new signing time attribute */
    NSS_Time timeStr;
    if (SecAsn1EncodeTime(poolp, t, &timeStr) != SECSuccess) {
        goto loser;
    }

    if (SEC_ASN1EncodeItem(poolp, &stime, &timeStr, kSecAsn1TimeTemplate) != &stime) {
        goto loser;
    }

    if ((attr = SecCmsAttributeCreate(poolp, SEC_OID_PKCS9_SIGNING_TIME, &stime, PR_TRUE)) == NULL) {
        goto loser;
    }

    if ((status = SecCmsSignerInfoAddAuthAttr(signerinfo, attr)) != SECSuccess) {
        goto loser;
    }

    PORT_ArenaUnmark(poolp, mark);

    return status;

loser:
    PORT_ArenaRelease(poolp, mark);
    return status;
}

/*
 * SecCmsSignerInfoAddSMIMECaps - add a SMIMECapabilities attribute to the
 * authenticated (i.e. signed) attributes of "signerinfo".
 *
 * This is expected to be included in outgoing signed
 * messages for email (S/MIME).
 */
OSStatus SecCmsSignerInfoAddSMIMECaps(SecCmsSignerInfoRef signerinfo)
{
    SecCmsAttribute* attr;
    CSSM_DATA_PTR smimecaps = NULL;
    void* mark;
    PLArenaPool* poolp;

    poolp = signerinfo->cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    smimecaps = SECITEM_AllocItem(poolp, NULL, 0);
    if (smimecaps == NULL) {
        goto loser;
    }

    /* create new signing time attribute */
#if 1
    // @@@ We don't do Fortezza yet.
    if (SecSMIMECreateSMIMECapabilities((SecArenaPoolRef)poolp, smimecaps, PR_FALSE) != SECSuccess)
#else
        if (SecSMIMECreateSMIMECapabilities(poolp, smimecaps, PK11_FortezzaHasKEA(signerinfo->cert)) != SECSuccess)
#endif
            goto loser;

    if ((attr = SecCmsAttributeCreate(poolp, SEC_OID_PKCS9_SMIME_CAPABILITIES, smimecaps, PR_TRUE)) == NULL)
        goto loser;

    if (SecCmsSignerInfoAddAuthAttr(signerinfo, attr) != SECSuccess)
        goto loser;

    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease(poolp, mark);
    return SECFailure;
}

/*
 * SecCmsSignerInfoAddSMIMEEncKeyPrefs - add a SMIMEEncryptionKeyPreferences attribute to the
 * authenticated (i.e. signed) attributes of "signerinfo".
 *
 * This is expected to be included in outgoing signed messages for email (S/MIME).
 */
static OSStatus SecCmsSignerInfoAddSMIMEEncKeyPrefs_internal(SecCmsSignerInfoRef signerinfo,
                                             SecCertificateRef cert,
                                             SecKeychainRef keychainOrArray)
{
    SecCmsAttribute* attr;
    CSSM_DATA_PTR smimeekp = NULL;
    void* mark;
    PLArenaPool* poolp;

    poolp = signerinfo->cmsg->poolp;
    mark = PORT_ArenaMark(poolp);

    smimeekp = SECITEM_AllocItem(poolp, NULL, 0);
    if (smimeekp == NULL) {
        goto loser;
    }

    /* create new signing time attribute */
    if (SecSMIMECreateSMIMEEncKeyPrefs((SecArenaPoolRef)poolp, smimeekp, cert) != SECSuccess)
        goto loser;

    if ((attr = SecCmsAttributeCreate(poolp, SEC_OID_SMIME_ENCRYPTION_KEY_PREFERENCE, smimeekp, PR_TRUE)) == NULL) {
        goto loser;
    }

    if (SecCmsSignerInfoAddAuthAttr(signerinfo, attr) != SECSuccess) {
        goto loser;
    }

    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease(poolp, mark);
    return SECFailure;
}

OSStatus SecCmsSignerInfoAddSMIMEEncKeyPrefs(SecCmsSignerInfoRef signerinfo,
                                             SecCertificateRef cert,
                                             SecKeychainRef keychainOrArray)
{
    return SecCmsSignerInfoAddSMIMEEncKeyPrefs_internal(signerinfo, cert, keychainOrArray);
}

OSStatus SecCmsSignerInfoAddSMIMEEncKeyPreferences(SecCmsSignerInfoRef signerinfo, SecCertificateRef cert)
{
    return SecCmsSignerInfoAddSMIMEEncKeyPrefs_internal(signerinfo, cert, NULL);
}

/*
 * SecCmsSignerInfoAddMSSMIMEEncKeyPrefs - add a SMIMEEncryptionKeyPreferences attribute to the
 * authenticated (i.e. signed) attributes of "signerinfo", using the OID preferred by Microsoft.
 *
 * This is expected to be included in outgoing signed messages for email (S/MIME),
 * if compatibility with Microsoft mail clients is wanted.
 */
static OSStatus SecCmsSignerInfoAddMSSMIMEEncKeyPrefs_internal(SecCmsSignerInfoRef signerinfo,
                                               SecCertificateRef cert,
                                               SecKeychainRef keychainOrArray)
{
    SecCmsAttribute* attr;
    CSSM_DATA_PTR smimeekp = NULL;
    void* mark;
    PLArenaPool* poolp;

    poolp = signerinfo->cmsg->poolp;
    mark = PORT_ArenaMark(poolp);

    smimeekp = SECITEM_AllocItem(poolp, NULL, 0);
    if (smimeekp == NULL) {
        goto loser;
    }

    /* create new signing time attribute */
    if (SecSMIMECreateMSSMIMEEncKeyPrefs((SecArenaPoolRef)poolp, smimeekp, cert) != SECSuccess)
        goto loser;

    if ((attr = SecCmsAttributeCreate(poolp, SEC_OID_MS_SMIME_ENCRYPTION_KEY_PREFERENCE, smimeekp, PR_TRUE)) == NULL) {
        goto loser;
    }

    if (SecCmsSignerInfoAddAuthAttr(signerinfo, attr) != SECSuccess) {
        goto loser;
    }

    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease(poolp, mark);
    return SECFailure;
}

OSStatus SecCmsSignerInfoAddMSSMIMEEncKeyPrefs(SecCmsSignerInfoRef signerinfo,
                                             SecCertificateRef cert,
                                             SecKeychainRef keychainOrArray)
{
    return SecCmsSignerInfoAddMSSMIMEEncKeyPrefs_internal(signerinfo, cert, keychainOrArray);
}

OSStatus SecCmsSignerInfoAddMSSMIMEEncKeyPreferences(SecCmsSignerInfoRef signerinfo, SecCertificateRef cert)
{
    return SecCmsSignerInfoAddMSSMIMEEncKeyPrefs_internal(signerinfo, cert, NULL);
}

/*
 * SecCmsSignerInfoAddTimeStamp - add time stamp to the
 * unauthenticated (i.e. unsigned) attributes of "signerinfo".
 *
 * This will initially be used for time stamping signed applications
 * by using a Time Stamping Authority. It may also be included in outgoing signed
 * messages for email (S/MIME), and may be useful in other situations.
 *
 * This should only be added once; a second call will do nothing.
 *
 */

/*
 Countersignature attribute values have ASN.1 type Countersignature:
 Countersignature ::= SignerInfo
 Countersignature values have the same meaning as SignerInfo values
 for ordinary signatures, except that:
 1.  The signedAttributes field MUST NOT contain a content-type
 attribute; there is no content type for countersignatures.
 2.  The signedAttributes field MUST contain a message-digest
 attribute if it contains any other attributes.
 3.  The input to the message-digesting process is the contents octets
 of the DER encoding of the signatureValue field of the SignerInfo
 value with which the attribute is associated.
 */

/*!
 @function
 @abstract Create a timestamp unsigned attribute with a TimeStampToken.
 */

OSStatus SecCmsSignerInfoAddTimeStamp(SecCmsSignerInfoRef signerinfo, CSSM_DATA* tstoken)
{
    SecCmsAttribute* attr;
    PLArenaPool* poolp = signerinfo->cmsg->poolp;
    void* mark = PORT_ArenaMark(poolp);

    // We have already encoded this ourselves, so last param is PR_TRUE
    if ((attr = SecCmsAttributeCreate(poolp, SEC_OID_PKCS9_TIMESTAMP_TOKEN, tstoken, PR_TRUE)) == NULL)
        goto loser;

    if (SecCmsSignerInfoAddUnauthAttr(signerinfo, attr) != SECSuccess)
        goto loser;

    PORT_ArenaUnmark(poolp, mark);

    return SECSuccess;

loser:
    PORT_ArenaRelease(poolp, mark);
    return SECFailure;
}

/*
 * SecCmsSignerInfoAddCounterSignature - countersign a signerinfo
 *
 * 1. digest the DER-encoded signature value of the original signerinfo
 * 2. create new signerinfo with correct version, sid, digestAlg
 * 3. add message-digest authAttr, but NO content-type
 * 4. sign the authAttrs
 * 5. DER-encode the new signerInfo
 * 6. add the whole thing to original signerInfo's unAuthAttrs
 *    as a SEC_OID_PKCS9_COUNTER_SIGNATURE attribute
 *
 * XXXX give back the new signerinfo?
 */
OSStatus SecCmsSignerInfoAddCounterSignature(SecCmsSignerInfoRef signerinfo,
                                             SECOidTag digestalg,
                                             SecIdentityRef identity)
{
    /* XXXX TBD XXXX */
    return SECFailure;
}

/*!
 @function
 @abstract Add the Apple Codesigning Hash Agility attribute to the authenticated (i.e. signed) attributes of "signerinfo".
 @discussion This is expected to be included in outgoing Apple code signatures.
 */
OSStatus SecCmsSignerInfoAddAppleCodesigningHashAgility(SecCmsSignerInfoRef signerinfo, CFDataRef attrValue)
{
    SecCmsAttribute* attr;
    PLArenaPool* poolp = signerinfo->cmsg->poolp;
    void* mark = PORT_ArenaMark(poolp);
    OSStatus status = SECFailure;

    /* The value is required for this attribute. */
    if (!attrValue || CFDataGetLength(attrValue) < 0) {
        status = errSecParam;
        goto loser;
    }

    /*
     * SecCmsAttributeCreate makes a copy of the data in value, so
     * we don't need to copy into the CSSM_DATA struct.
     */
    CSSM_DATA value;
    value.Length = (size_t)CFDataGetLength(attrValue);
    value.Data = (uint8_t*)CFDataGetBytePtr(attrValue);

    if ((attr = SecCmsAttributeCreate(poolp, SEC_OID_APPLE_HASH_AGILITY, &value, PR_FALSE)) == NULL) {
        status = errSecAllocate;
        goto loser;
    }

    if (SecCmsSignerInfoAddAuthAttr(signerinfo, attr) != SECSuccess) {
        status = errSecInternalError;
        goto loser;
    }

    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease(poolp, mark);
    return status;
}

static OSStatus
CMSAddAgileHashToAttribute(PLArenaPool* poolp, SecCmsAttribute* attr, CFNumberRef cftag, CFDataRef value)
{
    PLArenaPool* tmppoolp = NULL;
    int64_t tag;
    SECOidData* digestOid = NULL;
    CMSAppleAgileHash agileHash;
    SecAsn1Item attrValue = {.Data = NULL, .Length = 0};
    OSStatus status = errSecSuccess;

    memset(&agileHash, 0, sizeof(agileHash));

    if (!CFNumberGetValue(cftag, kCFNumberSInt64Type, &tag) || CFDataGetLength(value) < 0) {
        return errSecParam;
    }
    digestOid = SECOID_FindOIDByTag((SECOidTag)tag);

    agileHash.digestValue.Data = (uint8_t*)CFDataGetBytePtr(value);
    agileHash.digestValue.Length = (size_t)CFDataGetLength(value);
    agileHash.digestOID.Data = digestOid->oid.Data;
    agileHash.digestOID.Length = digestOid->oid.Length;

    tmppoolp = PORT_NewArena(1024);
    if (tmppoolp == NULL) {
        return errSecAllocate;
    }

    if (SEC_ASN1EncodeItem(tmppoolp, &attrValue, &agileHash, CMSAppleAgileHashTemplate) == NULL) {
        status = errSecParam;
        goto loser;
    }

    status = SecCmsAttributeAddValue(poolp, attr, &attrValue);

loser:
    if (tmppoolp) {
        PORT_FreeArena(tmppoolp, PR_FALSE);
    }
    return status;
}

/*!
 @function
 @abstract Add the Apple Codesigning Hash Agility attribute to the authenticated (i.e. signed) attributes of "signerinfo".
 @discussion This is expected to be included in outgoing Apple code signatures.
 */
OSStatus SecCmsSignerInfoAddAppleCodesigningHashAgilityV2(SecCmsSignerInfoRef signerinfo,
                                                          CFDictionaryRef attrValues)
{
    __block SecCmsAttribute* attr;
    __block PLArenaPool* poolp = signerinfo->cmsg->poolp;
    void* mark = PORT_ArenaMark(poolp);
    OSStatus status = SECFailure;

    /* The value is required for this attribute. */
    if (!attrValues) {
        status = errSecParam;
        goto loser;
    }

    if ((attr = SecCmsAttributeCreate(poolp, SEC_OID_APPLE_HASH_AGILITY_V2, NULL, PR_TRUE)) == NULL) {
        status = errSecAllocate;
        goto loser;
    }

    CFDictionaryForEach(attrValues, ^(const void* key, const void* value) {
        if (!isNumber(key) || !isData(value)) {
            return;
        }
        (void)CMSAddAgileHashToAttribute(poolp, attr, (CFNumberRef)key, (CFDataRef)value);
    });

    if (SecCmsSignerInfoAddAuthAttr(signerinfo, attr) != SECSuccess) {
        status = errSecInternal;
        goto loser;
    }

    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease(poolp, mark);
    return status;
}

/*
 * SecCmsSignerInfoAddAppleExpirationTime - add the expiration time to the
 * authenticated (i.e. signed) attributes of "signerinfo".
 *
 * This is expected to be included in outgoing signed
 * messages for Asset Receipts but is likely useful in other situations.
 *
 * This should only be added once; a second call will do nothing.
 */
OSStatus SecCmsSignerInfoAddAppleExpirationTime(SecCmsSignerInfoRef signerinfo, CFAbsoluteTime t)
{
    SecCmsAttribute* attr = NULL;
    PLArenaPool* poolp = signerinfo->cmsg->poolp;
    void* mark = PORT_ArenaMark(poolp);
    OSStatus status = errSecInternal;
    SecAsn1Item etime = { .Data = NULL, .Length = 0 };

    /* create new signing time attribute */
    NSS_Time timeStr;
    if (SecAsn1EncodeTime(poolp, t, &timeStr) != SECSuccess) {
        goto loser;
    }

    if (SEC_ASN1EncodeItem(poolp, &etime, &timeStr, kSecAsn1TimeTemplate) != &etime) {
        goto loser;
    }

    if ((attr = SecCmsAttributeCreate(poolp, SEC_OID_APPLE_EXPIRATION_TIME, &etime, PR_TRUE)) == NULL) {
        goto loser;
    }

    if ((status = SecCmsSignerInfoAddAuthAttr(signerinfo, attr)) != SECSuccess) {
        goto loser;
    }

    PORT_ArenaUnmark(poolp, mark);

    return status;

loser:
    PORT_ArenaRelease(poolp, mark);
    return status;
}

SecCertificateRef SecCmsSignerInfoCopyCertFromEncryptionKeyPreference(SecCmsSignerInfoRef signerinfo)
{
    SecCertificateRef cert = NULL;
    SecCmsAttribute* attr;
    CSSM_DATA_PTR ekp;
    SecKeychainRef keychainOrArray;

    (void)SecKeychainCopyDefault(&keychainOrArray);

    /* see if verification status is ok (unverified does not count...) */
    if (signerinfo->verificationStatus != SecCmsVSGoodSignature)
        return NULL;

    /* Prep the raw certs */
    CSSM_DATA_PTR* rawCerts = NULL;
    if (signerinfo->sigd) {
        rawCerts = signerinfo->sigd->rawCerts;
    }

    /* find preferred encryption cert */
    if (!SecCmsArrayIsEmpty((void**)signerinfo->authAttr) &&
        (attr = SecCmsAttributeArrayFindAttrByOidTag(
                                                     signerinfo->authAttr, SEC_OID_SMIME_ENCRYPTION_KEY_PREFERENCE, PR_TRUE)) !=
        NULL) { /* we have a SMIME_ENCRYPTION_KEY_PREFERENCE attribute! Find the cert. */
        ekp = SecCmsAttributeGetValue(attr);
        if (ekp == NULL) {
            return NULL;
        }
        cert = SecSMIMEGetCertFromEncryptionKeyPreference(keychainOrArray, rawCerts, ekp);
    }
    if (cert) {
        return cert;
    }

    if (!SecCmsArrayIsEmpty((void**)signerinfo->authAttr) &&
        (attr = SecCmsAttributeArrayFindAttrByOidTag(
                                                     signerinfo->authAttr, SEC_OID_MS_SMIME_ENCRYPTION_KEY_PREFERENCE, PR_TRUE)) !=
        NULL) { /* we have a MS_SMIME_ENCRYPTION_KEY_PREFERENCE attribute! Find the cert. */
        ekp = SecCmsAttributeGetValue(attr);
        if (ekp == NULL) {
            return NULL;
        }
        cert = SecSMIMEGetCertFromEncryptionKeyPreference(keychainOrArray, rawCerts, ekp);
    }
    return cert;
}

/*
 * XXXX the following needs to be done in the S/MIME layer code
 * after signature of a signerinfo is verified
 */
OSStatus SecCmsSignerInfoSaveSMIMEProfile(SecCmsSignerInfoRef signerinfo)
{
    SecCertificateRef cert = NULL;
    CSSM_DATA_PTR profile = NULL;
    SecCmsAttribute* attr;
    CSSM_DATA_PTR utc_stime = NULL;
    CSSM_DATA_PTR ekp;
    int save_error;
    OSStatus rv;
    Boolean must_free_cert = PR_FALSE;

    /* see if verification status is ok (unverified does not count...) */
    if (signerinfo->verificationStatus != SecCmsVSGoodSignature)
        return SECFailure;

    /* find preferred encryption cert */
    if (!SecCmsArrayIsEmpty((void**)signerinfo->authAttr) &&
        (attr = SecCmsAttributeArrayFindAttrByOidTag(signerinfo->authAttr, /* we have a SMIME_ENCRYPTION_KEY_PREFERENCE attribute! */
                                                     SEC_OID_SMIME_ENCRYPTION_KEY_PREFERENCE, PR_TRUE)) != NULL) {
        ekp = SecCmsAttributeGetValue(attr);
        if (ekp == NULL) {
            return SECFailure;
        }

        /* we assume that all certs coming with the message have been imported to the */
        /* temporary database */
        cert = SecSMIMEGetCertFromEncryptionKeyPreference(NULL, NULL, ekp);
        if (cert == NULL)
            return SECFailure;
        must_free_cert = PR_TRUE;
    }

    if (cert == NULL) {
        /* no preferred cert found?
         * find the cert the signerinfo is signed with instead */
        CFStringRef emailAddress = NULL;

        cert = SecCmsSignerInfoGetSigningCert(signerinfo);
        if (cert == NULL) {
            return SECFailure;
        }
        if (SecCertificateGetEmailAddress(cert, &emailAddress)) {
            return SECFailure;
        }
    }

    /* XXX store encryption cert permanently? */

    /*
     * Remember the current error set because we do not care about
     * anything set by the functions we are about to call.
     */
    save_error = PORT_GetError();

    if (!SecCmsArrayIsEmpty((void**)signerinfo->authAttr)) {
        attr = SecCmsAttributeArrayFindAttrByOidTag(
                                                    signerinfo->authAttr, SEC_OID_PKCS9_SMIME_CAPABILITIES, PR_TRUE);
        profile = SecCmsAttributeGetValue(attr);
        attr = SecCmsAttributeArrayFindAttrByOidTag(
                                                    signerinfo->authAttr, SEC_OID_PKCS9_SIGNING_TIME, PR_TRUE);
        utc_stime = SecCmsAttributeGetValue(attr);
    }

    rv = CERT_SaveSMimeProfile(cert, profile, utc_stime);
    if (must_free_cert) {
        CERT_DestroyCertificate(cert);
    }

    /*
     * Restore the saved error in case the calls above set a new
     * one that we do not actually care about.
     */
    PORT_SetError(save_error);

    return rv;
}

/*
 * SecCmsSignerInfoIncludeCerts - set cert chain inclusion mode for this signer
 */
OSStatus SecCmsSignerInfoIncludeCerts(SecCmsSignerInfoRef signerinfo, SecCmsCertChainMode cm, SECCertUsage usage)
{
    if (signerinfo->cert == NULL) {
        return SECFailure;
    }

    /* don't leak if we get called twice */
    CFReleaseNull(signerinfo->certList);

    switch (cm) {
        case SecCmsCMNone:
            signerinfo->certList = NULL;
            break;
        case SecCmsCMCertOnly:
            signerinfo->certList = CERT_CertListFromCert(signerinfo->cert);
            break;
        case SecCmsCMCertChain:
            signerinfo->certList = CERT_CertChainFromCert(signerinfo->cert, usage, PR_FALSE, PR_FALSE);
            break;
        case SecCmsCMCertChainWithRoot:
            signerinfo->certList = CERT_CertChainFromCert(signerinfo->cert, usage, PR_TRUE, PR_FALSE);
            break;
        case SecCmsCMCertChainWithRootOrFail:
            signerinfo->certList = CERT_CertChainFromCert(signerinfo->cert, usage, PR_TRUE, PR_TRUE);
    }

    if (cm != SecCmsCMNone && signerinfo->certList == NULL) {
        return SECFailure;
    }

    return SECSuccess;
}
