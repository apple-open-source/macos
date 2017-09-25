/*
 * Copyright (c) 2006-2012 Apple Inc. All Rights Reserved.
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
 * secCrypto.c - interface between SSL and SecKey/SecDH interfaces.
 */

#include <tls_helpers.h>

#include <Security/Security.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecureTransport.h>
#include <Security/oidsalg.h>
#include <AssertMacros.h>

#if TARGET_OS_IPHONE
#include <Security/SecRSAKey.h>
#include <Security/SecECKey.h>
#endif

#define CFReleaseNull(CF) { CFTypeRef _cf = (CF); if (_cf) CFRelease(_cf); CF = NULL; }
#define CFReleaseSafe(CF) { CFTypeRef _cf = (CF); if (_cf) CFRelease(_cf);}

static CFArrayRef
tls_helper_create_cfarray_from_buffer_list(const tls_buffer_list_t *list)
{
    CFMutableArrayRef array = NULL;
    CFDataRef data = NULL;

    array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    require(array, out);

    while(list) {
        require((data = CFDataCreate(kCFAllocatorDefault, list->buffer.data, list->buffer.length)), out);
        CFArrayAppendValue(array, data);
        CFReleaseNull(data);
        list=list->next;
    }

    return array;

out:
    CFReleaseSafe(array);
    CFReleaseSafe(data);
    return NULL;
}

static CFArrayRef
tls_helper_create_cfarray_from_certificates(const SSLCertificate *cert)
{
    CFMutableArrayRef array = NULL;
    CFDataRef certData = NULL;
    SecCertificateRef cfCert = NULL;

    if(cert == NULL)
        return NULL;

    array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    require(array, out);

    while(cert) {
        require((certData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, cert->derCert.data, cert->derCert.length, kCFAllocatorNull)), out);
        require((cfCert = SecCertificateCreateWithData(kCFAllocatorDefault, certData)), out);
        CFArrayAppendValue(array, cfCert);
        CFReleaseNull(cfCert);
        CFReleaseNull(certData);
        cert=cert->next;
    }

    return array;

out:
    CFReleaseSafe(array);
    CFReleaseSafe(cfCert);
    CFReleaseSafe(certData);
    return NULL;
}

OSStatus
tls_helper_create_peer_trust(tls_handshake_t hdsk, bool server, SecTrustRef *trustRef)
{
    OSStatus status = errSecAllocate;
    CFStringRef peerDomainName = NULL;
    CFTypeRef policies = NULL;
    SecTrustRef trust = NULL;
    const char *peerDomainNameData = NULL;
    size_t peerDomainNameLen = 0;
    CFArrayRef certChain = NULL;

    certChain = tls_helper_create_cfarray_from_certificates(tls_handshake_get_peer_certificates(hdsk));

    if(certChain == NULL) {
        *trustRef = NULL;
        return 0;
    }

    if(!server) {
        tls_handshake_get_peer_hostname(hdsk, &peerDomainNameData, &peerDomainNameLen);
    }

    if (peerDomainNameLen && peerDomainNameData) {
        CFIndex len = peerDomainNameLen;
        if (peerDomainNameData[len - 1] == 0) {
            len--;
            //secwarning("peerDomainName is zero terminated!");
        }
        /* @@@ Double check that this is the correct encoding. */
        require(peerDomainName = CFStringCreateWithBytes(kCFAllocatorDefault,
                                                         (const UInt8 *)peerDomainNameData, len,
                                                         kCFStringEncodingUTF8, false), errOut);
    }

    /* If we are the client, our peer certificates must satisfy the
     ssl server policy. */
    bool use_server_policy = (!server);
    require(policies = SecPolicyCreateSSL(use_server_policy, peerDomainName), errOut);


    require_noerr(status = SecTrustCreateWithCertificates(certChain, policies,
                                                          &trust), errOut);

    /* If we are the client, let's see if we have OCSP responses and SCTs in the TLS handshake */
    if(!server) {
        const tls_buffer_list_t *sct_list = tls_handshake_get_peer_sct_list(hdsk);
        const tls_buffer *ocsp_response = tls_handshake_get_peer_ocsp_response(hdsk);

        if(ocsp_response) {
            CFDataRef responseData = CFDataCreate(kCFAllocatorDefault, ocsp_response->data, ocsp_response->length);
            status = SecTrustSetOCSPResponse(trust, responseData);
            CFReleaseSafe(responseData);
            require_noerr(status, errOut);
        }

        if(sct_list) {
            CFArrayRef sctArray = tls_helper_create_cfarray_from_buffer_list(tls_handshake_get_peer_sct_list(hdsk));
            status = SecTrustSetSignedCertificateTimestamps(trust, sctArray);
            CFReleaseSafe(sctArray);
            require_noerr(status, errOut);
        }
    }

    status = errSecSuccess;
    
errOut:
    CFReleaseSafe(peerDomainName);
    CFReleaseSafe(policies);
    CFReleaseSafe(certChain);
    
    *trustRef = trust;
    
    return status;
}


