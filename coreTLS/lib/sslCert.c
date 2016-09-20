/*
 * Copyright (c) 1999-2001,2005-2012 Apple Inc. All Rights Reserved.
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
 * sslCert.c - certificate request/verify messages
 */

#include "tls_handshake_priv.h"
#include "sslHandshake.h"
#include "sslMemory.h"
#include "sslAlertMessage.h"
#include "sslDebug.h"
#include "sslUtils.h"
#include "sslDigests.h"
#include "sslCrypto.h"

#include <string.h>
#include <assert.h>

int SSLFreeCertificates(SSLCertificate *certs)
{
    return tls_free_buffer_list((tls_buffer_list_t *)certs);
}

int SSLFreeDNList(DNListElem *dn)
{
    return tls_free_buffer_list((tls_buffer_list_t *)dn);
}

int
SSLEncodeCertificate(tls_buffer *certificate, tls_handshake_t ctx)
{
    int                 err;
    size_t              totalLength;
    uint8_t             *charPtr;
    int                 certCount;
    SSLCertificate      *cert;
    int                 head;

    /*
	 * Note this can be called with localCert==0 for client side in TLS1+ and DTLS;
	 * in that case we send an empty cert msg.
	 */
	assert(ctx->negProtocolVersion >= tls_protocol_version_SSL_3);
	assert((ctx->localCert != NULL) || (ctx->negProtocolVersion >= tls_protocol_version_TLS_1_0));
    totalLength = 0;
    certCount = 0;

    /* If we are the client and didn't select an auth type, we will send an empty message */
    if(ctx->isServer || ctx->negAuthType != tls_client_auth_type_None) {
        cert = ctx->localCert;
        while (cert)
        {   totalLength += 3 + cert->derCert.length;    /* 3 for encoded length field */
            ++certCount;
            cert = cert->next;
        }
        cert = ctx->localCert;
    } else {
        certCount = 0;
        cert = NULL;
    }

    head = SSLHandshakeHeaderSize(ctx);
    if ((err = SSLAllocBuffer(certificate, totalLength + head + 3)))
        return err;

    charPtr = SSLEncodeHandshakeHeader(ctx, certificate, SSL_HdskCert, totalLength+3);

    charPtr = SSLEncodeSize(charPtr, totalLength, 3);      /* Vector length */

    /* Leaf cert is first in the linked list */
    while(cert) {
        charPtr = SSLEncodeSize(charPtr, cert->derCert.length, 3);
        memcpy(charPtr, cert->derCert.data, cert->derCert.length);
        charPtr += cert->derCert.length;
        cert = cert->next;
    }

    assert(charPtr == certificate->data + certificate->length);

    if ((!ctx->isServer) && (ctx->negAuthType != tls_client_auth_type_None)) {
		/* this tells us to send a CertificateVerify msg after the
		 * client key exchange. We skip the cert vfy if we just
		 * sent an empty cert msg (i.e., we were asked for a cert
		 * but we don't have one). */
        ctx->certSent = 1;
		assert(ctx->clientCertState == kSSLClientCertRequested);
		assert(ctx->certRequested);
		ctx->clientCertState = kSSLClientCertSent;
	}
	if(certCount == 0) {
		sslCertDebug("...sending empty cert msg");
	}
    return errSSLSuccess;
}

static bool
CertificateChainEqual(SSLCertificate *cert1, SSLCertificate *cert2)
{
    do {
        if(cert1 == NULL || cert2 == NULL)
            return false;
        if(cert1->derCert.length != cert2->derCert.length)
            return false;
        if(memcmp(cert1->derCert.data, cert2->derCert.data, cert1->derCert.length) != 0)
            return false;
        cert1 = cert1->next;
        cert2 = cert2->next;
    } while (cert1 != NULL || cert2 != NULL);

    return true;
}

static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


