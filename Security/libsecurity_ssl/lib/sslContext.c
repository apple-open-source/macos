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
 * sslContext.c - SSLContext accessors
 */

#include "ssl.h"
#include "sslContext.h"
#include "sslMemory.h"
#include "sslDigests.h"
#include "sslDebug.h"
#include "sslCrypto.h"

#include "SecureTransport.h"

#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFPreferences.h>

#include <Security/SecTrust.h>
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
#include <Security/oidsalg.h>
#include <Security/SecTrustSettingsPriv.h>
#endif

#include "sslKeychain.h"
#include "sslUtils.h"
#include "cipherSpecs.h"
#include "appleSession.h"
#include "SecureTransportPriv.h"
#include <string.h>
#include <pthread.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecInternal.h>
#include <Security/SecTrust.h>
#include <Security/oidsalg.h>
#include <Security/SecTrustSettingsPriv.h>
#include <AssertMacros.h>

static void sslFreeDnList(
	SSLContext *ctx)
{
    DNListElem      *dn, *nextDN;

    dn = ctx->acceptableDNList;
    while (dn)
    {
    	SSLFreeBuffer(&dn->derDN, ctx);
        nextDN = dn->next;
        sslFree(dn);
        dn = nextDN;
    }
    ctx->acceptableDNList = NULL;
}

#define min(a,b)	( ((a) < (b)) ? (a) : (b) )
#define max(a,b)	( ((a) > (b)) ? (a) : (b) )

/*
 * Minimum and maximum supported versions
 */
//#define MINIMUM_STREAM_VERSION  SSL_Version_2_0 /* Disabled */
#define MINIMUM_STREAM_VERSION  SSL_Version_3_0
#define MAXIMUM_STREAM_VERSION  TLS_Version_1_2
#define MINIMUM_DATAGRAM_VERSION  DTLS_Version_1_0

/* This should be changed when we start supporting DTLS_Version_1_x */
#define MAXIMUM_DATAGRAM_VERSION  DTLS_Version_1_0

#define SSL_ENABLE_ECDSA_SIGN_AUTH			0
#define SSL_ENABLE_RSA_FIXED_ECDH_AUTH		0
#define SSL_ENABLE_ECDSA_FIXED_ECDH_AUTH	0

#define DEFAULT_DTLS_TIMEOUT    1
#define DEFAULT_DTLS_MTU        1400
#define MIN_ALLOWED_DTLS_MTU    64      /* this ensure than there will be no integer
                                            underflow when calculating max write size */

static CFTypeID kSSLContextTypeID;
int kSplitDefaultValue;

static void _sslContextDestroy(CFTypeRef arg);
static Boolean _sslContextEqual(CFTypeRef a, CFTypeRef b);
static CFHashCode _sslContextHash(CFTypeRef arg);
static CFStringRef _sslContextDescribe(CFTypeRef arg);

static void _SSLContextReadDefault()
{
	CFTypeRef value = (CFTypeRef)CFPreferencesCopyValue(CFSTR("SSLWriteSplit"),
							CFSTR("com.apple.security"),
							kCFPreferencesAnyUser,
							kCFPreferencesAnyHost);
	if (value) {
		if (CFGetTypeID(value) == CFBooleanGetTypeID())
			kSplitDefaultValue = CFBooleanGetValue((CFBooleanRef)value) ? 1 : 0;
		else if (CFGetTypeID(value) == CFNumberGetTypeID()) {
			if (!CFNumberGetValue((CFNumberRef)value, kCFNumberIntType, &kSplitDefaultValue))
				kSplitDefaultValue = 0;
		}
		if (kSplitDefaultValue < 0 || kSplitDefaultValue > 2) {
			kSplitDefaultValue = 0;
		}
		CFRelease(value);
	}
	else {
		kSplitDefaultValue = 0;
	}
}

static void _SSLContextRegisterClass()
{
	static const CFRuntimeClass kSSLContextRegisterClass = {
		0,												/* version */
        "SSLContext",					     			/* class name */
		NULL,											/* init */
		NULL,											/* copy */
		_sslContextDestroy,     	                    /* dealloc */
		_sslContextEqual,							    /* equal */
		_sslContextHash,								/* hash */
		NULL,											/* copyFormattingDesc */
		_sslContextDescribe		                        /* copyDebugDesc */
	};

    kSSLContextTypeID = _CFRuntimeRegisterClass(&kSSLContextRegisterClass);
}

CFTypeID
SSLContextGetTypeID(void)
{
	static pthread_once_t sOnce = PTHREAD_ONCE_INIT;
	pthread_once(&sOnce, _SSLContextRegisterClass);
	return kSSLContextTypeID;
}


OSStatus
SSLNewContext				(Boolean 			isServer,
							 SSLContextRef 		*contextPtr)	/* RETURNED */
{
	if(contextPtr == NULL) {
		return paramErr;
	}

	*contextPtr = SSLCreateContext(kCFAllocatorDefault, isServer?kSSLServerSide:kSSLClientSide, kSSLStreamType);

	if (*contextPtr == NULL)
		return memFullErr;

	return noErr;
}

SSLContextRef SSLCreateContext(CFAllocatorRef alloc, SSLProtocolSide protocolSide, SSLConnectionType connectionType)
{
	OSStatus	serr = noErr;
	SSLContext *ctx = (SSLContext*) _CFRuntimeCreateInstance(alloc, SSLContextGetTypeID(), sizeof(SSLContext) - sizeof(CFRuntimeBase), NULL);

	if(ctx == NULL) {
		return NULL;
	}

	/* subsequent errors to errOut: */
    memset(((uint8_t*) ctx) + sizeof(CFRuntimeBase), 0, sizeof(SSLContext) - sizeof(CFRuntimeBase));

    ctx->state = SSL_HdskStateUninit;
    ctx->timeout_duration = DEFAULT_DTLS_TIMEOUT;
    ctx->clientCertState = kSSLClientCertNone;

    ctx->minProtocolVersion = MINIMUM_STREAM_VERSION;
    ctx->maxProtocolVersion = MAXIMUM_STREAM_VERSION;

    ctx->isDTLS = false;
    ctx->dtlsCookie.data = NULL;
    ctx->dtlsCookie.length = 0;
    ctx->hdskMessageSeq = 0;
    ctx->hdskMessageSeqNext = 0;
    ctx->mtu = DEFAULT_DTLS_MTU;

	ctx->negProtocolVersion = SSL_Version_Undetermined;


    ctx->protocolSide = protocolSide;
	/* Default value so we can send and receive hello msgs */
	ctx->sslTslCalls = &Ssl3Callouts;

    /* Initialize the cipher state to NULL_WITH_NULL_NULL */

    ctx->selectedCipher        = TLS_NULL_WITH_NULL_NULL;
    InitCipherSpec(ctx);
    ctx->writeCipher.macRef    = ctx->selectedCipherSpec.macAlgorithm;
    ctx->readCipher.macRef     = ctx->selectedCipherSpec.macAlgorithm;
    ctx->readCipher.symCipher  = ctx->selectedCipherSpec.cipher;
    ctx->writeCipher.symCipher = ctx->selectedCipherSpec.cipher;

	/* these two are invariant */
    ctx->writeCipher.encrypting = 1;
    ctx->writePending.encrypting = 1;

    /* this gets init'd on first call to SSLHandshake() */
    ctx->validCipherSuites = NULL;
    ctx->numValidCipherSuites = 0;
#if ENABLE_SSLV2
    ctx->numValidNonSSLv2Suites = 0;
#endif

	ctx->peerDomainName = NULL;
	ctx->peerDomainNameLen = 0;

#ifdef USE_CDSA_CRYPTO
	/* attach to CSP, CL, TP */
	serr = attachToAll(ctx);
	if(serr) {
		goto errOut;
	}
#endif /* USE_CDSA_CRYPTO */

	/* Initial cert verify state: verify with default system roots */
	ctx->enableCertVerify = true;

	/* Default for RSA blinding is ENABLED */
	ctx->rsaBlindingEnable = true;

	/* Default for sending one-byte app data record is DISABLED */
	ctx->oneByteRecordEnable = false;

	/* Consult global system preference for default behavior:
	 * 0 = disabled, 1 = split every write, 2 = split second and subsequent writes
	 * (caller can override by setting kSSLSessionOptionSendOneByteRecord)
	 */
	static pthread_once_t sReadDefault = PTHREAD_ONCE_INIT;
	pthread_once(&sReadDefault, _SSLContextReadDefault);
	if (kSplitDefaultValue > 0)
		ctx->oneByteRecordEnable = true;

	/* default for anonymous ciphers is DISABLED */
	ctx->anonCipherEnable = false;

    ctx->breakOnServerAuth = false;
    ctx->breakOnCertRequest = false;
    ctx->breakOnClientAuth = false;
    ctx->signalServerAuth = false;
    ctx->signalCertRequest = false;
    ctx->signalClientAuth = false;

	/*
	 * Initial/default set of ECDH curves
	 */
	ctx->ecdhNumCurves = SSL_ECDSA_NUM_CURVES;
	ctx->ecdhCurves[0] = SSL_Curve_secp256r1;
	ctx->ecdhCurves[1] = SSL_Curve_secp384r1;
	ctx->ecdhCurves[2] = SSL_Curve_secp521r1;

	ctx->ecdhPeerCurve = SSL_Curve_None;		/* until we negotiate one */
	ctx->negAuthType = SSLClientAuthNone;		/* ditto */

    ctx->recordWriteQueue = NULL;
    ctx->messageWriteQueue = NULL;

    if(connectionType==kSSLDatagramType) {
	ctx->minProtocolVersion = MINIMUM_DATAGRAM_VERSION;
	ctx->maxProtocolVersion = MAXIMUM_DATAGRAM_VERSION;
        ctx->isDTLS = true;
	}

    ctx->secure_renegotiation = false;

#ifdef USE_CDSA_CRYPTO
errOut:
#endif /* USE_CDSA_CRYPTO */
	if (serr != noErr) {
		CFRelease(ctx);
		ctx = NULL;
    }
	return ctx;
}