/* Extract the pubkey from the peer certificate chain, and send it to the tls_handshake context */
OSStatus
tls_helper_set_peer_pubkey(tls_handshake_t hdsk)
{
    int err;
    CFIndex algId;
    SecKeyRef pubkey = NULL;
    CFDataRef modulus = NULL;
    CFDataRef exponent = NULL;
    CFDataRef ecpubdata = NULL;
    SecTrustRef trustRef = NULL;

    CFArrayRef certChain = tls_helper_create_cfarray_from_certificates(tls_handshake_get_peer_certificates(hdsk));

    /* We should really set the coreTLS key to "NULL", but coreTLS API needs to be fixed to allow it */
    if(certChain == NULL)
        return 0;

    require_noerr(err = SecTrustCreateWithCertificates(certChain, NULL, &trustRef), errOut);

    require_action((pubkey = SecTrustCopyPublicKey(trustRef)), errOut, err=-9808); // errSSLBadCert

	algId = SecKeyGetAlgorithmId(pubkey);

    err = -9809; //errSSLCrypto;

    switch(algId) {
        case kSecRSAAlgorithmID:
        {
            require((modulus = SecKeyCopyModulus(pubkey)), errOut);
            require((exponent = SecKeyCopyExponent(pubkey)), errOut);

            tls_buffer mod;
            tls_buffer exp;

            mod.data = (uint8_t *)CFDataGetBytePtr(modulus);
            mod.length = CFDataGetLength(modulus);

            exp.data = (uint8_t *)CFDataGetBytePtr(exponent);
            exp.length = CFDataGetLength(exponent);

            err = tls_handshake_set_peer_rsa_public_key(hdsk, &mod, &exp);
            break;
        }
        case kSecECDSAAlgorithmID:
        {
            tls_named_curve curve = (tls_named_curve)SecECKeyGetNamedCurve(pubkey);
            require((ecpubdata = SecECKeyCopyPublicBits(pubkey)), errOut);

            tls_buffer pubdata;
            pubdata.data = (uint8_t *)CFDataGetBytePtr(ecpubdata);
            pubdata.length = CFDataGetLength(ecpubdata);

            err = tls_handshake_set_peer_ec_public_key(hdsk, curve, &pubdata);

            break;
        }
        default:
            break;
    }

errOut:
    CFReleaseSafe(pubkey);
    CFReleaseSafe(modulus);
    CFReleaseSafe(exponent);
    CFReleaseSafe(ecpubdata);
    CFReleaseSafe(trustRef);
    CFReleaseSafe(certChain);

    return err;
}


tls_protocol_version
tls_helper_version_from_SSLProtocol(SSLProtocol protocol)
{
    switch (protocol) {
        case kSSLProtocol3:             return tls_protocol_version_SSL_3;
        case kTLSProtocol1:             return tls_protocol_version_TLS_1_0;
        case kTLSProtocol11:            return tls_protocol_version_TLS_1_1;
        case kTLSProtocol12:            return tls_protocol_version_TLS_1_2;
        case kTLSProtocol13:            return tls_protocol_version_TLS_1_3;
        case kDTLSProtocol1:            return tls_protocol_version_DTLS_1_0;
        case kTLSProtocolMaxSupported:  return tls_protocol_version_TLS_1_3;
        default:                        return tls_protocol_version_Undertermined;
    }
}

/* concert between private SSLProtocolVersion and public SSLProtocol */
SSLProtocol
tls_helper_SSLProtocol_from_version(tls_protocol_version version)
{
    switch(version) {
        case tls_protocol_version_SSL_3:     return kSSLProtocol3;
        case tls_protocol_version_TLS_1_0:   return kTLSProtocol1;
        case tls_protocol_version_TLS_1_1:   return kTLSProtocol11;
        case tls_protocol_version_TLS_1_2:   return kTLSProtocol12;
        case tls_protocol_version_TLS_1_3:   return kTLSProtocol13;
        case tls_protocol_version_DTLS_1_0:  return kDTLSProtocol1;
        case tls_protocol_version_Undertermined: /* DROPTHROUGH */
        default:
            return kSSLProtocolUnknown;
    }
}

CFArrayRef
tls_helper_create_peer_acceptable_dn_array(tls_handshake_t hdsk)
{
    tls_buffer_list_t *dn_list = (tls_buffer_list_t *)tls_handshake_get_peer_acceptable_dn_list(hdsk);
    return tls_helper_create_cfarray_from_buffer_list(dn_list);
}
