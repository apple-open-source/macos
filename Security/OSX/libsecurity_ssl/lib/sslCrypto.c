/*
 * Copyright (c) 2006-2014 Apple Inc. All Rights Reserved.
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

/*
 * sslCrypto.c - interface between SSL and crypto libraries
 */

#include "sslCrypto.h"
#include "sslContext.h"
#include "sslMemory.h"
#include "sslDebug.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <Security/SecTrustPriv.h>
#include <Security/SecPolicy.h>
#include <Security/SecCertificate.h>

#include <AssertMacros.h>
#include "utilities/SecCFRelease.h"

#if TARGET_OS_IPHONE
#include <Security/SecRSAKey.h>
#include <Security/SecECKey.h>
#endif

#include <tls_helpers.h>

/*
 * Get algorithm id for a SSLPubKey object.
 */
CFIndex sslPubKeyGetAlgorithmID(SecKeyRef pubKey)
{
#if TARGET_OS_IPHONE
	return SecKeyGetAlgorithmID(pubKey);
#else
	return SecKeyGetAlgorithmId(pubKey);
#endif
}

/*
 * Get algorithm id for a SSLPrivKey object.
 */
CFIndex sslPrivKeyGetAlgorithmID(SecKeyRef privKey)
{
#if TARGET_OS_IPHONE
	return SecKeyGetAlgorithmID(privKey);
#else
	return SecKeyGetAlgorithmId(privKey);
#endif
}


OSStatus
sslCreateSecTrust(
	SSLContext				*ctx,
    SecTrustRef             *pTrust)	/* RETURNED */
{
	OSStatus status = errSecAllocate;
	SecTrustRef trust = NULL;

    require_noerr(status = tls_helper_create_peer_trust(ctx->hdsk, ctx->protocolSide==kSSLServerSide, &trust), errOut);

	/* If we have trustedAnchors we set them here. */
    if (trust && ctx->trustedCerts) {
        require_noerr(status = SecTrustSetAnchorCertificates(trust, ctx->trustedCerts), errOut);
        require_noerr(status = SecTrustSetAnchorCertificatesOnly(trust, ctx->trustedCertsOnly), errOut);
    }

    status = errSecSuccess;

errOut:
    if(status != noErr) {
        CFReleaseSafe(trust);
        *pTrust = NULL;
    } else {
        *pTrust = trust;
    }

	return status;
}

#if !TARGET_OS_IPHONE
/* Return the first certificate reference from the supplied array
 * whose data matches the given certificate, or NULL if none match.
 */
static
SecCertificateRef
sslGetMatchingCertInArray(
	SecCertificateRef	certRef,
	CFArrayRef			certArray)
{
	SecCertificateRef matchedCert = NULL;

	if (certRef == NULL || certArray == NULL) {
		return NULL;
	}

	CFDataRef certData = SecCertificateCopyData(certRef);
	if (certData) {
		CFIndex idx, count = CFArrayGetCount(certArray);
		for(idx=0; idx<count; idx++) {
			SecCertificateRef aCert = (SecCertificateRef)CFArrayGetValueAtIndex(certArray, idx);
			CFDataRef aData = SecCertificateCopyData(aCert);
			if (aData && CFEqual(aData, certData)) {
				matchedCert = aCert;
			}
			CFReleaseSafe(aData);
			if (matchedCert)
				break;
		}
		CFReleaseSafe(certData);
	}

    return matchedCert;
}
#endif

/*
 * Verify a chain of DER-encoded certs.
 */
