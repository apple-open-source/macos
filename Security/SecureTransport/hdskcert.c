/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
	File:		hdskcert.c

	Contains:	certificate request/verify messages

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: hdskcert.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: hdskcert.c   Contains support for certificate-related messages

    Support for encoding and decoding the certificate, certificate
    request, and certificate verify messages.

    ****************************************************************** */

#ifndef _SSLCTX_H_
#include "sslctx.h"
#endif

#ifndef _SSLHDSHK_H_
#include "sslhdshk.h"
#endif

#ifndef _SSLALLOC_H_
#include "sslalloc.h"
#endif

#ifndef _SSLALERT_H_
#include "sslalert.h"
#endif

#ifndef	_SSL_DEBUG_H_
#include "sslDebug.h"
#endif

#ifndef _SSLUTIL_H_
#include "sslutil.h"
#endif

#ifndef	_DIGESTS_H_
#include "digests.h"
#endif

#ifndef	_APPLE_CDSA_H_
#include "appleCdsa.h"
#endif

#ifndef	_SSL_DEBUG_H_
#include "sslDebug.h"
#endif

#include <string.h>
#include <assert.h>

SSLErr
SSLEncodeCertificate(SSLRecord *certificate, SSLContext *ctx)
{   SSLErr          err;
    UInt32          totalLength;
    int             i, j, certCount;
    UInt8           *progress;
    SSLCertificate  *cert;
    
    /* Match DER-encoded root certs here */

    cert = ctx->localCert;
    CASSERT(cert != 0);
    totalLength = 0;
    certCount = 0;
    while (cert)
    {   totalLength += 3 + cert->derCert.length;    /* 3 for encoded length field */
        ++certCount;
        cert = cert->next;
    }
    
    certificate->contentType = SSL_handshake;
	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
		   (ctx->negProtocolVersion == TLS_Version_1_0));
    certificate->protocolVersion = ctx->negProtocolVersion;
    if ((err = SSLAllocBuffer(&certificate->contents, totalLength + 7, &ctx->sysCtx)) != 0)
        return err;
    
    progress = certificate->contents.data;
    *progress++ = SSL_certificate;
    progress = SSLEncodeInt(progress, totalLength+3, 3);    /* Handshake message length */
    progress = SSLEncodeInt(progress, totalLength, 3);      /* Vector length */
    
    /* Root cert is first in the linked list, but has to go last, so walk list backwards */
    for (i = 0; i < certCount; ++i)
    {   cert = ctx->localCert;
        for (j = i+1; j < certCount; ++j)
            cert = cert->next;
        progress = SSLEncodeInt(progress, cert->derCert.length, 3);
        memcpy(progress, cert->derCert.data, cert->derCert.length);
        progress += cert->derCert.length;
    }
    
    CASSERT(progress == certificate->contents.data + certificate->contents.length);
    
    if (ctx->protocolSide == SSL_ClientSide)
        ctx->certSent = 1;

    return SSLNoErr;
}

SSLErr
SSLProcessCertificate(SSLBuffer message, SSLContext *ctx)
{   SSLErr          err;
    UInt32          listLen, certLen;
    UInt8           *p;
    SSLCertificate  *cert;
    
    p = message.data;
    listLen = SSLDecodeInt(p,3);
    p += 3;
    if (listLen + 3 != message.length) {
    	errorLog0("SSLProcessCertificate: length decode error 1\n");
        return SSLProtocolErr;
    }
    
    while (listLen > 0)
    {   certLen = SSLDecodeInt(p,3);
        p += 3;
        if (listLen < certLen + 3) {
    		errorLog0("SSLProcessCertificate: length decode error 2\n");
            return SSLProtocolErr;
        }
		cert = (SSLCertificate *)sslMalloc(sizeof(SSLCertificate));
		if(cert == NULL) {
			return SSLMemoryErr;
		}
        if ((err = SSLAllocBuffer(&cert->derCert, certLen, &ctx->sysCtx)) != 0)
        {   sslFree(cert);
            return err;
        }
        memcpy(cert->derCert.data, p, certLen);
        p += certLen;
        cert->next = ctx->peerCert;     /* Insert backwards; root cert will be first in linked list */
        ctx->peerCert = cert;
        listLen -= 3+certLen;
    }
    CASSERT(p == message.data + message.length && listLen == 0);
    
    if (ctx->peerCert == 0)
        return X509CertChainInvalidErr;
    
    if((err = sslVerifyCertChain(ctx, ctx->peerCert)) != 0) 
        return err;

	/* Server's certificate is the last one in the chain */
    cert = ctx->peerCert;
    while (cert->next != 0)
        cert = cert->next;
	/* Convert its public key to CDSA format */
    if ((err = sslPubKeyFromCert(ctx, 
    	&cert->derCert, 
    	&ctx->peerPubKey,
    	&ctx->peerPubKeyCsp)) != 0)
        return err;
        
    return SSLNoErr;
}

