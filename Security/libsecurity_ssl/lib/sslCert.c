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

#include "ssl.h"
#include "sslContext.h"
#include "sslHandshake.h"
#include "sslMemory.h"
#include "sslAlertMessage.h"
#include "sslDebug.h"
#include "sslUtils.h"
#include "sslDigests.h"
#include "sslCrypto.h"

#include <string.h>
#include <assert.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/oidsalg.h>
#include "utilities/SecCFRelease.h"


OSStatus
SSLEncodeCertificate(SSLRecord *certificate, SSLContext *ctx)
{   OSStatus            err;
    size_t              totalLength;
    UInt8               *charPtr;
    CFIndex             i, certCount;
#ifdef USE_SSLCERTIFICATE
    int                 j;
    SSLCertificate      *cert;
#else
    CFArrayRef			certChain;
#endif
    int                 head;

    /*
	 * TBD: for client side, match Match DER-encoded acceptable DN list
	 * (ctx->acceptableDNList) to one of our certs. For now we just send
	 * what we have since we don't support multiple certs.
	 *
	 * Note this can be called with localCert==0 for client side in TLS1+ and DTLS;
	 * in that case we send an empty cert msg.
	 */
	assert(ctx->negProtocolVersion >= SSL_Version_3_0);
	assert((ctx->localCert != NULL) || (ctx->negProtocolVersion >= TLS_Version_1_0));
    totalLength = 0;

#ifdef USE_SSLCERTIFICATE
    certCount = 0;
    cert = ctx->localCert;
    while (cert)
    {   totalLength += 3 + cert->derCert.length;    /* 3 for encoded length field */
        ++certCount;
        cert = cert->next;
    }
#else
    certChain = ctx->localCert;
	certCount = certChain ? CFArrayGetCount(certChain) : 0;
	for (i = 0; i < certCount; ++i) {
		SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex(certChain, i);
		totalLength += 3 + SecCertificateGetLength(cert);    /* 3 for encoded length field */
	}
#endif
    certificate->contentType = SSL_RecordTypeHandshake;
    certificate->protocolVersion = ctx->negProtocolVersion;
    head = SSLHandshakeHeaderSize(certificate);
    if ((err = SSLAllocBuffer(&certificate->contents, totalLength + head + 3)))
        return err;

    charPtr = SSLEncodeHandshakeHeader(ctx, certificate, SSL_HdskCert, totalLength+3);

    charPtr = SSLEncodeSize(charPtr, totalLength, 3);      /* Vector length */

#ifdef USE_SSLCERTIFICATE
    /* Root cert is first in the linked list, but has to go last,
	 * so walk list backwards */
    for (i = 0; i < certCount; ++i)
    {   cert = ctx->localCert;
        for (j = i+1; j < certCount; ++j)
            cert = cert->next;
        charPtr = SSLEncodeSize(charPtr, cert->derCert.length, 3);
        memcpy(charPtr, cert->derCert.data, cert->derCert.length);
        charPtr += cert->derCert.length;
    }
#else
    /* Root cert is last in the array, and has to go last,
	 * so walk list forwards */
	for (i = 0; i < certCount; ++i) {
		SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex(certChain, i);
		CFIndex certLength = SecCertificateGetLength(cert);
        charPtr = SSLEncodeSize(charPtr, certLength, 3);
        memcpy(charPtr, SecCertificateGetBytePtr(cert), certLength);
        charPtr += certLength;
    }
#endif

    assert(charPtr == certificate->contents.data + certificate->contents.length);

    if ((ctx->protocolSide == kSSLClientSide) && (ctx->localCert)) {
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
    return errSecSuccess;
}

OSStatus
SSLProcessCertificate(SSLBuffer message, SSLContext *ctx)
{
    size_t          listLen, certLen;
    UInt8           *p;
    OSStatus        err;
#ifdef USE_SSLCERTIFICATE
    SSLCertificate      *cert;
#else
    CFMutableArrayRef   certChain = NULL;
    SecCertificateRef   cert;
#endif

    p = message.data;
    listLen = SSLDecodeInt(p,3);
    p += 3;
    if (listLen + 3 != message.length) {
    	sslErrorLog("SSLProcessCertificate: length decode error 1\n");
        return errSSLProtocol;
    }

    while (listLen > 0)
    {   certLen = SSLDecodeInt(p,3);
        p += 3;
        if (listLen < certLen + 3) {
    		sslErrorLog("SSLProcessCertificate: length decode error 2\n");
            return errSSLProtocol;
        }
#ifdef USE_SSLCERTIFICATE
		cert = (SSLCertificate *)sslMalloc(sizeof(SSLCertificate));
		if(cert == NULL) {
			return errSecAllocate;
		}
        if ((err = SSLAllocBuffer(&cert->derCert, certLen)
        {   sslFree(cert);
            return err;
        }
        memcpy(cert->derCert.data, p, certLen);
        p += certLen;
        cert->next = ctx->peerCert;     /* Insert backwards; root cert
										 * will be first in linked list */
        ctx->peerCert = cert;
#else
		if (!certChain) {
			certChain = CFArrayCreateMutable(kCFAllocatorDefault, 0,
				&kCFTypeArrayCallBacks);
			if (certChain == NULL) {
				return errSecAllocate;
			}
			if (ctx->peerCert) {
				sslDebugLog("SSLProcessCertificate: releasing existing cert chain\n");
				CFRelease(ctx->peerCert);
			}
			ctx->peerCert = certChain;
		}
 		cert = SecCertificateCreateWithBytes(NULL, p, certLen);
		#if SSL_DEBUG && !TARGET_OS_IPHONE
		{
			/* print cert name when debugging; leave disabled otherwise */
			CFStringRef certName = NULL;
			OSStatus status = SecCertificateInferLabel(cert, &certName);
			char buf[1024];
			if (status || !certName || !CFStringGetCString(certName, buf, 1024-1, kCFStringEncodingUTF8)) { buf[0]=0; }
			sslDebugLog("SSLProcessCertificate: err=%d, received \"%s\" (%ld bytes)\n",(int) status, buf, certLen);
			CFReleaseSafe(certName);
		}
		#endif
		if (cert == NULL) {
			sslErrorLog("SSLProcessCertificate: unable to create cert ref from data\n");
			return errSecAllocate;
		}
        p += certLen;
		/* Insert forwards; root cert will be last in linked list */
		CFArrayAppendValue(certChain, cert);
		CFRelease(cert);
#endif
        listLen -= 3+certLen;
    }
    assert(p == message.data + message.length && listLen == 0);

    if (!ctx->peerCert) {
		/* this *might* be OK... */
		if((ctx->protocolSide == kSSLServerSide) &&
		   (ctx->clientAuth != kAlwaysAuthenticate)) {
			/*
			 * we tried to authenticate, client doesn't have a cert, and
			 * app doesn't require it. OK.
			 */
			return errSecSuccess;
		}
		else {
			AlertDescription desc;
			if(ctx->negProtocolVersion == SSL_Version_3_0) {
				/* this one's for SSL3 only */
				desc = SSL_AlertBadCert;
			}
			else {
				desc = SSL_AlertCertUnknown;
			}
			SSLFatalSessionAlert(desc, ctx);
			return errSSLXCertChainInvalid;
		}
    }

    if((err = sslVerifyCertChain(ctx, ctx->peerCert, true)) != 0) {
        AlertDescription desc;
        switch(err) {
        case errSSLUnknownRootCert:
        case errSSLNoRootCert:
            desc = SSL_AlertUnknownCA;
            break;
        case errSSLCertExpired:
        case errSSLCertNotYetValid:
            desc = SSL_AlertCertExpired;
            break;
        case errSSLXCertChainInvalid:
        default:
            desc = SSL_AlertCertUnknown;
            break;
        }
        SSLFatalSessionAlert(desc, ctx);
    }

    if (err == errSecSuccess) {
        if(ctx->peerPubKey != NULL) {
            /* renegotiating - free old key first */
            sslFreePubKey(&ctx->peerPubKey);
        }
        err = sslCopyPeerPubKey(ctx, &ctx->peerPubKey);
    }

    /* Now that cert verification is done, update context state */
    /* (this code was formerly in SSLProcessHandshakeMessage, */
    /* directly after the return from SSLProcessCertificate) */
    if(ctx->protocolSide == kSSLServerSide) {
        if(err) {
            /*
             * Error could be from no cert (when we require one)
             * or invalid cert
             */
            if(ctx->peerCert != NULL) {
                ctx->clientCertState = kSSLClientCertRejected;
            }
        } else if(ctx->peerCert != NULL) {
            /*
             * This still might change if cert verify msg
             * fails. Note we avoid going to state
             * if we get en empty cert message which is
             * otherwise valid.
             */
            ctx->clientCertState = kSSLClientCertSent;
        }

        /*
         * Schedule return to the caller to verify the client's identity.
         * Note that an error during processing will cause early
         * termination of the handshake.
         */
        if (ctx->breakOnClientAuth) {
            ctx->signalClientAuth = true;
        }
    } else {
        /*
         * Schedule return to the caller to verify the server's identity.
         * Note that an error during processing will cause early
         * termination of the handshake.
         */
        if (ctx->breakOnServerAuth) {
            ctx->signalServerAuth = true;
        }
    }

    return err;
}

OSStatus
SSLEncodeCertificateRequest(SSLRecord *request, SSLContext *ctx)
{
	OSStatus    err;
    size_t      shListLen = 0, dnListLen, msgLen;
    UInt8       *charPtr;
    DNListElem  *dn;
    int         head;

	assert(ctx->protocolSide == kSSLServerSide);
    if (sslVersionIsLikeTls12(ctx)) {
        shListLen = 2 + 2 * (ctx->ecdsaEnable ? 5 : 3);  //FIXME: 5:3 should not be hardcoded here.
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

    request->contentType = SSL_RecordTypeHandshake;
    assert(ctx->negProtocolVersion >= SSL_Version_3_0);

    request->protocolVersion = ctx->negProtocolVersion;
    head = SSLHandshakeHeaderSize(request);
    if ((err = SSLAllocBuffer(&request->contents, msgLen + head)))
        return err;

    charPtr = SSLEncodeHandshakeHeader(ctx, request, SSL_HdskCertRequest, msgLen);

    *charPtr++ = 2;        /* two cert types */
    *charPtr++ = SSLClientAuth_RSASign;
    *charPtr++ = SSLClientAuth_ECDSASign;

    if (shListLen) {
        /* Encode the supported_signature_algorithms added in TLS1.2 */
        /* We dont support SHA512 or SHA224 because we didnot implement the digest abstraction for those
           and we dont keep a running hash for those.
           We dont support SHA384/ECDSA because corecrypto ec does not support it with 256 bits curves */
        charPtr = SSLEncodeSize(charPtr, shListLen - 2, 2);
        // *charPtr++ = SSL_HashAlgorithmSHA512;
        // *charPtr++ = SSL_SignatureAlgorithmRSA;
        *charPtr++ = SSL_HashAlgorithmSHA384;
        *charPtr++ = SSL_SignatureAlgorithmRSA;
        *charPtr++ = SSL_HashAlgorithmSHA256;
        *charPtr++ = SSL_SignatureAlgorithmRSA;
        // *charPtr++ = SSL_HashAlgorithmSHA224;
        // *charPtr++ = SSL_SignatureAlgorithmRSA;
        *charPtr++ = SSL_HashAlgorithmSHA1;
        *charPtr++ = SSL_SignatureAlgorithmRSA;
        if (ctx->ecdsaEnable) {
            // *charPtr++ = SSL_HashAlgorithmSHA512;
            // *charPtr++ = SSL_SignatureAlgorithmECDSA;
            // *charPtr++ = SSL_HashAlgorithmSHA384;
            // *charPtr++ = SSL_SignatureAlgorithmECDSA;
            *charPtr++ = SSL_HashAlgorithmSHA256;
            *charPtr++ = SSL_SignatureAlgorithmECDSA;
            // *charPtr++ = SSL_HashAlgorithmSHA224;
            // *charPtr++ = SSL_SignatureAlgorithmECDSA;
            *charPtr++ = SSL_HashAlgorithmSHA1;
            *charPtr++ = SSL_SignatureAlgorithmECDSA;
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

    assert(charPtr == request->contents.data + request->contents.length);
    return errSecSuccess;
}

#define SSL_ENABLE_ECDSA_SIGN_AUTH			0
#define SSL_ENABLE_RSA_FIXED_ECDH_AUTH		0
#define SSL_ENABLE_ECDSA_FIXED_ECDH_AUTH	0

OSStatus
SSLProcessCertificateRequest(SSLBuffer message, SSLContext *ctx)
{
    unsigned        i;
    unsigned	    typeCount;
    unsigned		shListLen = 0;
    UInt8           *charPtr;
    unsigned		dnListLen;
	unsigned		dnLen;
    SSLBuffer		dnBuf;
    DNListElem		*dn;
	OSStatus		err;

	/*
	 * Cert request only happens in during client authentication.
	 * We'll send a client cert if we have an appropriate one, but
	 * we don't do any DNList compare.
	 */
    unsigned minLen = (sslVersionIsLikeTls12(ctx)) ? 5 : 3;
    if (message.length < minLen) {
    	sslErrorLog("SSLProcessCertificateRequest: length decode error 1\n");
        return errSSLProtocol;
    }
    charPtr = message.data;
    typeCount = *charPtr++;
    if (typeCount < 1 || message.length < minLen + typeCount) {
    	sslErrorLog("SSLProcessCertificateRequest: length decode error 2\n");
        return errSSLProtocol;
    }
	if(typeCount != 0) {
		/* Store server-specified auth types */
		if(ctx->clientAuthTypes != NULL) {
			sslFree(ctx->clientAuthTypes);
		}
		ctx->clientAuthTypes = (SSLClientAuthenticationType *)
			sslMalloc(typeCount * sizeof(SSLClientAuthenticationType));
		for(i=0; i<typeCount; i++) {
			sslLogNegotiateDebug("===Server specifies authType %d", (int)(*charPtr));
			ctx->clientAuthTypes[i] = (SSLClientAuthenticationType)(*charPtr++);
		}
		ctx->numAuthTypes = typeCount;
    }

    if (sslVersionIsLikeTls12(ctx)) {
        /* Parse the supported_signature_algorithms field added in TLS1.2 */
        shListLen = SSLDecodeInt(charPtr, 2);
        charPtr += 2;
        if (message.length < minLen + typeCount + shListLen) {
            sslErrorLog("SSLProcessCertificateRequest: length decode error 3\n");
            return errSSLProtocol;
        }

        if (shListLen & 1) {
            sslErrorLog("SSLProcessCertificateRequest: signAlg len odd\n");
            return errSSLProtocol;
        }
		ctx->numServerSigAlgs = shListLen / 2;
		if(ctx->serverSigAlgs != NULL) {
			sslFree(ctx->serverSigAlgs);
		}
		ctx->serverSigAlgs = (SSLSignatureAndHashAlgorithm *)
        sslMalloc((ctx->numServerSigAlgs) * sizeof(SSLSignatureAndHashAlgorithm));
		for(i=0; i<ctx->numServerSigAlgs; i++) {
            /* TODO: Validate hash and signature fields. */
			ctx->serverSigAlgs[i].hash = *charPtr++;
			ctx->serverSigAlgs[i].signature = *charPtr++;
			sslLogNegotiateDebug("===Server specifies sigAlg %d %d",
                                 ctx->serverSigAlgs[i].hash,
                                 ctx->serverSigAlgs[i].signature);
		}
    }

	/* if a client cert is set, it must match a server-specified auth type */
	err = SSLUpdateNegotiatedClientAuthType(ctx);

	/* obtain server's DNList */
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

    return errSecSuccess;
}


/* TODO: this should be refactored with FindSigAlg in sslKeyExchange.c */
static
OSStatus FindCertSigAlg(SSLContext *ctx,
                        SSLSignatureAndHashAlgorithm *alg)
{
    unsigned i;

    assert(ctx->protocolSide == kSSLClientSide);
    assert(ctx->negProtocolVersion >= TLS_Version_1_2);
    assert(!ctx->isDTLS);

    if((ctx->numServerSigAlgs==0) ||(ctx->serverSigAlgs==NULL))
        return errSSLInternal;

    for(i=0; i<ctx->numServerSigAlgs; i++) {
        alg->hash = ctx->serverSigAlgs[i].hash;
        alg->signature = ctx->serverSigAlgs[i].signature;
        // We only support RSA cert for our own certs.
        if(ctx->serverSigAlgs[i].signature != SSL_SignatureAlgorithmRSA)
            continue;

        //Let's only support SHA1 and SHA256. SHA384 does not work with 512 bits RSA keys
        // We should actually test against what the client cert can do.
        if((alg->hash==SSL_HashAlgorithmSHA1) || (alg->hash==SSL_HashAlgorithmSHA256)) {
            return errSecSuccess;
        }
    }
    // We could not find a supported signature and hash algorithm
    return errSSLProtocol;
}

OSStatus
SSLEncodeCertificateVerify(SSLRecord *certVerify, SSLContext *ctx)
{   OSStatus        err;
    UInt8           hashData[SSL_MAX_DIGEST_LEN];
    SSLBuffer       hashDataBuf;
    size_t          len;
    size_t		    outputLen;
    UInt8           *charPtr;
    int             head;
    size_t          maxSigLen;
    bool            isRSA = false;

    certVerify->contents.data = 0;
    hashDataBuf.data = hashData;
    hashDataBuf.length = SSL_MAX_DIGEST_LEN;


	assert(ctx->signingPrivKeyRef != NULL);
    err = sslGetMaxSigSize(ctx->signingPrivKeyRef, &maxSigLen);
    if(err) {
        goto fail;
    }

	switch(ctx->negAuthType) {
		case SSLClientAuth_RSASign:
            isRSA = true;
			break;
#if SSL_ENABLE_ECDSA_SIGN_AUTH
		case SSLClientAuth_ECDSASign:
			break;
#endif
		default:
			/* shouldn't be here */
			assert(0);
			return errSSLInternal;
	}

    certVerify->contentType = SSL_RecordTypeHandshake;
	assert(ctx->negProtocolVersion >= SSL_Version_3_0);
    certVerify->protocolVersion = ctx->negProtocolVersion;
    head = SSLHandshakeHeaderSize(certVerify);

    outputLen = maxSigLen + head + 2;

    SSLSignatureAndHashAlgorithm sigAlg;

    if (sslVersionIsLikeTls12(ctx)) {
        err=FindCertSigAlg(ctx, &sigAlg);
        if(err)
            goto fail;
        outputLen += 2;
    }

    assert(ctx->sslTslCalls != NULL);
    if ((err = ctx->sslTslCalls->computeCertVfyMac(ctx, &hashDataBuf, sigAlg.hash)) != 0)
        goto fail;

    if ((err = SSLAllocBuffer(&certVerify->contents, outputLen)) != 0)
        goto fail;

    /* Sign now to get the actual length */
    charPtr = certVerify->contents.data+head;

    if (sslVersionIsLikeTls12(ctx))
    {
        *charPtr++ = sigAlg.hash;
        *charPtr++ = sigAlg.signature;

        /* We don't support anything but RSA for client side auth yet */
        assert(isRSA);
        SecAsn1AlgId algId;
        switch (sigAlg.hash) {
            case SSL_HashAlgorithmSHA384:
                algId.algorithm = CSSMOID_SHA384WithRSA;
                break;
            case SSL_HashAlgorithmSHA256:
                algId.algorithm = CSSMOID_SHA256WithRSA;
                break;
            case SSL_HashAlgorithmSHA1:
                algId.algorithm = CSSMOID_SHA1WithRSA;
                break;
            default:
				sslErrorLog("SSLEncodeCertificateVerify: unsupported signature hash algorithm (%d)\n",
					sigAlg.hash);
                assert(0);          // if you get here, something is wrong in FindCertSigAlg
                err=errSSLInternal;
                goto fail;
        }

        err = sslRsaSign(ctx,
                         ctx->signingPrivKeyRef,
                         &algId,
                         hashData,
                         hashDataBuf.length,
                         charPtr+2,
                         maxSigLen,
                         &outputLen);
        len=outputLen+2+2;
    } else {
        err = sslRawSign(ctx,
            ctx->signingPrivKeyRef,
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
	certVerify->contents.length = len + head;

    /* charPtr point at the len field here */
    charPtr = SSLEncodeSize(charPtr, outputLen, 2);
    charPtr = SSLEncodeHandshakeHeader(ctx, certVerify, SSL_HdskCertVerify, len);

    assert(charPtr==(certVerify->contents.data+head));

    err = errSecSuccess;

fail:

    return err;
}

OSStatus
SSLProcessCertificateVerify(SSLBuffer message, SSLContext *ctx)
{   OSStatus        err;
    UInt8           hashData[SSL_MAX_DIGEST_LEN];
    size_t          signatureLen;
    SSLBuffer       hashDataBuf;
    size_t          publicModulusLen;
    uint8_t         *charPtr = message.data;
	uint8_t         *endCp = charPtr + message.length;

    SSLSignatureAndHashAlgorithm    sigAlg;
    SecAsn1AlgId                    algId;

    if (ctx->isDTLS
        ? ctx->negProtocolVersion < DTLS_Version_1_0
        : ctx->negProtocolVersion >= TLS_Version_1_2) {
        /* Parse the algorithm field added in TLS1.2 */
        if((charPtr+2) > endCp) {
            sslErrorLog("SSLProcessCertificateVerify: msg len error 1\n");
            return errSSLProtocol;
        }
        sigAlg.hash = *charPtr++;
        sigAlg.signature = *charPtr++;

        switch (sigAlg.hash) {
            case SSL_HashAlgorithmSHA256:
                algId.algorithm = CSSMOID_SHA256WithRSA;
                if(ctx->selectedCipherSpecParams.macAlg == HA_SHA384) {
                    sslErrorLog("SSLProcessCertificateVerify: inconsistent hash, HA_SHA384\n");
                    return errSSLInternal;
                }
                break;
            case SSL_HashAlgorithmSHA384:
                algId.algorithm = CSSMOID_SHA384WithRSA;
                if(ctx->selectedCipherSpecParams.macAlg != HA_SHA384) {
                    sslErrorLog("SSLProcessCertificateVerify: inconsistent hash, %d not HA_SHA384\n", ctx->selectedCipherSpecParams.macAlg);
                    return errSSLInternal;
                }
                break;
            default:
                sslErrorLog("SSLProcessCertificateVerify: unsupported hash %d\n", sigAlg.hash);
                return errSSLProtocol;
        }
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

	publicModulusLen = sslPubKeyLengthInBytes(ctx->peerPubKey);

	#if 0
    if (signatureLen != publicModulusLen) {
    	sslErrorLog("SSLProcessCertificateVerify: sig len error 2\n");
        return errSSLProtocol;
    }
	#endif
	if (publicModulusLen == 0) {
		sslErrorLog("SSLProcessCertificateVerify: pub key modulus is 0\n");
	}

    hashDataBuf.data = hashData;
    hashDataBuf.length = SSL_MAX_DIGEST_LEN;

	assert(ctx->sslTslCalls != NULL);
    if ((err = ctx->sslTslCalls->computeCertVfyMac(ctx, &hashDataBuf, sigAlg.hash)) != 0)
        goto fail;

    if (sslVersionIsLikeTls12(ctx))
    {
        if(sigAlg.signature==SSL_SignatureAlgorithmRSA) {
            err = sslRsaVerify(ctx,
                               ctx->peerPubKey,
                               &algId,
                               hashData,
                               hashDataBuf.length,
                               charPtr,
                               signatureLen);
        } else {
            err = sslRawVerify(ctx,
                               ctx->peerPubKey,
                               hashData,
                               hashDataBuf.length,
                               charPtr,
                               signatureLen);
        }
    } else {
        /* sslRawVerify does the decrypt & compare for us in one shot. */
        err = sslRawVerify(ctx,
            ctx->peerPubKey,
            hashData,				// data to verify
            hashDataBuf.length,
            charPtr, 		// signature
            signatureLen);
    }

    if(err) {
		SSLFatalSessionAlert(SSL_AlertDecryptError, ctx);
		goto fail;
	}
    err = errSecSuccess;

fail:
    return err;
}