OSStatus
SSLNewDatagramContext       (Boolean 			isServer,
							 SSLContextRef 		*contextPtr)	/* RETURNED */
{
	if (contextPtr == NULL)
		return paramErr;
	*contextPtr = SSLCreateContext(kCFAllocatorDefault, isServer?kSSLServerSide:kSSLClientSide, kSSLDatagramType);
	if (*contextPtr == NULL)
		return memFullErr;
    return noErr;
}

/*
 * Dispose of an SSLContext. (private)
 * This function is invoked after our dispatch queue is safely released,
 * or directly from SSLDisposeContext if there is no dispatch queue.
 */
OSStatus
SSLDisposeContext				(SSLContextRef context)
{
    if(context == NULL) {
        return paramErr;
    }
	CFRelease(context);
	return noErr;
}

CFStringRef _sslContextDescribe(CFTypeRef arg)
{
    SSLContext* ctx = (SSLContext*) arg;

    if (ctx == NULL) {
        return NULL;
    } else {
		CFStringRef result = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("<SSLContext(%p) { ... }>"), ctx);
		return result;
    }
}

Boolean _sslContextEqual(CFTypeRef a, CFTypeRef b)
{
	return a == b;
}

CFHashCode _sslContextHash(CFTypeRef arg)
{
	return (CFHashCode) arg;
}

void _sslContextDestroy(CFTypeRef arg)
{
	SSLContext* ctx = (SSLContext*) arg;
	WaitingRecord   *waitRecord, *next;

#if USE_SSLCERTIFICATE
	sslDeleteCertificateChain(ctx->localCert, ctx);
    sslDeleteCertificateChain(ctx->encryptCert, ctx);
    sslDeleteCertificateChain(ctx->peerCert, ctx);
    ctx->localCert = ctx->encryptCert = ctx->peerCert = NULL;
#else
	CFReleaseNull(ctx->localCert);
	CFReleaseNull(ctx->encryptCert);
	CFReleaseNull(ctx->peerCert);
	CFReleaseNull(ctx->trustedCerts);
#endif

    /* Free the last handshake message flight */
    SSLResetFlight(ctx);

    SSLFreeBuffer(&ctx->partialReadBuffer, ctx);
	if(ctx->peerSecTrust) {
		CFRelease(ctx->peerSecTrust);
		ctx->peerSecTrust = NULL;
	}
    waitRecord = ctx->recordWriteQueue;
    while (waitRecord)
    {   next = waitRecord->next;
		sslFree(waitRecord);
        waitRecord = next;
    }
    SSLFreeBuffer(&ctx->sessionTicket, ctx);

	#if APPLE_DH
    SSLFreeBuffer(&ctx->dhParamsEncoded, ctx);
#ifdef USE_CDSA_CRYPTO
	sslFreeKey(ctx->cspHand, &ctx->dhPrivate, NULL);
#else
    if (ctx->secDHContext)
        SecDHDestroy(ctx->secDHContext);
#endif /* !USE_CDSA_CRYPTO */
    SSLFreeBuffer(&ctx->dhPeerPublic, ctx);
    SSLFreeBuffer(&ctx->dhExchangePublic, ctx);
    #endif	/* APPLE_DH */

    SSLFreeBuffer(&ctx->ecdhPeerPublic, ctx);
    SSLFreeBuffer(&ctx->ecdhExchangePublic, ctx);
#if USE_CDSA_CRYPTO
	if(ctx->ecdhPrivCspHand == ctx->cspHand) {
		sslFreeKey(ctx->ecdhPrivCspHand, &ctx->ecdhPrivate, NULL);
	}
	/* else we got this key from a SecKeyRef, no free needed */
#endif

	CloseHash(&SSLHashSHA1, &ctx->shaState, ctx);
	CloseHash(&SSLHashMD5,  &ctx->md5State, ctx);
	CloseHash(&SSLHashSHA256,  &ctx->sha256State, ctx);
	CloseHash(&SSLHashSHA384,  &ctx->sha512State, ctx);

    SSLFreeBuffer(&ctx->sessionID, ctx);
    SSLFreeBuffer(&ctx->peerID, ctx);
    SSLFreeBuffer(&ctx->resumableSession, ctx);
    SSLFreeBuffer(&ctx->preMasterSecret, ctx);
    SSLFreeBuffer(&ctx->partialReadBuffer, ctx);
    SSLFreeBuffer(&ctx->fragmentedMessageCache, ctx);
    SSLFreeBuffer(&ctx->receivedDataBuffer, ctx);

	if(ctx->peerDomainName) {
		sslFree(ctx->peerDomainName);
		ctx->peerDomainName = NULL;
		ctx->peerDomainNameLen = 0;
	}
    SSLDisposeCipherSuite(&ctx->readCipher, ctx);
    SSLDisposeCipherSuite(&ctx->writeCipher, ctx);
    SSLDisposeCipherSuite(&ctx->readPending, ctx);
    SSLDisposeCipherSuite(&ctx->writePending, ctx);

	sslFree(ctx->validCipherSuites);
	ctx->validCipherSuites = NULL;
	ctx->numValidCipherSuites = 0;

#if USE_CDSA_CRYPTO
	/*
	 * NOTE: currently, all public keys come from the CL via CSSM_CL_CertGetKeyInfo.
	 * We really don't know what CSP the CL used to generate a public key (in fact,
	 * it uses the raw CSP only to get LogicalKeySizeInBits, but we can't know
	 * that). Thus using e.g. signingKeyCsp (or any other CSP) to free
	 * signingPubKey is not tecnically accurate. However, our public keys
	 * are all raw keys, and all Apple CSPs dispose of raw keys in the same
	 * way.
	 */
	sslFreeKey(ctx->cspHand, &ctx->signingPubKey, NULL);
	sslFreeKey(ctx->cspHand, &ctx->encryptPubKey, NULL);
	sslFreeKey(ctx->peerPubKeyCsp, &ctx->peerPubKey, NULL);

	if(ctx->signingPrivKeyRef) {
		CFRelease(ctx->signingPrivKeyRef);
	}
	if(ctx->encryptPrivKeyRef) {
		CFRelease(ctx->encryptPrivKeyRef);
	}
	if(ctx->trustedCerts) {
		CFRelease(ctx->trustedCerts);
	}
	detachFromAll(ctx);
#else
	sslFreePubKey(&ctx->signingPubKey);
	sslFreePubKey(&ctx->encryptPubKey);
	sslFreePubKey(&ctx->peerPubKey);
	sslFreePrivKey(&ctx->signingPrivKeyRef);
	sslFreePrivKey(&ctx->encryptPrivKeyRef);
#endif /* USE_CDSA_CRYPTO */
    CFReleaseSafe(ctx->acceptableCAs);
    CFReleaseSafe(ctx->trustedLeafCerts);
	CFReleaseSafe(ctx->localCertArray);
	CFReleaseSafe(ctx->encryptCertArray);
    CFReleaseSafe(ctx->encryptCertArray);
	if(ctx->clientAuthTypes) {
		sslFree(ctx->clientAuthTypes);
	}
    if(ctx->serverSigAlgs != NULL) {
        sslFree(ctx->serverSigAlgs);
    }
    if(ctx->clientSigAlgs != NULL) {
        sslFree(ctx->clientSigAlgs);
    }
	sslFreeDnList(ctx);

    SSLFreeBuffer(&ctx->ownVerifyData, ctx);
    SSLFreeBuffer(&ctx->peerVerifyData, ctx);

    memset(((uint8_t*) ctx) + sizeof(CFRuntimeBase), 0, sizeof(SSLContext) - sizeof(CFRuntimeBase));

	sslCleanupSession();
}

/*
 * Determine the state of an SSL session.
 */
OSStatus
SSLGetSessionState			(SSLContextRef		context,
							 SSLSessionState	*state)		/* RETURNED */
{
	SSLSessionState rtnState = kSSLIdle;

	if(context == NULL) {
		return paramErr;
	}
	*state = rtnState;
	switch(context->state) {
		case SSL_HdskStateUninit:
		case SSL_HdskStateServerUninit:
		case SSL_HdskStateClientUninit:
			rtnState = kSSLIdle;
			break;
		case SSL_HdskStateGracefulClose:
			rtnState = kSSLClosed;
			break;
		case SSL_HdskStateErrorClose:
		case SSL_HdskStateNoNotifyClose:
			rtnState = kSSLAborted;
			break;
		case SSL_HdskStateServerReady:
		case SSL_HdskStateClientReady:
			rtnState = kSSLConnected;
			break;
		default:
			assert((context->state >= SSL_HdskStateServerHello) &&
			        (context->state <= SSL2_HdskStateServerFinished));
			rtnState = kSSLHandshake;
			break;

	}
	*state = rtnState;
	return noErr;
}