static void
debug_log_chain(const char *scope, SSLCertificate *cert)
{
    size_t n, m, count = 0;
    while(cert) {
        char line[65];
        uint32_t c;

        ssl_secdebug(scope, "cert: %lu", (unsigned long)count++);
        ssl_secdebug(scope, "-----BEGIN CERTIFICATE-----");
        m = n = 0;
        while (n < cert->derCert.length) {
            c = cert->derCert.data[n];
            n++;

            c = c << 8;
            if (n < cert->derCert.length)
                c |= cert->derCert.data[n];
            n++;

            c = c << 8;
            if (n < cert->derCert.length)
                c |= cert->derCert.data[n];
            n++;

            line[m++] = base64_chars[(c & 0x00fc0000) >> 18];
            line[m++] = base64_chars[(c & 0x0003f000) >> 12];
            if (n > cert->derCert.length + 1)
                line[m++] = '=';
            else
                line[m++] = base64_chars[(c & 0x00000fc0) >> 6];
            if (n > cert->derCert.length)
                line[m++] = '=';
            else
                line[m++] = base64_chars[(c & 0x0000003f) >> 0];
            if (m == sizeof(line) - 1) {
                line[sizeof(line) - 1] = '\0';
                ssl_secdebug(scope, "%s", line);
                m = 0;
            }
            assert(m < sizeof(line) - 1);
        }
        if (m) {
            line[m] = '\0';
            ssl_secdebug(scope, "%s", line);
        }
        ssl_secdebug(scope, "-----END CERTIFICATE-----");

        cert = cert->next;
    }
}

int
SSLProcessCertificate(tls_buffer message, tls_handshake_t ctx)
{
    size_t          listLen;
    UInt8           *p;
    int        err = 0;
    SSLCertificate *certChain;

    if (message.length < 3) {
        sslErrorLog("SSLProcessCertificate: message length decode error\n");
        return errSSLProtocol;
    }

    p = message.data;
    listLen = SSLDecodeInt(p,3);
    p += 3;
    if (listLen + 3 != message.length) {
    	sslErrorLog("SSLProcessCertificate: length decode error 1\n");
        return errSSLProtocol;
    }

    // Note: An empty certificate list (listLen==0) is allowed by the TLS RFC,
    // but empty certificates (certLen==0) are not. Section 7.4.2 in RFC 5246
    // defines the message syntax as such:
    //
    //  opaque ASN.1Cert<1..2^24-1>;
    //
    //  struct {
    //        ASN.1Cert certificate_list<0..2^24-1>;
    //  } Certificate;
    //
    // Note the difference between <1..2^24-1> and <0..2^24-1>
    //

    if((err = SSLDecodeBufferList(p, listLen, 3, (tls_buffer_list_t **)&certChain))) {
        return err;
    }
    p+=listLen;

    /* Do not accept a different server cert during renegotiation unless allowed */
    if(!ctx->allowServerIdentityChange && ctx->peerCert && !CertificateChainEqual(ctx->peerCert, certChain)) {
        sslErrorLog("Illegal server identity change during renegotiation\n");
        SSLFreeCertificates(certChain);
        return errSSLProtocol;
    }

    if (ctx->peerCert == NULL && __ssl_debug_enabled("sslLogNegotiateDebug")) {
        debug_log_chain("sslLogNegotiateDebug", certChain);
    }

    /* Free certs if they already exist */
    SSLFreeCertificates(ctx->peerCert);
    ctx->peerCert=certChain;

    assert(p == message.data + message.length);

    /* Don't fail here if peerCert is NULL.
       An empty Certificate message is valid in some cases.
       The rest of the stack will handle it. */

    return err;
}


int
SSLEncodeCertificateStatus(tls_buffer *status, tls_handshake_t ctx)
{
    int                 err;
    size_t              totalLength;
    uint8_t             *charPtr;
    int                 head;

    assert(ctx->isServer);
    assert(ctx->ocsp_enabled && ctx->ocsp_peer_enabled);

    if(ctx->ocsp_response.length==0) {
        return errSSLInternal;
    }

    totalLength = 1 + 3 + ctx->ocsp_response.length;

    head = SSLHandshakeHeaderSize(ctx);

    if ((err = SSLAllocBuffer(status, totalLength + head)))
        return err;

    charPtr = SSLEncodeHandshakeHeader(ctx, status, SSL_HdskCertificateStatus, totalLength);

    *charPtr++ = SSL_CST_Ocsp;
    charPtr = SSLEncodeSize(charPtr, ctx->ocsp_response.length, 3);
    memcpy(charPtr, ctx->ocsp_response.data, ctx->ocsp_response.length);

    return 0;
}