SSLErr
SSLEncodeCertificateRequest(SSLRecord *request, SSLContext *ctx)
{   
	#if		!ST_SERVER_MODE_ENABLE
	
	/* cert request only happens in server mode */
	errorLog0("SSLEncodeCertificateRequest called\n");
	return SSLUnsupportedErr;
	
	#else
 
	SSLErr      err;
    UInt32      dnListLen, msgLen;
    UInt8       *progress;
    DNListElem  *dn;
    
	dnListLen = 0;
    dn = ctx->acceptableDNList;
    CASSERT(dn != NULL);
    while (dn)
    {   dnListLen += 2 + dn->derDN.length;
        dn = dn->next;
    }
    msgLen = 1 + 1 + 2 + dnListLen;
    
    request->contentType = SSL_handshake;
	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
		   (ctx->negProtocolVersion == TLS_Version_1_0));
    request->protocolVersion = ctx->negProtocolVersion;
    if ((err = SSLAllocBuffer(&request->contents, msgLen + 4, &ctx->sysCtx)) != 0)
        return err;
    
    progress = request->contents.data;
    *progress++ = SSL_certificate_request;
    progress = SSLEncodeInt(progress, msgLen, 3);
    
    *progress++ = 1;        /* one cert type */
    *progress++ = 1;        /* RSA-sign type */
    progress = SSLEncodeInt(progress, dnListLen, 2);
    dn = ctx->acceptableDNList;
    while (dn)
    {   progress = SSLEncodeInt(progress, dn->derDN.length, 2);
        memcpy(progress, dn->derDN.data, dn->derDN.length);
        progress += dn->derDN.length;
        dn = dn->next;
    }
    
    CASSERT(progress == request->contents.data + request->contents.length);
    
    return SSLNoErr;
	#endif	/* ST_SERVER_MODE_ENABLE */
}

SSLErr
SSLProcessCertificateRequest(SSLBuffer message, SSLContext *ctx)
{   SSLErr          err;
    int             i, dnListLen, dnLen;
    unsigned int    typeCount;
    UInt8           *progress;
    SSLBuffer       dnBuf;
    DNListElem      *dn;
    
	/* cert request only happens in during client authentication, which
	 * we don't do */
	errorLog0("SSLProcessCertificateRequest called\n");
    if (message.length < 3) {
    	errorLog0("SSLProcessCertificateRequest: length decode error 1\n");
        return ERR(SSLProtocolErr);
    }
    progress = message.data;
    typeCount = *progress++;
    if (typeCount < 1 || message.length < 3 + typeCount) {
    	errorLog0("SSLProcessCertificateRequest: length decode error 2\n");
        return ERR(SSLProtocolErr);
    }
    for (i = 0; i < typeCount; i++)
    {   if (*progress++ == 1)
            ctx->x509Requested = 1;
    }
    
    dnListLen = SSLDecodeInt(progress, 2);
    progress += 2;
    if (message.length != 3 + typeCount + dnListLen) {
    	errorLog0("SSLProcessCertificateRequest: length decode error 3\n");
        return ERR(SSLProtocolErr);
	}    
    while (dnListLen > 0)
    {   if (dnListLen < 2) {
    		errorLog0("SSLProcessCertificateRequest: dnListLen error 1\n");
            return ERR(SSLProtocolErr);
        }
        dnLen = SSLDecodeInt(progress, 2);
        progress += 2;
        if (dnListLen < 2 + dnLen) {
     		errorLog0("SSLProcessCertificateRequest: dnListLen error 2\n");
           	return ERR(SSLProtocolErr);
    	}
        if (ERR(err = SSLAllocBuffer(&dnBuf, sizeof(DNListElem), &ctx->sysCtx)) != 0)
            return err;
        dn = (DNListElem*)dnBuf.data;
        if (ERR(err = SSLAllocBuffer(&dn->derDN, dnLen, &ctx->sysCtx)) != 0)
        {   SSLFreeBuffer(&dnBuf, &ctx->sysCtx);
            return err;
        }
        memcpy(dn->derDN.data, progress, dnLen);
        progress += dnLen;
        dn->next = ctx->acceptableDNList;
        ctx->acceptableDNList = dn;
        dnListLen -= 2 + dnLen;
    }
    
    CASSERT(progress == message.data + message.length);
    
    return SSLNoErr;
}