/*
 * Set options for an SSL session.
 */
OSStatus
SSLSetSessionOption			(SSLContextRef		context,
							 SSLSessionOption	option,
							 Boolean			value)
{
	if(context == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(context)) {
		/* can't do this with an active session */
		return badReqErr;
	}
    switch(option) {
        case kSSLSessionOptionBreakOnServerAuth:
            context->breakOnServerAuth = value;
            context->enableCertVerify = !value;
            break;
        case kSSLSessionOptionBreakOnCertRequested:
            context->breakOnCertRequest = value;
            break;
        case kSSLSessionOptionBreakOnClientAuth:
            context->breakOnClientAuth = value;
            context->enableCertVerify = !value;
            break;
		case kSSLSessionOptionSendOneByteRecord:
			context->oneByteRecordEnable = value;
			break;
        default:
            return paramErr;
    }

    return noErr;
}

/*
 * Determine current value for the specified option in an SSL session.
 */
OSStatus
SSLGetSessionOption			(SSLContextRef		context,
							 SSLSessionOption	option,
							 Boolean			*value)
{
	if(context == NULL || value == NULL) {
		return paramErr;
	}
    switch(option) {
        case kSSLSessionOptionBreakOnServerAuth:
            *value = context->breakOnServerAuth;
            break;
        case kSSLSessionOptionBreakOnCertRequested:
            *value = context->breakOnCertRequest;
            break;
        case kSSLSessionOptionBreakOnClientAuth:
            *value = context->breakOnClientAuth;
            break;
		case kSSLSessionOptionSendOneByteRecord:
			*value = context->oneByteRecordEnable;
			break;
        default:
            return paramErr;
    }

    return noErr;
}

OSStatus
SSLSetIOFuncs				(SSLContextRef		ctx,
							 SSLReadFunc 		readFunc,
							 SSLWriteFunc		writeFunc)
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}

	ctx->ioCtx.read = readFunc;
	ctx->ioCtx.write = writeFunc;
	return noErr;
}

OSStatus
SSLSetConnection			(SSLContextRef		ctx,
							 SSLConnectionRef	connection)
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}

	ctx->ioCtx.ioRef = connection;
    return noErr;
}

OSStatus
SSLGetConnection			(SSLContextRef		ctx,
							 SSLConnectionRef	*connection)
{
	if((ctx == NULL) || (connection == NULL)) {
		return paramErr;
	}
	*connection = ctx->ioCtx.ioRef;
	return noErr;
}

OSStatus
SSLSetPeerDomainName		(SSLContextRef		ctx,
							 const char			*peerName,
							 size_t				peerNameLen)
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}

	/* free possible existing name */
	if(ctx->peerDomainName) {
		sslFree(ctx->peerDomainName);
	}

	/* copy in */
	ctx->peerDomainName = (char *)sslMalloc(peerNameLen);
	if(ctx->peerDomainName == NULL) {
		return memFullErr;
	}
	memmove(ctx->peerDomainName, peerName, peerNameLen);
	ctx->peerDomainNameLen = peerNameLen;
	return noErr;
}

/*
 * Determine the buffer size needed for SSLGetPeerDomainName().
 */
OSStatus
SSLGetPeerDomainNameLength	(SSLContextRef		ctx,
							 size_t				*peerNameLen)	// RETURNED
{
	if(ctx == NULL) {
		return paramErr;
	}
	*peerNameLen = ctx->peerDomainNameLen;
	return noErr;
}

OSStatus
SSLGetPeerDomainName		(SSLContextRef		ctx,
							 char				*peerName,		// returned here
							 size_t				*peerNameLen)	// IN/OUT
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(*peerNameLen < ctx->peerDomainNameLen) {
		return errSSLBufferOverflow;
	}
	memmove(peerName, ctx->peerDomainName, ctx->peerDomainNameLen);
	*peerNameLen = ctx->peerDomainNameLen;
	return noErr;
}

OSStatus
SSLSetDatagramHelloCookie   (SSLContextRef	ctx,
                             const void         *cookie,
                             size_t             cookieLen)
{
    OSStatus err;

    if(ctx == NULL) {
		return paramErr;
	}

    if(!ctx->isDTLS) return paramErr;

	if((ctx == NULL) || (cookieLen>32)) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}

	/* free possible existing cookie */
	if(ctx->dtlsCookie.data) {
        SSLFreeBuffer(&ctx->dtlsCookie, ctx);
	}

	/* copy in */
    if((err=SSLAllocBuffer(&ctx->dtlsCookie, cookieLen, ctx))!=noErr)
       return err;

	memmove(ctx->dtlsCookie.data, cookie, cookieLen);
    return noErr;
}

OSStatus
SSLSetMaxDatagramRecordSize (SSLContextRef		ctx,
                             size_t             maxSize)
{

    if(ctx == NULL) return paramErr;
    if(!ctx->isDTLS) return paramErr;
    if(maxSize < MIN_ALLOWED_DTLS_MTU) return paramErr;

    ctx->mtu = maxSize;

    return noErr;
}

OSStatus
SSLGetMaxDatagramRecordSize (SSLContextRef		ctx,
                             size_t             *maxSize)
{
    if(ctx == NULL) return paramErr;
    if(!ctx->isDTLS) return paramErr;

    *maxSize = ctx->mtu;

    return noErr;
}

/*

 Keys to to math below:

 A DTLS record looks like this: | header (13 bytes) | fragment |

 For Null cipher, fragment is clear text as follows:
 | Contents | Mac |

 For block cipher, fragment size must be a multiple of the cipher block size, and is the
 encryption of the following plaintext :
 | IV (1 block) | content | MAC | padding (0 to 255 bytes) | Padlen (1 byte) |

 The maximum content length in that case is achieved for 0 padding bytes.

*/

OSStatus
SSLGetDatagramWriteSize		(SSLContextRef ctx,
							 size_t *bufSize)
{
    if(ctx == NULL) return paramErr;
    if(!ctx->isDTLS) return paramErr;
    if(bufSize == NULL) return paramErr;

    size_t max_fragment_size = ctx->mtu-13; /* 13 = dtls record header */

    UInt16 blockSize = ctx->writeCipher.symCipher->blockSize;

    if (blockSize > 0) {
        /* max_fragment_size must be a multiple of blocksize */
        max_fragment_size = max_fragment_size & ~(blockSize-1);
        max_fragment_size -= blockSize; /* 1 block for IV */
        max_fragment_size -= 1; /* 1 byte for pad length */
    }

    /* less the mac size */
    max_fragment_size -= ctx->writeCipher.macRef->hash->digestSize;

    /* Thats just a sanity check */
    assert(max_fragment_size<ctx->mtu);

    *bufSize = max_fragment_size;

    return noErr;
}

static SSLProtocolVersion SSLProtocolToProtocolVersion(SSLProtocol protocol) {
    switch (protocol) {
        case kSSLProtocol2:             return SSL_Version_2_0;
        case kSSLProtocol3:             return SSL_Version_3_0;
        case kTLSProtocol1:             return TLS_Version_1_0;
        case kTLSProtocol11:            return TLS_Version_1_1;
        case kTLSProtocol12:            return TLS_Version_1_2;
        case kDTLSProtocol1:            return DTLS_Version_1_0;
        default:                        return SSL_Version_Undetermined;
    }
}

/* concert between private SSLProtocolVersion and public SSLProtocol */
static SSLProtocol SSLProtocolVersionToProtocol(SSLProtocolVersion version)
{
	switch(version) {
		case SSL_Version_2_0:           return kSSLProtocol2;
		case SSL_Version_3_0:           return kSSLProtocol3;
		case TLS_Version_1_0:           return kTLSProtocol1;
		case TLS_Version_1_1:           return kTLSProtocol11;
		case TLS_Version_1_2:           return kTLSProtocol12;
		case DTLS_Version_1_0:          return kDTLSProtocol1;
		default:
			sslErrorLog("SSLProtocolVersionToProtocol: bad prot (%04x)\n",
                        version);
            /* DROPTHROUGH */
		case SSL_Version_Undetermined:  return kSSLProtocolUnknown;
	}
}

OSStatus
SSLSetProtocolVersionMin  (SSLContextRef      ctx,
                           SSLProtocol        minVersion)
{
    if(ctx == NULL) return paramErr;

    SSLProtocolVersion version = SSLProtocolToProtocolVersion(minVersion);
    if (ctx->isDTLS) {
        if (version > MINIMUM_DATAGRAM_VERSION ||
            version < MAXIMUM_DATAGRAM_VERSION)
            return errSSLIllegalParam;
        if (version < ctx->maxProtocolVersion)
            ctx->maxProtocolVersion = version;
    } else {
        if (version < MINIMUM_STREAM_VERSION || version > MAXIMUM_STREAM_VERSION)
            return errSSLIllegalParam;
        if (version > ctx->maxProtocolVersion)
            ctx->maxProtocolVersion = version;
    }
    ctx->minProtocolVersion = version;

    return noErr;
}