int
SSLProcessCertificateStatus(tls_buffer message, tls_handshake_t ctx)
{
    uint8_t status_type;
    uint8_t *p = message.data;
    assert(!ctx->isServer);

    if (message.length < 1) {
        sslErrorLog("SSLProcessCertificateStatus: message length decode error (1)\n");
        return errSSLProtocol;
    }

    status_type = *p++;

    if(status_type!=SSL_CST_Ocsp) {
        return noErr;
    }

    if (message.length < 3) {
        sslErrorLog("SSLProcessCertificateStatus: message length decode error (2)\n");
        return errSSLProtocol;
    }

    size_t OCSPResponseLen = SSLDecodeSize(p, 3); p+=3;

    if(OCSPResponseLen==0) {
        sslErrorLog("SSLProcessCertificateStatus: message length decode error (3)\n");
        return errSSLProtocol;
    }

    if(OCSPResponseLen+4 != message.length) {
        sslErrorLog("SSLProcessCertificateStatus: message length decode error (4)\n");
        return errSSLProtocol;
    }

    ctx->ocsp_response_received = true;

    SSLFreeBuffer(&ctx->ocsp_response);
    return SSLCopyBufferFromData(p, OCSPResponseLen, &ctx->ocsp_response);
}

int
SSLEncodeCertificateRequest(tls_buffer *request, tls_handshake_t ctx)
{
	int    err;
    size_t      shListLen = 0, dnListLen, msgLen;
    UInt8       *charPtr;
    DNListElem  *dn;
    int         head;

	assert(ctx->isServer);
    if (sslVersionIsLikeTls12(ctx)) {
        shListLen = 2 + 2 * ctx->numLocalSigAlgs;
    }

	dnListLen = 0;
    dn = ctx->acceptableDNList;
    while (dn)
    {   dnListLen += 2 + dn->derDN.length;
        dn = dn->next;
    }
    msgLen = 1 +	// number of cert types
			 2 +	// cert types
             shListLen +  // SignatureAlgorithms
			 2 +	// length of DN list
			 dnListLen;

    assert(ctx->negProtocolVersion >= tls_protocol_version_SSL_3);

    head = SSLHandshakeHeaderSize(ctx);
    if ((err = SSLAllocBuffer(request, msgLen + head)))
        return err;

    charPtr = SSLEncodeHandshakeHeader(ctx, request, SSL_HdskCertRequest, msgLen);

    *charPtr++ = 2;        /* two cert types */
    *charPtr++ = tls_client_auth_type_RSASign;
    *charPtr++ = tls_client_auth_type_ECDSASign;

    if (shListLen) {
        /* Encode the supported_signature_algorithms added in TLS1.2 */
        charPtr = SSLEncodeSize(charPtr, shListLen - 2, 2);
        for(int i=0; i<ctx->numLocalSigAlgs; i++) {
            charPtr = SSLEncodeInt(charPtr, ctx->localSigAlgs[i].hash, 1);
            charPtr = SSLEncodeInt(charPtr, ctx->localSigAlgs[i].signature, 1);
        }
    }

    charPtr = SSLEncodeSize(charPtr, dnListLen, 2);
    dn = ctx->acceptableDNList;
    while (dn)
    {   charPtr = SSLEncodeSize(charPtr, dn->derDN.length, 2);
        memcpy(charPtr, dn->derDN.data, dn->derDN.length);
        charPtr += dn->derDN.length;
        dn = dn->next;
    }

    assert(charPtr == request->data + request->length);
    return errSSLSuccess;
}