SSLErr
SSLEncodeCertificateVerify(SSLRecord *certVerify, SSLContext *ctx)
{   SSLErr          err;
    UInt8           signedHashData[36];
    SSLBuffer       hashData, shaMsgState, md5MsgState;
    UInt32          len;
    UInt32		    outputLen;
    
    certVerify->contents.data = 0;
    hashData.data = signedHashData;
    hashData.length = 36;
    
    if (ERR(err = CloneHashState(&SSLHashSHA1, ctx->shaState, &shaMsgState, ctx)) != 0)
        goto fail;
    if (ERR(err = CloneHashState(&SSLHashMD5, ctx->md5State, &md5MsgState, ctx)) != 0)
        goto fail;
	assert(ctx->sslTslCalls != NULL);
    if (ERR(err = ctx->sslTslCalls->computeCertVfyMac(ctx,
			hashData, shaMsgState, md5MsgState)) != 0)
        goto fail;
    
	CASSERT(ctx->signingPrivKey != NULL);
	len = sslKeyLengthInBytes(ctx->signingPrivKey);
    
    certVerify->contentType = SSL_handshake;
	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
		   (ctx->negProtocolVersion == TLS_Version_1_0));
    certVerify->protocolVersion = ctx->negProtocolVersion;
    if (ERR(err = SSLAllocBuffer(&certVerify->contents, len + 6, &ctx->sysCtx)) != 0)
        goto fail;
    
    certVerify->contents.data[0] = SSL_certificate_verify;
    SSLEncodeInt(certVerify->contents.data+1, len+2, 3);
    SSLEncodeInt(certVerify->contents.data+4, len, 2);

	err = sslRsaRawSign(ctx,
		ctx->signingPrivKey,
		ctx->signingKeyCsp,
		signedHashData,
		36,				// MD5 size + SHA1 size
		certVerify->contents.data+6,
		len,			// we mallocd len+6
		&outputLen);
	if(err) {
		goto fail;
	}
    
    CASSERT(outputLen == len);
    
    err = SSLNoErr;
    
fail:
    ERR(SSLFreeBuffer(&shaMsgState, &ctx->sysCtx));
    ERR(SSLFreeBuffer(&md5MsgState, &ctx->sysCtx));

    return err;
}

SSLErr
SSLProcessCertificateVerify(SSLBuffer message, SSLContext *ctx)
{   SSLErr          err;
    UInt8           signedHashData[36];
    UInt16          signatureLen;
    SSLBuffer       hashData, shaMsgState, md5MsgState, outputData;
    unsigned int    publicModulusLen;
    
    shaMsgState.data = 0;
    md5MsgState.data = 0;
    outputData.data = 0;
    
    if (message.length < 2) {
    	errorLog0("SSLProcessCertificateVerify: msg len error\n");
        return ERR(SSLProtocolErr);     
    }
    
    signatureLen = (UInt16)SSLDecodeInt(message.data, 2);
    if (message.length != 2 + signatureLen) {
    	errorLog0("SSLProcessCertificateVerify: sig len error 1\n");
        return ERR(SSLProtocolErr);
    }
    
	CASSERT(ctx->peerPubKey != NULL);
	publicModulusLen = sslKeyLengthInBytes(ctx->peerPubKey);
    
    if (signatureLen != publicModulusLen) {
    	errorLog0("SSLProcessCertificateVerify: sig len error 2\n");
        return ERR(SSLProtocolErr);
    }
    outputData.data = 0;
    hashData.data = signedHashData;
    hashData.length = 36;
    
    if (ERR(err = CloneHashState(&SSLHashSHA1, ctx->shaState, &shaMsgState, ctx)) != 0)
        goto fail;
    if (ERR(err = CloneHashState(&SSLHashMD5, ctx->md5State, &md5MsgState, ctx)) != 0)
        goto fail;
	assert(ctx->sslTslCalls != NULL);
    if (ERR(err = ctx->sslTslCalls->computeCertVfyMac(ctx, hashData, 
			shaMsgState, md5MsgState)) != 0)
        goto fail;
    
    if (ERR(err = SSLAllocBuffer(&outputData, publicModulusLen, &ctx->sysCtx)) != 0)
        goto fail;
    
	/* 
	 * The CSP does the decrypt & compare for us in one shot
	 */
	err = sslRsaRawVerify(ctx,
		ctx->peerPubKey,
		ctx->peerPubKeyCsp,		// FIXME - maybe we just use cspHand?
		message.data + 2, 
		signatureLen,
		outputData.data,
		36);
	if(err) {
		goto fail;
	}
    err = SSLNoErr;
    
fail:
    ERR(SSLFreeBuffer(&shaMsgState, &ctx->sysCtx));
    ERR(SSLFreeBuffer(&md5MsgState, &ctx->sysCtx));
    ERR(SSLFreeBuffer(&outputData, &ctx->sysCtx));

    return err;
}