static OSStatus sslVerifyCertChain(
	SSLContext				*ctx)
{
	OSStatus status;
	SecTrustRef trust = NULL;

    /* renegotiate - start with a new SecTrustRef */
    CFReleaseNull(ctx->peerSecTrust);

    /* on failure, we always return trust==NULL, so we don't check the returned status here */
    sslCreateSecTrust(ctx, &trust);

    if(trust==NULL) {
        if(ctx->protocolSide == kSSLClientSide) {
            /* No cert chain is always a trust failure on the server side */
            status = errSSLXCertChainInvalid;
            sslErrorLog("***Error: NULL server cert chain\n");
        } else {
            /* No cert chain on the client side is ok unless using kAlwaysAuthenticate */
            if(ctx->clientAuth == kAlwaysAuthenticate) {
                sslErrorLog("***Error: NULL client cert chain\n");
                status = errSSLXCertChainInvalid;
            } else {
                status = noErr;
            }
        }
        goto errOut;
    }


	if (!ctx->enableCertVerify) {
		/* trivial case, this is caller's responsibility */
		status = errSecSuccess;
		goto errOut;
	}

	SecTrustResultType secTrustResult;
	require_noerr(status = SecTrustEvaluate(trust, &secTrustResult), errOut);
	switch (secTrustResult) {
        case kSecTrustResultUnspecified:
            /* cert chain valid, no special UserTrust assignments */
        case kSecTrustResultProceed:
            /* cert chain valid AND user explicitly trusts this */
            status = errSecSuccess;
            break;
        case kSecTrustResultDeny:
        case kSecTrustResultConfirm:
        case kSecTrustResultRecoverableTrustFailure:
        default:
            if(ctx->allowAnyRoot) {
                sslErrorLog("***Warning: accepting unverified cert chain\n");
                status = errSecSuccess;
            }
            else {
#if !TARGET_OS_IPHONE
				/*
				 * If the caller provided a list of trusted leaf certs, check them here
				 */
                if(ctx->trustedLeafCerts) {
                    if (sslGetMatchingCertInArray(SecTrustGetCertificateAtIndex(trust, 0),
                                                  ctx->trustedLeafCerts)) {
                        status = errSecSuccess;
                        goto errOut;
                    }
                }
#endif
				status = errSSLXCertChainInvalid;
            }
            /* Do we really need to return things like:
                   errSSLNoRootCert
                   errSSLUnknownRootCert
                   errSSLCertExpired
                   errSSLCertNotYetValid
                   errSSLHostNameMismatch
               for our client to see what went wrong, or should we just always
               return
                   errSSLXCertChainInvalid
               when something is wrong? */
            break;
	}

errOut:
	ctx->peerSecTrust = trust;

	return status;
}

/* Convert cert in DER format into an CFArray of SecCertificateRef */
CFArrayRef
tls_get_peer_certs(const SSLCertificate *certs)
{
    const SSLCertificate *cert;

    CFMutableArrayRef certArray = NULL;
    CFDataRef certData = NULL;
    SecCertificateRef cfCert = NULL;

    certArray = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    require(certArray, out);
    cert = certs;
    while(cert) {
        require((certData = CFDataCreate(kCFAllocatorDefault, cert->derCert.data, cert->derCert.length)), out);
        require((cfCert = SecCertificateCreateWithData(kCFAllocatorDefault, certData)), out);
        CFArrayAppendValue(certArray, cfCert);
        CFReleaseNull(cfCert);
        CFReleaseNull(certData);
        cert=cert->next;
    }

    return certArray;

out:
    CFReleaseNull(cfCert);
    CFReleaseNull(certData);
    CFReleaseNull(certArray);
    return NULL;
}

int
tls_verify_peer_cert(SSLContext *ctx)
{
    int err = 0;
    OSStatus st;

    /* Note: A verification failure here does not cause the function to return an error.
       This will allow the handshake to continue, coreTLS will eventually returns an error,
       after sending the appropriate alert messages, based on the trust value set with the
       call to tls_handshake_set_peer_trust(). In some case a verification failure here is 
       normal, for example if there is no cert (eg: PSK and Anon DH ciphersuites) */

    st = sslVerifyCertChain(ctx);
    tls_handshake_trust_t trust;
    switch (st) {
        case errSecSuccess:
            trust = tls_handshake_trust_ok;
            break;
        case errSSLUnknownRootCert:
        case errSSLNoRootCert:
            trust = tls_handshake_trust_unknown_root;
            break;
        case errSSLCertExpired:
        case errSSLCertNotYetValid:
            trust = tls_handshake_trust_cert_expired;
            break;
        case errSSLXCertChainInvalid:
        default:
            trust = tls_handshake_trust_cert_invalid;
            break;
    }

    tls_handshake_set_peer_trust(ctx->hdsk, trust);

    /* Now that trust has been (possibly) evaluated,
       we check if we need to break out of the handshake */
    if(ctx->protocolSide == kSSLServerSide) {
        /*
         * Schedule return to the caller to verify the client's identity.
         * This will return even if there was no client cert sent.
         */
        if (ctx->breakOnClientAuth) {
            err = errSSLClientAuthCompleted;
        }
    } else if(ctx->peerSecTrust) {
        /*
         * Schedule return to the caller to verify the server's identity.
         * This will only return if a server cert was sent. In other cases
         * such as PSK and AnonDH, we don't want to break out of the handshake.
         */
        if (ctx->breakOnServerAuth) {
            err = errSSLServerAuthCompleted;
        }
    }

    return err;
}