int
SSLProcessCertificateRequest(tls_buffer message, tls_handshake_t ctx)
{
    unsigned        i;
    unsigned	    typeCount;
    unsigned		shListLen = 0;
    UInt8           *charPtr;
    unsigned		dnListLen;
	unsigned		dnLen;
    tls_buffer		dnBuf;
    DNListElem		*dn;
	int		err;

    /*
     * Cert request only happens in during client authentication.
     * Application can send a client cert if they have an appropriate one.
     * coreTLS does not ensure the client cert is appropriate.
     */

    unsigned minLen = (sslVersionIsLikeTls12(ctx)) ? 5 : 3;
    if (message.length < minLen) {
    	sslErrorLog("SSLProcessCertificateRequest: length decode error 1\n");
        return errSSLProtocol;
    }
    charPtr = message.data;
    typeCount = *charPtr++;
    if ((typeCount < 1) || (message.length < minLen + typeCount)) {
    	sslErrorLog("SSLProcessCertificateRequest: length decode error 2\n");
        return errSSLProtocol;
    }

    /* Update the server-specified auth types */
    sslFree(ctx->clientAuthTypes);
    ctx->numAuthTypes = typeCount;
    ctx->clientAuthTypes = (tls_client_auth_type *)
                           sslMalloc(ctx->numAuthTypes * sizeof(tls_client_auth_type));
    if(ctx->clientAuthTypes==NULL)
        return errSSLInternal;

    for(i=0; i<ctx->numAuthTypes; i++) {
        sslLogNegotiateDebug("===Server specifies authType %d", (int)(*charPtr));
        ctx->clientAuthTypes[i] = (tls_client_auth_type)(*charPtr++);
    }

    if (sslVersionIsLikeTls12(ctx)) {
        /* Parse the supported_signature_algorithms field added in TLS1.2 */
        shListLen = SSLDecodeInt(charPtr, 2);
        charPtr += 2;
        if ((shListLen < 2) || (message.length < minLen + typeCount + shListLen)) {
            sslErrorLog("SSLProcessCertificateRequest: length decode error 3\n");
            return errSSLProtocol;
        }

        if (shListLen & 1) {
            sslErrorLog("SSLProcessCertificateRequest: signAlg len odd\n");
            return errSSLProtocol;
        }

        sslFree(ctx->peerSigAlgs);
        ctx->numPeerSigAlgs = shListLen / 2;
        ctx->peerSigAlgs = (tls_signature_and_hash_algorithm *)
                              sslMalloc((ctx->numPeerSigAlgs) * sizeof(tls_signature_and_hash_algorithm));
        if(ctx->peerSigAlgs==NULL)
            return errSSLInternal;

        for(i=0; i<ctx->numPeerSigAlgs; i++) {
            ctx->peerSigAlgs[i].hash = *charPtr++;
            ctx->peerSigAlgs[i].signature = *charPtr++;
            sslLogNegotiateDebug("===Server specifies sigAlg %d %d",
                                 ctx->peerSigAlgs[i].hash,
                                 ctx->peerSigAlgs[i].signature);
        }
    }

    /* Update the acceptable DNList */
    SSLFreeDNList(ctx->acceptableDNList);
    ctx->acceptableDNList=NULL;

    dnListLen = SSLDecodeInt(charPtr, 2);
    charPtr += 2;
    if (message.length != minLen + typeCount + shListLen + dnListLen) {
    	sslErrorLog("SSLProcessCertificateRequest: length decode error 3\n");
        return errSSLProtocol;
	}
    while (dnListLen > 0)
    {   if (dnListLen < 2) {
		sslErrorLog("SSLProcessCertificateRequest: dnListLen error 1\n");
            return errSSLProtocol;
        }
        dnLen = SSLDecodeInt(charPtr, 2);
        charPtr += 2;
        if (dnListLen < 2 + dnLen) {
     		sslErrorLog("SSLProcessCertificateRequest: dnListLen error 2\n");
           	return errSSLProtocol;
    	}
        if ((err = SSLAllocBuffer(&dnBuf, sizeof(DNListElem))))
            return err;
        dn = (DNListElem*)dnBuf.data;
        if ((err = SSLAllocBuffer(&dn->derDN, dnLen)))
        {   SSLFreeBuffer(&dnBuf);
            return err;
        }
        memcpy(dn->derDN.data, charPtr, dnLen);
        charPtr += dnLen;
        dn->next = ctx->acceptableDNList;
        ctx->acceptableDNList = dn;
        dnListLen -= 2 + dnLen;
    }

    assert(charPtr == message.data + message.length);

    return errSSLSuccess;
}