OSStatus
SSLGetProtocolVersionMin  (SSLContextRef      ctx,
                           SSLProtocol        *minVersion)
{
    if(ctx == NULL) return paramErr;

    *minVersion = SSLProtocolVersionToProtocol(ctx->minProtocolVersion);
    return noErr;
}

OSStatus
SSLSetProtocolVersionMax  (SSLContextRef      ctx,
                           SSLProtocol        maxVersion)
{
    if(ctx == NULL) return paramErr;

    SSLProtocolVersion version = SSLProtocolToProtocolVersion(maxVersion);
    if (ctx->isDTLS) {
        if (version > MINIMUM_DATAGRAM_VERSION ||
            version < MAXIMUM_DATAGRAM_VERSION)
            return errSSLIllegalParam;
        if (version > ctx->minProtocolVersion)
            ctx->minProtocolVersion = version;
    } else {
        if (version < MINIMUM_STREAM_VERSION || version > MAXIMUM_STREAM_VERSION)
            return errSSLIllegalParam;
        if (version < ctx->minProtocolVersion)
            ctx->minProtocolVersion = version;
    }
    ctx->maxProtocolVersion = version;

    return noErr;
}

OSStatus
SSLGetProtocolVersionMax  (SSLContextRef      ctx,
                           SSLProtocol        *maxVersion)
{
    if(ctx == NULL) return paramErr;

    *maxVersion = SSLProtocolVersionToProtocol(ctx->maxProtocolVersion);
    return noErr;
}


OSStatus
SSLSetProtocolVersionEnabled(SSLContextRef     ctx,
							 SSLProtocol		protocol,
							 Boolean			enable)
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx) || ctx->isDTLS) {
		/* Can't do this with an active session, nor with a DTLS session */
		return badReqErr;
	}
    if (protocol == kSSLProtocolAll) {
        if (enable) {
            ctx->minProtocolVersion = MINIMUM_STREAM_VERSION;
            ctx->maxProtocolVersion = MAXIMUM_STREAM_VERSION;
        } else {
            ctx->minProtocolVersion = SSL_Version_Undetermined;
            ctx->maxProtocolVersion = SSL_Version_Undetermined;
        }
	} else {
		SSLProtocolVersion version = SSLProtocolToProtocolVersion(protocol);
        if (enable) {
			if (version < MINIMUM_STREAM_VERSION || version > MAXIMUM_STREAM_VERSION) {
				return paramErr;
			}
            if (version > ctx->maxProtocolVersion) {
                ctx->maxProtocolVersion = version;
                if (ctx->minProtocolVersion == SSL_Version_Undetermined)
                    ctx->minProtocolVersion = version;
            }
            if (version < ctx->minProtocolVersion) {
                ctx->minProtocolVersion = version;
            }
        } else {
			if (version < SSL_Version_2_0 || version > MAXIMUM_STREAM_VERSION) {
				return paramErr;
			}
			/* Disabling a protocol version now resets the minimum acceptable
			 * version to the next higher version. This means it's no longer
			 * possible to enable a discontiguous set of protocol versions.
			 */
			SSLProtocolVersion nextVersion;
			switch (version) {
				case SSL_Version_2_0:
					nextVersion = SSL_Version_3_0;
					break;
				case SSL_Version_3_0:
					nextVersion = TLS_Version_1_0;
					break;
				case TLS_Version_1_0:
					nextVersion = TLS_Version_1_1;
					break;
				case TLS_Version_1_1:
					nextVersion = TLS_Version_1_2;
					break;
				case TLS_Version_1_2:
				default:
					nextVersion = SSL_Version_Undetermined;
					break;
			}
			ctx->minProtocolVersion = max(ctx->minProtocolVersion, nextVersion);
			if (ctx->minProtocolVersion > ctx->maxProtocolVersion) {
				ctx->minProtocolVersion = SSL_Version_Undetermined;
				ctx->maxProtocolVersion = SSL_Version_Undetermined;
			}
        }
    }

	return noErr;
}

OSStatus
SSLGetProtocolVersionEnabled(SSLContextRef 		ctx,
							 SSLProtocol		protocol,
							 Boolean			*enable)		/* RETURNED */
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(ctx->isDTLS) {
		/* Can't do this with a DTLS session */
		return badReqErr;
	}
	switch(protocol) {
		case kSSLProtocol2:
		case kSSLProtocol3:
		case kTLSProtocol1:
        case kTLSProtocol11:
        case kTLSProtocol12:
        {
            SSLProtocolVersion version = SSLProtocolToProtocolVersion(protocol);
			*enable = (ctx->minProtocolVersion <= version
                       && ctx->maxProtocolVersion >= version);
			break;
        }
		case kSSLProtocolAll:
            *enable = (ctx->minProtocolVersion <= MINIMUM_STREAM_VERSION
                       && ctx->maxProtocolVersion >= MAXIMUM_STREAM_VERSION);
			break;
		default:
			return paramErr;
	}
	return noErr;
}

/* deprecated */
OSStatus
SSLSetProtocolVersion		(SSLContextRef 		ctx,
							 SSLProtocol		version)
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx) || ctx->isDTLS) {
		/* Can't do this with an active session, nor with a DTLS session */
		return badReqErr;
	}

	switch(version) {
		case kSSLProtocol3:
			/* this tells us to do our best, up to 3.0 */
            ctx->minProtocolVersion = MINIMUM_STREAM_VERSION;
            ctx->maxProtocolVersion = SSL_Version_3_0;
			break;
		case kSSLProtocol3Only:
            ctx->minProtocolVersion = SSL_Version_3_0;
            ctx->maxProtocolVersion = SSL_Version_3_0;
			break;
		case kTLSProtocol1:
			/* this tells us to do our best, up to TLS, but allows 3.0 */
            ctx->minProtocolVersion = MINIMUM_STREAM_VERSION;
            ctx->maxProtocolVersion = TLS_Version_1_0;
            break;
        case kTLSProtocol1Only:
            ctx->minProtocolVersion = TLS_Version_1_0;
            ctx->maxProtocolVersion = TLS_Version_1_0;
			break;
        case kTLSProtocol11:
			/* This tells us to do our best, up to TLS 1.1, currently also
               allows 3.0 or TLS 1.0 */
            ctx->minProtocolVersion = MINIMUM_STREAM_VERSION;
            ctx->maxProtocolVersion = TLS_Version_1_1;
			break;
        case kTLSProtocol12:
        case kSSLProtocolAll:
		case kSSLProtocolUnknown:
			/* This tells us to do our best, up to TLS 1.2, currently also
               allows 3.0 or TLS 1.0 or TLS 1.1 */
            ctx->minProtocolVersion = MINIMUM_STREAM_VERSION;
            ctx->maxProtocolVersion = MAXIMUM_STREAM_VERSION;
			break;
		default:
			return paramErr;
	}

    return noErr;
}

/* deprecated */
OSStatus
SSLGetProtocolVersion		(SSLContextRef		ctx,
							 SSLProtocol		*protocol)		/* RETURNED */
{
	if(ctx == NULL) {
		return paramErr;
	}
	/* translate array of booleans to public value; not all combinations
	 * are legal (i.e., meaningful) for this call */
    if (ctx->maxProtocolVersion == MAXIMUM_STREAM_VERSION) {
        if(ctx->minProtocolVersion == MINIMUM_STREAM_VERSION) {
            /* traditional 'all enabled' */
            *protocol = kSSLProtocolAll;
            return noErr;
		}
	} else if (ctx->maxProtocolVersion == TLS_Version_1_1) {
        if(ctx->minProtocolVersion == MINIMUM_STREAM_VERSION) {
            /* traditional 'all enabled' */
            *protocol = kTLSProtocol11;
            return noErr;
        }
	} else if (ctx->maxProtocolVersion == TLS_Version_1_0) {
        if(ctx->minProtocolVersion == MINIMUM_STREAM_VERSION) {
            /* TLS1.1 and below enabled */
            *protocol = kTLSProtocol1;
            return noErr;
        } else if(ctx->minProtocolVersion == TLS_Version_1_0) {
        	*protocol = kTLSProtocol1Only;
		}
	} else if(ctx->maxProtocolVersion == SSL_Version_3_0) {
        if(ctx->minProtocolVersion == MINIMUM_STREAM_VERSION) {
            /* Could also return kSSLProtocol3Only since
               MINIMUM_STREAM_VERSION == SSL_Version_3_0. */
            *protocol = kSSLProtocol3;
			return noErr;
		}
	}

    return paramErr;
}

OSStatus
SSLGetNegotiatedProtocolVersion		(SSLContextRef		ctx,
									 SSLProtocol		*protocol) /* RETURNED */
{
	if(ctx == NULL) {
		return paramErr;
	}
	*protocol = SSLProtocolVersionToProtocol(ctx->negProtocolVersion);
	return noErr;
}

OSStatus
SSLSetEnableCertVerify		(SSLContextRef		ctx,
							 Boolean			enableVerify)
{
	if(ctx == NULL) {
		return paramErr;
	}
	sslCertDebug("SSLSetEnableCertVerify %s",
		enableVerify ? "true" : "false");
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	ctx->enableCertVerify = enableVerify;
	return noErr;
}

OSStatus
SSLGetEnableCertVerify		(SSLContextRef		ctx,
							Boolean				*enableVerify)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*enableVerify = ctx->enableCertVerify;
	return noErr;
}