/*
 * After ciphersuite negotiation is complete, verify that we have
 * the capability of actually performing the selected cipher.
 * Currently we just verify that we have a cert and private signing
 * key, if needed, and that the signing key's algorithm matches the
 * expected key exchange method.
 *
 * This is currently called from FindCipherSpec(), after it sets
 * ctx->selectedCipherSpec to a (supposedly) valid value, and from
 * sslBuildCipherSpecArray(), in server mode (pre-negotiation) only.
 */

#if 0
OSStatus sslVerifySelectedCipher(SSLContext *ctx)
{

    if(ctx->protocolSide == kSSLClientSide) {
        return errSecSuccess;
    }
#if SSL_PAC_SERVER_ENABLE
    if((ctx->masterSecretCallback != NULL) &&
       (ctx->sessionTicket.data != NULL)) {
            /* EAP via PAC resumption; we can do it */
	return errSecSuccess;
    }
#endif	/* SSL_PAC_SERVER_ENABLE */

    CFIndex requireAlg;
    switch (ctx->selectedCipherSpecParams.keyExchangeMethod) {
        case SSL_RSA:
        case SSL_RSA_EXPORT:
	case SSL_DH_RSA:
	case SSL_DH_RSA_EXPORT:
	case SSL_DHE_RSA:
	case SSL_DHE_RSA_EXPORT:
            requireAlg = kSecRSAAlgorithmID;
            break;
 	case SSL_DHE_DSS:
	case SSL_DHE_DSS_EXPORT:
 	case SSL_DH_DSS:
	case SSL_DH_DSS_EXPORT:
            requireAlg = kSecDSAAlgorithmID;
            break;
	case SSL_DH_anon:
	case SSL_DH_anon_EXPORT:
        case TLS_PSK:
            requireAlg = kSecNullAlgorithmID; /* no signing key */
            break;
        /*
         * When SSL_ECDSA_SERVER is true and we support ECDSA on the server side,
         * we'll need to add some logic here...
         */
#if SSL_ECDSA_SERVER
        case SSL_ECDHE_ECDSA:
        case SSL_ECDHE_RSA:
        case SSL_ECDH_ECDSA:
        case SSL_ECDH_RSA:
        case SSL_ECDH_anon:
            requireAlg = kSecECDSAAlgorithmID;
            break;
#endif

	default:
            /* needs update per cipherSpecs.c */
            assert(0);
            sslErrorLog("sslVerifySelectedCipher: unknown key exchange method\n");
            return errSSLInternal;
    }

    if(requireAlg == kSecNullAlgorithmID) {
	return errSecSuccess;
    }

    /* private signing key required */
    if(ctx->signingPrivKeyRef == NULL) {
	sslErrorLog("sslVerifySelectedCipher: no signing key\n");
	return errSSLBadConfiguration;
    }

    /* Check the alg of our signing key. */
    CFIndex keyAlg = sslPrivKeyGetAlgorithmID(ctx->signingPrivKeyRef);
    if (requireAlg != keyAlg) {
	sslErrorLog("sslVerifySelectedCipher: signing key alg mismatch\n");
	return errSSLBadConfiguration;
    }

    return errSecSuccess;
}

#endif