/* TODO: this should be refactored with FindSigAlg in sslKeyExchange.c */
static
int FindCertSigAlg(tls_handshake_t ctx, tls_signature_and_hash_algorithm *alg)
{
	assert(!ctx->isServer);
    assert(ctx->negProtocolVersion >= tls_protocol_version_TLS_1_2);
    assert(!ctx->isDTLS);

    if((ctx->numPeerSigAlgs==0) || (ctx->numLocalSigAlgs==0)) {
        assert(0);
        return errSSLInternal;
    }

    //Check for matching server signature algorithm and corresponding hash algorithm
    for(int i=0; i<ctx->numLocalSigAlgs; i++) {
        if (alg->signature != ctx->localSigAlgs[i].signature)
            continue;
        alg->hash = ctx->localSigAlgs[i].hash;
        for(int j=0; j<ctx->numPeerSigAlgs; j++) {
            if (alg->signature != ctx->peerSigAlgs[j].signature)
                continue;
            if(alg->hash == ctx->peerSigAlgs[j].hash) {
                return errSSLSuccess;
            }
        }
    }

    // We could not find a supported signature and hash algorithm
    return errSSLProtocol;
}

int
SSLEncodeCertificateVerify(tls_buffer *certVerify, tls_handshake_t ctx)
{   int        err;
    UInt8           hashData[SSL_MAX_DIGEST_LEN];
    tls_buffer       hashDataBuf;
    size_t          len;
    size_t		    outputLen;
    UInt8           *charPtr;
    int             head;
    size_t          maxSigLen;

    certVerify->data = 0;
    hashDataBuf.data = hashData;
    hashDataBuf.length = SSL_MAX_DIGEST_LEN;


	assert(ctx->signingPrivKeyRef != NULL);
    err = sslGetMaxSigSize(ctx->signingPrivKeyRef, &maxSigLen);
    if(err) {
        goto fail;
    }

    tls_signature_and_hash_algorithm sigAlg = {0,};

	switch(ctx->signingPrivKeyRef->desc.type) {
        case tls_private_key_type_rsa:
            sigAlg.signature = tls_signature_algorithm_RSA;
            break;
        case tls_private_key_type_ecdsa:
            sigAlg.signature = tls_signature_algorithm_ECDSA;
            if (ctx->negProtocolVersion <= tls_protocol_version_SSL_3) {
                return errSSLInternal;
            }
			break;
		default:
			/* shouldn't be here */
			assert(0);
			return errSSLInternal;
	}

	assert(ctx->negProtocolVersion >= tls_protocol_version_SSL_3);
    head = SSLHandshakeHeaderSize(ctx);

    outputLen = maxSigLen + head + 2;

    // Note: this is only used for TLS 1.2
    if (sslVersionIsLikeTls12(ctx)) {
        err=FindCertSigAlg(ctx, &sigAlg);
        if(err)
            goto fail;
        outputLen += 2;
        ctx->certSigAlg = sigAlg; // Save for metrics reporting.
    }

    assert(ctx->sslTslCalls != NULL);
    if ((err = ctx->sslTslCalls->computeCertVfyMac(ctx, &hashDataBuf, sigAlg.hash)) != 0)
        goto fail;

    if ((err = SSLAllocBuffer(certVerify, outputLen)) != 0)
        goto fail;

    /* Sign now to get the actual length */
    charPtr = certVerify->data+head;

    if (sslVersionIsLikeTls12(ctx))
    {
        *charPtr++ = sigAlg.hash;
        *charPtr++ = sigAlg.signature;

        switch (sigAlg.hash) {
            case tls_hash_algorithm_SHA512:
            case tls_hash_algorithm_SHA384:
            case tls_hash_algorithm_SHA256:
            case tls_hash_algorithm_SHA1:
                break;
            default:
				sslErrorLog("SSLEncodeCertificateVerify: unsupported signature hash algorithm (%d)\n",
					sigAlg.hash);
                assert(0);          // if you get here, something is wrong in FindCertSigAlg
                err=errSSLInternal;
                goto fail;
        }

        if (sigAlg.signature == tls_signature_algorithm_RSA) {
            err = sslRsaSign(ctx->signingPrivKeyRef,
                             sigAlg.hash,
                             hashData,
                             hashDataBuf.length,
                             charPtr+2,
                             maxSigLen,
                             &outputLen);
        } else {
            err = sslEcdsaSign(ctx->signingPrivKeyRef,
                             hashData,
                             hashDataBuf.length,
                             charPtr+2,
                             maxSigLen,
                             &outputLen);
        }
        len=outputLen+2+2;
    } else {
        err = sslRawSign(ctx->signingPrivKeyRef,
            hashData,						// data to sign
            hashDataBuf.length,				// Data to sign size
            charPtr+2,	// signature destination
            maxSigLen,							// we mallocd len+head+2
            &outputLen);
        len = outputLen+2;
    }
	if(err) {
		sslErrorLog("SSLEncodeCertificateVerify: unable to sign data (error %d)\n", (int)err);
		goto fail;
	}
    // At this point:
    //  len = message length
    //  outputlen = sig length
	certVerify->length = len + head;

    /* charPtr point at the len field here */
    SSLEncodeSize(charPtr, outputLen, 2);

    SSLEncodeHandshakeHeader(ctx, certVerify, SSL_HdskCertVerify, len);

    err = errSSLSuccess;

fail:

    return err;
}