OSStatus
SSLSetAllowsExpiredCerts(SSLContextRef		ctx,
						 Boolean			allowExpired)
{
	if(ctx == NULL) {
		return paramErr;
	}
	sslCertDebug("SSLSetAllowsExpiredCerts %s",
		allowExpired ? "true" : "false");
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	ctx->allowExpiredCerts = allowExpired;
	return noErr;
}

OSStatus
SSLGetAllowsExpiredCerts	(SSLContextRef		ctx,
							 Boolean			*allowExpired)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*allowExpired = ctx->allowExpiredCerts;
	return noErr;
}

OSStatus
SSLSetAllowsExpiredRoots(SSLContextRef		ctx,
						 Boolean			allowExpired)
{
	if(ctx == NULL) {
		return paramErr;
	}
	sslCertDebug("SSLSetAllowsExpiredRoots %s",
		allowExpired ? "true" : "false");
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	ctx->allowExpiredRoots = allowExpired;
	return noErr;
}

OSStatus
SSLGetAllowsExpiredRoots	(SSLContextRef		ctx,
							 Boolean			*allowExpired)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*allowExpired = ctx->allowExpiredRoots;
	return noErr;
}

OSStatus SSLSetAllowsAnyRoot(
	SSLContextRef	ctx,
	Boolean			anyRoot)
{
	if(ctx == NULL) {
		return paramErr;
	}
	sslCertDebug("SSLSetAllowsAnyRoot %s",	anyRoot ? "true" : "false");
	ctx->allowAnyRoot = anyRoot;
	return noErr;
}

OSStatus
SSLGetAllowsAnyRoot(
	SSLContextRef	ctx,
	Boolean			*anyRoot)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*anyRoot = ctx->allowAnyRoot;
	return noErr;
}

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
/* obtain the system roots sets for this app, policy SSL */
static OSStatus sslDefaultSystemRoots(
	SSLContextRef ctx,
	CFArrayRef *systemRoots)				// created and RETURNED

{
	return SecTrustSettingsCopyQualifiedCerts(&CSSMOID_APPLE_TP_SSL,
		NULL, true,	// application - us
		ctx->peerDomainName,
		ctx->peerDomainNameLen,
		(ctx->protocolSide == kSSLServerSide) ?
			/* server verifies, client encrypts */
			CSSM_KEYUSE_VERIFY : CSSM_KEYUSE_ENCRYPT,
		systemRoots);
}
#endif /* OS X only */

OSStatus
SSLSetTrustedRoots			(SSLContextRef 		ctx,
							 CFArrayRef 		trustedRoots,
							 Boolean 			replaceExisting)
{
#ifdef USE_CDSA_CRYPTO
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}

	if(replaceExisting) {
		/* trivial case - retain the new, throw out the old.  */
		if (trustedRoots)
            CFRetain(trustedRoots);
        CFReleaseSafe(ctx->trustedCerts);
		ctx->trustedCerts = trustedRoots;
		return noErr;
	}

	/* adding new trusted roots - to either our existing set, or the system set */
	CFArrayRef existingRoots = NULL;
	OSStatus ortn;
	if(ctx->trustedCerts != NULL) {
		/* we'll release these as we exit */
		existingRoots = ctx->trustedCerts;
	}
	else {
		/* get system set for this app, policy SSL */
		ortn = sslDefaultSystemRoots(ctx, &existingRoots);
		if(ortn) {
            CFReleaseSafe(existingRoots);
			return ortn;
		}
	}

	/* Create a new root array with caller's roots first */
	CFMutableArrayRef newRoots = CFArrayCreateMutableCopy(NULL, 0, trustedRoots);
	CFRange existRange = { 0, CFArrayGetCount(existingRoots) };
	CFArrayAppendArray(newRoots, existingRoots, existRange);
	CFRelease(existingRoots);
	ctx->trustedCerts = newRoots;
	return noErr;

#else
	if (sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	sslCertDebug("SSLSetTrustedRoot  numCerts %d  replaceExist %s",
		(int)CFArrayGetCount(trustedRoots), replaceExisting ? "true" : "false");

    if (replaceExisting) {
        ctx->trustedCertsOnly = true;
        CFReleaseNull(ctx->trustedCerts);
    }

    if (ctx->trustedCerts) {
        CFIndex count = CFArrayGetCount(trustedRoots);
        CFRange range = { 0, count };
        CFArrayAppendArray(ctx->trustedCerts, trustedRoots, range);
    } else {
        require(ctx->trustedCerts =
            CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, trustedRoots),
            errOut);
    }

    return noErr;

errOut:
    return memFullErr;
#endif /* !USE_CDSA_CRYPTO */
}

OSStatus
SSLCopyTrustedRoots			(SSLContextRef 		ctx,
							 CFArrayRef 		*trustedRoots)	/* RETURNED */
{
	if(ctx == NULL || trustedRoots == NULL) {
		return paramErr;
	}
	if(ctx->trustedCerts != NULL) {
		*trustedRoots = ctx->trustedCerts;
		CFRetain(ctx->trustedCerts);
		return noErr;
	}
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
	/* use default system roots */
    return sslDefaultSystemRoots(ctx, trustedRoots);
#else
    *trustedRoots = NULL;
    return noErr;
#endif
}

/* legacy version, caller must CFRelease each cert */
OSStatus
SSLGetTrustedRoots			(SSLContextRef 		ctx,
							 CFArrayRef 		*trustedRoots)	/* RETURNED */
{
	OSStatus ortn;

	if((ctx == NULL) || (trustedRoots == NULL)) {
		return paramErr;
	}

	ortn = SSLCopyTrustedRoots(ctx, trustedRoots);
	if(ortn) {
		return ortn;
	}
	/* apply the legacy bug */
	CFIndex numCerts = CFArrayGetCount(*trustedRoots);
	CFIndex dex;
	for(dex=0; dex<numCerts; dex++) {
		CFRetain(CFArrayGetValueAtIndex(*trustedRoots, dex));
	}
	return noErr;
}

OSStatus
SSLSetTrustedLeafCertificates	(SSLContextRef 		ctx,
								 CFArrayRef 		trustedCerts)
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}

	if(ctx->trustedLeafCerts) {
		CFRelease(ctx->trustedLeafCerts);
	}
	ctx->trustedLeafCerts = trustedCerts;
	CFRetain(trustedCerts);
	return noErr;
}

OSStatus
SSLCopyTrustedLeafCertificates	(SSLContextRef 		ctx,
								 CFArrayRef 		*trustedCerts)	/* RETURNED */
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(ctx->trustedLeafCerts != NULL) {
		*trustedCerts = ctx->trustedLeafCerts;
		CFRetain(ctx->trustedCerts);
		return noErr;
	}
	*trustedCerts = NULL;
	return noErr;
}

OSStatus
SSLSetClientSideAuthenticate 	(SSLContext			*ctx,
								 SSLAuthenticate	auth)
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	ctx->clientAuth = auth;
	switch(auth) {
		case kNeverAuthenticate:
			ctx->tryClientAuth = false;
			break;
		case kAlwaysAuthenticate:
		case kTryAuthenticate:
			ctx->tryClientAuth = true;
			break;
	}
	return noErr;
}

OSStatus
SSLGetClientSideAuthenticate 	(SSLContext			*ctx,
								 SSLAuthenticate	*auth)	/* RETURNED */
{
	if(ctx == NULL || auth == NULL) {
		return paramErr;
	}
	*auth = ctx->clientAuth;
	return noErr;
}

OSStatus
SSLGetClientCertificateState	(SSLContextRef				ctx,
								 SSLClientCertificateState	*clientState)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*clientState = ctx->clientCertState;
	return noErr;
}

OSStatus
SSLSetCertificate			(SSLContextRef		ctx,
							 CFArrayRef			certRefs)
{
	/*
	 * -- free localCerts if we have any
	 * -- Get raw cert data, convert to ctx->localCert
	 * -- get pub, priv keys from certRef[0]
	 * -- validate cert chain
	 */
	if(ctx == NULL) {
		return paramErr;
	}

	/* can't do this with an active session */
	if(sslIsSessionActive(ctx) &&
	   /* kSSLClientCertRequested implies client side */
	   (ctx->clientCertState != kSSLClientCertRequested))
	{
			return badReqErr;
	}
    CFReleaseNull(ctx->localCertArray);
	/* changing the client cert invalidates negotiated auth type */
	ctx->negAuthType = SSLClientAuthNone;
	if(certRefs == NULL) {
		return noErr; // we have cleared the cert, as requested
	}
	OSStatus ortn = parseIncomingCerts(ctx,
		certRefs,
		&ctx->localCert,
		&ctx->signingPubKey,
		&ctx->signingPrivKeyRef,
		&ctx->ourSignerAlg);
	if(ortn == noErr) {
		ctx->localCertArray = certRefs;
		CFRetain(certRefs);
		/* client cert was changed, must update auth type */
		ortn = SSLUpdateNegotiatedClientAuthType(ctx);
	}
	return ortn;
}

OSStatus
SSLSetEncryptionCertificate	(SSLContextRef		ctx,
							 CFArrayRef			certRefs)
{
	/*
	 * -- free encryptCert if we have any
	 * -- Get raw cert data, convert to ctx->encryptCert
	 * -- get pub, priv keys from certRef[0]
	 * -- validate cert chain
	 */
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
    CFReleaseNull(ctx->encryptCertArray);
	OSStatus ortn = parseIncomingCerts(ctx,
		certRefs,
		&ctx->encryptCert,
		&ctx->encryptPubKey,
		&ctx->encryptPrivKeyRef,
		NULL);			/* Signer alg */
	if(ortn == noErr) {
		ctx->encryptCertArray = certRefs;
		CFRetain(certRefs);
	}
	return ortn;
}

OSStatus SSLGetCertificate(SSLContextRef		ctx,
						   CFArrayRef			*certRefs)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*certRefs = ctx->localCertArray;
	return noErr;
}

OSStatus SSLGetEncryptionCertificate(SSLContextRef		ctx,
								     CFArrayRef			*certRefs)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*certRefs = ctx->encryptCertArray;
	return noErr;
}

OSStatus
SSLSetPeerID				(SSLContext 		*ctx,
							 const void 		*peerID,
							 size_t				peerIDLen)
{
	OSStatus serr;

	/* copy peerId to context->peerId */
	if((ctx == NULL) ||
	   (peerID == NULL) ||
	   (peerIDLen == 0)) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx) &&
        /* kSSLClientCertRequested implies client side */
        (ctx->clientCertState != kSSLClientCertRequested))
    {
		return badReqErr;
	}
	SSLFreeBuffer(&ctx->peerID, ctx);
	serr = SSLAllocBuffer(&ctx->peerID, peerIDLen, ctx);
	if(serr) {
		return serr;
	}
	memmove(ctx->peerID.data, peerID, peerIDLen);
	return noErr;
}

OSStatus
SSLGetPeerID				(SSLContextRef 		ctx,
							 const void 		**peerID,
							 size_t				*peerIDLen)
{
	*peerID = ctx->peerID.data;			// may be NULL
	*peerIDLen = ctx->peerID.length;
	return noErr;
}

OSStatus
SSLGetNegotiatedCipher		(SSLContextRef 		ctx,
							 SSLCipherSuite 	*cipherSuite)
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(!sslIsSessionActive(ctx)) {
		return badReqErr;
	}
	*cipherSuite = (SSLCipherSuite)ctx->selectedCipher;
	return noErr;
}

/*
 * Add an acceptable distinguished name (client authentication only).
 */
OSStatus
SSLAddDistinguishedName(
	SSLContextRef ctx,
	const void *derDN,
	size_t derDNLen)
{
    DNListElem      *dn;
    OSStatus        err;

	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		return badReqErr;
	}

	dn = (DNListElem *)sslMalloc(sizeof(DNListElem));
	if(dn == NULL) {
		return memFullErr;
	}
    if ((err = SSLAllocBuffer(&dn->derDN, derDNLen, ctx)) != 0)
        return err;
    memcpy(dn->derDN.data, derDN, derDNLen);
    dn->next = ctx->acceptableDNList;
    ctx->acceptableDNList = dn;
    return noErr;
}

/* single-cert version of SSLSetCertificateAuthorities() */
static OSStatus
sslAddCA(SSLContextRef		ctx,
		 SecCertificateRef	cert)
{
	OSStatus ortn = paramErr;
	CFDataRef subjectName;

    /* Get subject from certificate. */
    require(subjectName = SecCertificateCopySubjectSequence(cert), errOut);
	/* add to acceptableCAs as cert, creating array if necessary */
	if(ctx->acceptableCAs == NULL) {
		require(ctx->acceptableCAs = CFArrayCreateMutable(NULL, 0,
            &kCFTypeArrayCallBacks), errOut);
		if(ctx->acceptableCAs == NULL) {
			return memFullErr;
		}
	}
	CFArrayAppendValue(ctx->acceptableCAs, cert);

	/* then add this cert's subject name to acceptableDNList */
	ortn = SSLAddDistinguishedName(ctx, CFDataGetBytePtr(subjectName),
        CFDataGetLength(subjectName));
errOut:
    CFReleaseSafe(subjectName);
	return ortn;
}

/*
 * Add a SecCertificateRef, or a CFArray of them, to a server's list
 * of acceptable Certificate Authorities (CAs) to present to the client
 * when client authentication is performed.
 */
OSStatus
SSLSetCertificateAuthorities(SSLContextRef		ctx,
							 CFTypeRef			certificateOrArray,
							 Boolean 			replaceExisting)
{
	CFTypeID itemType;
	OSStatus ortn = noErr;

	if((ctx == NULL) || sslIsSessionActive(ctx) ||
	   (ctx->protocolSide != kSSLServerSide)) {
		return paramErr;
	}
	if(replaceExisting) {
		sslFreeDnList(ctx);
		if(ctx->acceptableCAs) {
			CFRelease(ctx->acceptableCAs);
			ctx->acceptableCAs = NULL;
		}
	}
	/* else appending */

	itemType = CFGetTypeID(certificateOrArray);
	if(itemType == SecCertificateGetTypeID()) {
		/* one cert */
		ortn = sslAddCA(ctx, (SecCertificateRef)certificateOrArray);
	}
	else if(itemType == CFArrayGetTypeID()) {
		CFArrayRef cfa = (CFArrayRef)certificateOrArray;
		CFIndex numCerts = CFArrayGetCount(cfa);
		CFIndex dex;

		/* array of certs */
		for(dex=0; dex<numCerts; dex++) {
			SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex(cfa, dex);
			if(CFGetTypeID(cert) != SecCertificateGetTypeID()) {
				return paramErr;
			}
			ortn = sslAddCA(ctx, cert);
			if(ortn) {
				break;
			}
		}
	}
	else {
		ortn = paramErr;
	}
	return ortn;
}


/*
 * Obtain the certificates specified in SSLSetCertificateAuthorities(),
 * if any. Returns a NULL array if SSLSetCertificateAuthorities() has not
 * been called.
 * Caller must CFRelease the returned array.
 */
OSStatus
SSLCopyCertificateAuthorities(SSLContextRef		ctx,
							  CFArrayRef		*certificates)	/* RETURNED */
{
	if((ctx == NULL) || (certificates == NULL)) {
		return paramErr;
	}
	if(ctx->acceptableCAs == NULL) {
		*certificates = NULL;
		return noErr;
	}
	*certificates = ctx->acceptableCAs;
	CFRetain(ctx->acceptableCAs);
	return noErr;
}


/*
 * Obtain the list of acceptable distinguished names as provided by
 * a server (if the SSLCotextRef is configured as a client), or as
 * specified by SSLSetCertificateAuthorities() (if the SSLContextRef
 * is configured as a server).
  */
OSStatus
SSLCopyDistinguishedNames	(SSLContextRef		ctx,
							 CFArrayRef			*names)
{
	CFMutableArrayRef outArray = NULL;
	DNListElem *dn;

	if((ctx == NULL) || (names == NULL)) {
		return paramErr;
	}
	if(ctx->acceptableDNList == NULL) {
		*names = NULL;
		return noErr;
	}
	outArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	dn = ctx->acceptableDNList;
	while (dn) {
		CFDataRef cfDn = CFDataCreate(NULL, dn->derDN.data, dn->derDN.length);
		CFArrayAppendValue(outArray, cfDn);
		CFRelease(cfDn);
		dn = dn->next;
	}
	*names = outArray;
	return noErr;
}


/*
 * Request peer certificates. Valid anytime, subsequent to
 * a handshake attempt.
 * Common code for SSLGetPeerCertificates() and SSLCopyPeerCertificates().
 */
static OSStatus
sslCopyPeerCertificates		(SSLContextRef 		ctx,
							 CFArrayRef			*certs,
							 Boolean			legacy)
{
	if(ctx == NULL) {
		return paramErr;
	}

#ifdef USE_SSLCERTIFICATE
	uint32 				numCerts;
	CFMutableArrayRef	ca;
	CFIndex				i;
	SecCertificateRef	cfd;
	OSStatus			ortn;
	CSSM_DATA			certData;
	SSLCertificate		*scert;

	*certs = NULL;

	/*
	 * Copy peerCert, a chain of SSLCertificates, to a CFArray of
	 * CFDataRefs, each of which is one DER-encoded cert.
	 */
	numCerts = SSLGetCertificateChainLength(ctx->peerCert);
	if(numCerts == 0) {
		return noErr;
	}
	ca = CFArrayCreateMutable(kCFAllocatorDefault,
		(CFIndex)numCerts, &kCFTypeArrayCallBacks);
	if(ca == NULL) {
		return memFullErr;
	}

	/*
	 * Caller gets leaf cert first, the opposite of the way we store them.
	 */
	scert = ctx->peerCert;
	for(i=0; (unsigned)i<numCerts; i++) {
		assert(scert != NULL);		/* else SSLGetCertificateChainLength
									 * broken */
		SSLBUF_TO_CSSM(&scert->derCert, &certData);
		ortn = SecCertificateCreateFromData(&certData,
			CSSM_CERT_X_509v3,
			CSSM_CERT_ENCODING_DER,
			&cfd);
		if(ortn) {
			CFRelease(ca);
			return ortn;
		}
		/* insert at head of array */
		CFArrayInsertValueAtIndex(ca, 0, cfd);
		if(!legacy) {
			/* skip for legacy SSLGetPeerCertificates() */
			CFRelease(cfd);
		}
		scert = scert->next;
	}
	*certs = ca;

#else
	if (!ctx->peerCert) {
		*certs = NULL;
		return badReqErr;
	}

    CFArrayRef ca = CFArrayCreateCopy(kCFAllocatorDefault, ctx->peerCert);
    *certs = ca;
    if (ca == NULL) {
        return memFullErr;
    }

	if (legacy) {
		CFIndex ix, count = CFArrayGetCount(ca);
		for (ix = 0; ix < count; ++ix) {
			CFRetain(CFArrayGetValueAtIndex(ca, ix));
		}
	}
#endif

	return noErr;
}