int
SSLProcessCertificateVerify(tls_buffer message, tls_handshake_t ctx)
{   int        err;
    UInt8           hashData[SSL_MAX_DIGEST_LEN];
    size_t          signatureLen;
    tls_buffer       hashDataBuf;
    uint8_t         *charPtr = message.data;
	uint8_t         *endCp = charPtr + message.length;

    tls_signature_and_hash_algorithm    sigAlg = {0,};

    if (sslVersionIsLikeTls12(ctx)) {
        /* Parse the algorithm field added in TLS1.2 */
        if((charPtr+2) > endCp) {
            sslErrorLog("SSLProcessCertificateVerify: msg len error 1\n");
            return errSSLProtocol;
        }
        sigAlg.hash = *charPtr++;
        sigAlg.signature = *charPtr++;
    }

    if ((charPtr + 2) > endCp) {
    	sslErrorLog("SSLProcessCertificateVerify: msg len error\n");
        return errSSLProtocol;
    }

    signatureLen = SSLDecodeSize(charPtr, 2);
    charPtr += 2;
    if ((charPtr + signatureLen) > endCp) {
    	sslErrorLog("SSLProcessCertificateVerify: sig len error 1\n");
        return errSSLProtocol;
    }

    hashDataBuf.data = hashData;
    hashDataBuf.length = SSL_MAX_DIGEST_LEN;

	assert(ctx->sslTslCalls != NULL);
    if ((err = ctx->sslTslCalls->computeCertVfyMac(ctx, &hashDataBuf, sigAlg.hash)) != 0)
        goto fail;

    if (sslVersionIsLikeTls12(ctx))
    {
        if(sigAlg.signature==tls_signature_algorithm_RSA) {
            err = sslRsaVerify(&ctx->peerPubKey,
                               sigAlg.hash,
                               hashData,
                               hashDataBuf.length,
                               charPtr,
                               signatureLen);
        } else {
            err = sslRawVerify(&ctx->peerPubKey,
                               hashData,
                               hashDataBuf.length,
                               charPtr,
                               signatureLen);
        }
    } else {
        /* sslRawVerify does the decrypt & compare for us in one shot. */
        err = sslRawVerify(&ctx->peerPubKey,
            hashData,				// data to verify
            hashDataBuf.length,
            charPtr, 		// signature
            signatureLen);
    }

    if(err) {
		SSLFatalSessionAlert(SSL_AlertDecryptError, ctx);
		goto fail;
	}
    err = errSSLSuccess;

fail:
    return err;
}