OSStatus
SSLCopyPeerCertificates		(SSLContextRef 		ctx,
							 CFArrayRef			*certs)
{
	return sslCopyPeerCertificates(ctx, certs, false);
}

OSStatus
SSLGetPeerCertificates		(SSLContextRef 		ctx,
							 CFArrayRef			*certs)
 {
	 return sslCopyPeerCertificates(ctx, certs, true);
 }


/*
 * Specify Diffie-Hellman parameters. Optional; if we are configured to allow
 * for D-H ciphers and a D-H cipher is negotiated, and this function has not
 * been called, a set of process-wide parameters will be calculated. However
 * that can take a long time (30 seconds).
 */
OSStatus SSLSetDiffieHellmanParams(
	SSLContextRef	ctx,
	const void 		*dhParams,
	size_t			dhParamsLen)
{
#if APPLE_DH
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		return badReqErr;
	}
	SSLFreeBuffer(&ctx->dhParamsEncoded, ctx);
#if !USE_CDSA_CRYPTO
    if (ctx->secDHContext)
        SecDHDestroy(ctx->secDHContext);
#endif

	OSStatus ortn;
	ortn = SSLCopyBufferFromData(dhParams, dhParamsLen,
		&ctx->dhParamsEncoded);

    return ortn;

#endif /* APPLE_DH */
}

/*
 * Return parameter block specified in SSLSetDiffieHellmanParams.
 * Returned data is not copied and belongs to the SSLContextRef.
 */
OSStatus SSLGetDiffieHellmanParams(
	SSLContextRef	ctx,
	const void 		**dhParams,
	size_t			*dhParamsLen)
{
#if APPLE_DH
	if(ctx == NULL) {
		return paramErr;
	}
	*dhParams = ctx->dhParamsEncoded.data;
	*dhParamsLen = ctx->dhParamsEncoded.length;
	return noErr;
#else
    return unimpErr;
#endif /* APPLE_DH */
}

OSStatus SSLSetRsaBlinding(
	SSLContextRef	ctx,
	Boolean			blinding)
{
	if(ctx == NULL) {
		return paramErr;
	}
	ctx->rsaBlindingEnable = blinding;
	return noErr;
}

OSStatus SSLGetRsaBlinding(
	SSLContextRef	ctx,
	Boolean			*blinding)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*blinding = ctx->rsaBlindingEnable;
	return noErr;
}

OSStatus
SSLCopyPeerTrust(
    SSLContextRef 		ctx,
    SecTrustRef        *trust)	/* RETURNED */
{
	OSStatus status = noErr;
	if (ctx == NULL || trust == NULL)
		return paramErr;

	/* Create a SecTrustRef if this was a resumed session and we
	   didn't have one yet. */
	if (!ctx->peerSecTrust && ctx->peerCert) {
		status = sslCreateSecTrust(ctx, ctx->peerCert, true,
			&ctx->peerSecTrust);
    }

	*trust = ctx->peerSecTrust;
    if (ctx->peerSecTrust)
        CFRetain(ctx->peerSecTrust);

	return status;
}

OSStatus SSLGetPeerSecTrust(
	SSLContextRef	ctx,
	SecTrustRef		*trust)	/* RETURNED */
{
    OSStatus status = noErr;
	if (ctx == NULL || trust == NULL)
		return paramErr;

	/* Create a SecTrustRef if this was a resumed session and we
	   didn't have one yet. */
	if (!ctx->peerSecTrust && ctx->peerCert) {
		status = sslCreateSecTrust(ctx, ctx->peerCert, true,
			&ctx->peerSecTrust);
    }

	*trust = ctx->peerSecTrust;
	return status;
}

OSStatus SSLInternalMasterSecret(
   SSLContextRef ctx,
   void *secret,        // mallocd by caller, SSL_MASTER_SECRET_SIZE
   size_t *secretSize)  // in/out
{
	if((ctx == NULL) || (secret == NULL) || (secretSize == NULL)) {
		return paramErr;
	}
	if(*secretSize < SSL_MASTER_SECRET_SIZE) {
		return paramErr;
	}
	memmove(secret, ctx->masterSecret, SSL_MASTER_SECRET_SIZE);
	*secretSize = SSL_MASTER_SECRET_SIZE;
	return noErr;
}

OSStatus SSLInternalServerRandom(
   SSLContextRef ctx,
   void *randBuf, 			// mallocd by caller, SSL_CLIENT_SRVR_RAND_SIZE
   size_t *randSize)	// in/out
{
	if((ctx == NULL) || (randBuf == NULL) || (randSize == NULL)) {
		return paramErr;
	}
	if(*randSize < SSL_CLIENT_SRVR_RAND_SIZE) {
		return paramErr;
	}
	memmove(randBuf, ctx->serverRandom, SSL_CLIENT_SRVR_RAND_SIZE);
	*randSize = SSL_CLIENT_SRVR_RAND_SIZE;
	return noErr;
}

OSStatus SSLInternalClientRandom(
   SSLContextRef ctx,
   void *randBuf,  		// mallocd by caller, SSL_CLIENT_SRVR_RAND_SIZE
   size_t *randSize)	// in/out
{
	if((ctx == NULL) || (randBuf == NULL) || (randSize == NULL)) {
		return paramErr;
	}
	if(*randSize < SSL_CLIENT_SRVR_RAND_SIZE) {
		return paramErr;
	}
	memmove(randBuf, ctx->clientRandom, SSL_CLIENT_SRVR_RAND_SIZE);
	*randSize = SSL_CLIENT_SRVR_RAND_SIZE;
	return noErr;
}

OSStatus SSLGetCipherSizes(
	SSLContextRef ctx,
	size_t *digestSize,
	size_t *symmetricKeySize,
	size_t *ivSize)
{
	const SSLCipherSpec *currCipher;

	if((ctx == NULL) || (digestSize == NULL) ||
	   (symmetricKeySize == NULL) || (ivSize == NULL)) {
		return paramErr;
	}
	currCipher = &ctx->selectedCipherSpec;
	*digestSize = currCipher->macAlgorithm->hash->digestSize;
	*symmetricKeySize = currCipher->cipher->secretKeySize;
	*ivSize = currCipher->cipher->ivSize;
	return noErr;
}

OSStatus
SSLGetResumableSessionInfo(
	SSLContextRef	ctx,
	Boolean			*sessionWasResumed,		// RETURNED
	void			*sessionID,				// RETURNED, mallocd by caller
	size_t			*sessionIDLength)		// IN/OUT
{
	if((ctx == NULL) || (sessionWasResumed == NULL) ||
	   (sessionID == NULL) || (sessionIDLength == NULL) ||
	   (*sessionIDLength < MAX_SESSION_ID_LENGTH)) {
		return paramErr;
	}
	if(ctx->sessionMatch) {
		*sessionWasResumed = true;
		if(ctx->sessionID.length > *sessionIDLength) {
			/* really should never happen - means ID > 32 */
			return paramErr;
		}
		if(ctx->sessionID.length) {
			/*
 			 * Note PAC-based session resumption can result in sessionMatch
			 * with no sessionID
			 */
			memmove(sessionID, ctx->sessionID.data, ctx->sessionID.length);
		}
		*sessionIDLength = ctx->sessionID.length;
	}
	else {
		*sessionWasResumed = false;
		*sessionIDLength = 0;
	}
	return noErr;
}

/*
 * Get/set enable of anonymous ciphers. Default is enabled.
 */
OSStatus
SSLSetAllowAnonymousCiphers(
	SSLContextRef	ctx,
	Boolean			enable)
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		return badReqErr;
	}
	if(ctx->validCipherSuites != NULL) {
		/* SSLSetEnabledCiphers() has already been called */
		return badReqErr;
	}
	ctx->anonCipherEnable = enable;
	return noErr;
}

OSStatus
SSLGetAllowAnonymousCiphers(
	SSLContextRef	ctx,
	Boolean			*enable)
{
	if((ctx == NULL) || (enable == NULL)) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		return badReqErr;
	}
	*enable = ctx->anonCipherEnable;
	return noErr;
}

/*
 * Override the default session cache timeout for a cache entry created for
 * the current session.
 */
OSStatus
SSLSetSessionCacheTimeout(
	SSLContextRef ctx,
	uint32_t timeoutInSeconds)
{
	if(ctx == NULL) {
		return paramErr;
	}
	ctx->sessionCacheTimeout = timeoutInSeconds;
	return noErr;
}

/*
 * Register a callback for obtaining the master_secret when performing
 * PAC-based session resumption.
 */
OSStatus
SSLInternalSetMasterSecretFunction(
	SSLContextRef ctx,
	SSLInternalMasterSecretFunction mFunc,
	const void *arg)		/* opaque to SecureTransport; app-specific */
{
	if(ctx == NULL) {
		return paramErr;
	}
	ctx->masterSecretCallback = mFunc;
	ctx->masterSecretArg = arg;
	return noErr;
}

/*
 * Provide an opaque SessionTicket for use in PAC-based session
 * resumption. Client side only. The provided ticket is sent in
 * the ClientHello message as a SessionTicket extension.
 *
 * We won't reject this on the server side, but server-side support
 * for PAC-based session resumption is currently enabled for
 * Development builds only. To fully support this for server side,
 * besides the rudimentary support that's here for Development builds,
 * we'd need a getter for the session ticket, so the app code can
 * access the SessionTicket when its SSLInternalMasterSecretFunction
 * callback is called.
 */
OSStatus SSLInternalSetSessionTicket(
   SSLContextRef ctx,
   const void *ticket,
   size_t ticketLength)
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	if(ticketLength > 0xffff) {
		/* extension data encoded with a 2-byte length! */
		return paramErr;
	}
	SSLFreeBuffer(&ctx->sessionTicket, NULL);
	return SSLCopyBufferFromData(ticket, ticketLength, &ctx->sessionTicket);
}

/*
 * ECDSA curve accessors.
 */

/*
 * Obtain the SSL_ECDSA_NamedCurve negotiated during a handshake.
 * Returns paramErr if no ECDH-related ciphersuite was negotiated.
 */
OSStatus SSLGetNegotiatedCurve(
   SSLContextRef ctx,
   SSL_ECDSA_NamedCurve *namedCurve)    /* RETURNED */
{
	if((ctx == NULL) || (namedCurve == NULL)) {
		return paramErr;
	}
	if(ctx->ecdhPeerCurve == SSL_Curve_None) {
		return paramErr;
	}
	*namedCurve = ctx->ecdhPeerCurve;
	return noErr;
}

/*
 * Obtain the number of currently enabled SSL_ECDSA_NamedCurves.
 */
OSStatus SSLGetNumberOfECDSACurves(
   SSLContextRef ctx,
   unsigned *numCurves)	/* RETURNED */
{
	if((ctx == NULL) || (numCurves == NULL)) {
		return paramErr;
	}
	*numCurves = ctx->ecdhNumCurves;
	return noErr;
}

/*
 * Obtain the ordered list of currently enabled SSL_ECDSA_NamedCurves.
 */
OSStatus SSLGetECDSACurves(
   SSLContextRef ctx,
   SSL_ECDSA_NamedCurve *namedCurves,		/* RETURNED */
   unsigned *numCurves)						/* IN/OUT */
{
	if((ctx == NULL) || (namedCurves == NULL) || (numCurves == NULL)) {
		return paramErr;
	}
	if(*numCurves < ctx->ecdhNumCurves) {
		return paramErr;
	}
	memmove(namedCurves, ctx->ecdhCurves,
		(ctx->ecdhNumCurves * sizeof(SSL_ECDSA_NamedCurve)));
	*numCurves = ctx->ecdhNumCurves;
	return noErr;
}

/*
 * Specify ordered list of allowable named curves.
 */
OSStatus SSLSetECDSACurves(
   SSLContextRef ctx,
   const SSL_ECDSA_NamedCurve *namedCurves,
   unsigned numCurves)
{
	if((ctx == NULL) || (namedCurves == NULL) || (numCurves == 0)) {
		return paramErr;
	}
	if(numCurves > SSL_ECDSA_NUM_CURVES) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	memmove(ctx->ecdhCurves, namedCurves, (numCurves * sizeof(SSL_ECDSA_NamedCurve)));
	ctx->ecdhNumCurves = numCurves;
	return noErr;
}

/*
 * Obtain the number of client authentication mechanisms specified by
 * the server in its Certificate Request message.
 * Returns paramErr if server hasn't sent a Certificate Request message
 * (i.e., client certificate state is kSSLClientCertNone).
 */
OSStatus SSLGetNumberOfClientAuthTypes(
	SSLContextRef ctx,
	unsigned *numTypes)
{
	if((ctx == NULL) || (ctx->clientCertState == kSSLClientCertNone)) {
		return paramErr;
	}
	*numTypes = ctx->numAuthTypes;
	return noErr;
}

/*
 * Obtain the client authentication mechanisms specified by
 * the server in its Certificate Request message.
 * Caller allocates returned array and specifies its size (in
 * SSLClientAuthenticationTypes) in *numType on entry; *numTypes
 * is the actual size of the returned array on successful return.
 */
OSStatus SSLGetClientAuthTypes(
   SSLContextRef ctx,
   SSLClientAuthenticationType *authTypes,		/* RETURNED */
   unsigned *numTypes)							/* IN/OUT */
{
	if((ctx == NULL) || (ctx->clientCertState == kSSLClientCertNone)) {
		return paramErr;
	}
	memmove(authTypes, ctx->clientAuthTypes,
		ctx->numAuthTypes * sizeof(SSLClientAuthenticationType));
	*numTypes = ctx->numAuthTypes;
	return noErr;
}

/*
 * Obtain the SSLClientAuthenticationType actually performed.
 * Only valid if client certificate state is kSSLClientCertSent
 * or kSSLClientCertRejected; returns paramErr otherwise.
 */
OSStatus SSLGetNegotiatedClientAuthType(
   SSLContextRef ctx,
   SSLClientAuthenticationType *authType)		/* RETURNED */
{
	if(ctx == NULL) {
		return paramErr;
	}
	*authType = ctx->negAuthType;
	return noErr;
}

/*
 * Update the negotiated client authentication type.
 * This function may be called at any time; however, note that
 * the negotiated authentication type will be SSLClientAuthNone
 * until both of the following have taken place (in either order):
 *   - a CertificateRequest message from the server has been processed
 *   - a client certificate has been specified
 * As such, this function (only) needs to be called from (both)
 * SSLProcessCertificateRequest and SSLSetCertificate.
 */
OSStatus SSLUpdateNegotiatedClientAuthType(
	SSLContextRef ctx)
{
	if(ctx == NULL) {
		return paramErr;
	}
	/*
	 * See if we have a signing cert that matches one of the
	 * allowed auth types. The x509Requested flag indicates "we
	 * have a cert that we think the server will accept".
	 */
	ctx->x509Requested = 0;
	ctx->negAuthType = SSLClientAuthNone;
	if(ctx->signingPrivKeyRef != NULL) {
        CFIndex ourKeyAlg = sslPubKeyGetAlgorithmID(ctx->signingPubKey);
		unsigned i;
		for(i=0; i<ctx->numAuthTypes; i++) {
			switch(ctx->clientAuthTypes[i]) {
				case SSLClientAuth_RSASign:
					if(ourKeyAlg == kSecRSAAlgorithmID) {
						ctx->x509Requested = 1;
						ctx->negAuthType = SSLClientAuth_RSASign;
					}
					break;
			#if SSL_ENABLE_ECDSA_SIGN_AUTH
				case SSLClientAuth_ECDSASign:
			#endif
			#if SSL_ENABLE_ECDSA_FIXED_ECDH_AUTH
				case SSLClientAuth_ECDSAFixedECDH:
			#endif
					if((ourKeyAlg == kSecECDSAAlgorithmID) &&
					   (ctx->ourSignerAlg == kSecECDSAAlgorithmID)) {
						ctx->x509Requested = 1;
						ctx->negAuthType = ctx->clientAuthTypes[i];
					}
					break;
			#if SSL_ENABLE_RSA_FIXED_ECDH_AUTH
				case SSLClientAuth_RSAFixedECDH:
					/* Odd case, we differ from our signer */
					if((ourKeyAlg == kSecECDSAAlgorithmID) &&
					   (ctx->ourSignerAlg == kSecRSAAlgorithmID)) {
						ctx->x509Requested = 1;
						ctx->negAuthType = SSLClientAuth_RSAFixedECDH;
					}
					break;
			#endif
				default:
					/* None others supported */
					break;
			}
			if(ctx->x509Requested) {
				sslLogNegotiateDebug("===CHOOSING authType %d", (int)ctx->negAuthType);
				break;
			}
		}	/* parsing authTypes */
	}	/* we have a signing key */

	return noErr;
}

OSStatus SSLGetNumberOfSignatureAlgorithms(
    SSLContextRef ctx,
    unsigned *numSigAlgs)
{
	if((ctx == NULL) || (ctx->clientCertState == kSSLClientCertNone)) {
		return paramErr;
	}
	*numSigAlgs = ctx->numServerSigAlgs;
	return noErr;
}

OSStatus SSLGetSignatureAlgorithms(
    SSLContextRef ctx,
    SSLSignatureAndHashAlgorithm *sigAlgs,		/* RETURNED */
    unsigned *numSigAlgs)							/* IN/OUT */
{
	if((ctx == NULL) || (ctx->clientCertState == kSSLClientCertNone)) {
		return paramErr;
	}
	memmove(sigAlgs, ctx->serverSigAlgs,
            ctx->numServerSigAlgs * sizeof(SSLSignatureAndHashAlgorithm));
	*numSigAlgs = ctx->numServerSigAlgs;
	return noErr;
}
