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
 * sslContext.h - Private SSL typedefs: SSLContext and its components
 */

#ifndef _SSLCONTEXT_H_
#define _SSLCONTEXT_H_ 1

#include "SecureTransport.h"
#include "sslBuildFlags.h"

#ifdef USE_CDSA_CRYPTO
#include <Security/cssmtype.h>
#else
#if TARGET_OS_IPHONE
#include <Security/SecDH.h>
#include <Security/SecKeyInternal.h>
#else
#include "../sec/Security/SecDH.h"  // hack to get SecDH.
// typedef struct OpaqueSecDHContext *SecDHContext;
#endif
#include <corecrypto/ccec.h>
#endif

#include <CoreFoundation/CFRuntime.h>
#include <AssertMacros.h>

#include "sslPriv.h"
#include "tls_ssl.h"
#include "sslDigests.h"
#include "sslRecord.h"
#include "cipherSpecs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{   SSLReadFunc         read;
    SSLWriteFunc        write;
    SSLConnectionRef   	ioRef;
} IOContext;


#ifdef USE_SSLCERTIFICATE

/*
 * An element in a certificate chain.
 */
typedef struct SSLCertificate
{   
    struct SSLCertificate   *next;
    SSLBuffer               derCert;
} SSLCertificate;

size_t SSLGetCertificateChainLength(
    const SSLCertificate *c);
OSStatus sslDeleteCertificateChain(
    SSLCertificate 		*certs,
    SSLContext 			*ctx);

#endif /* USE_SSLCERTIFICATE */


#include "sslHandshake.h"

typedef struct WaitingMessage
{
    struct WaitingMessage *next;
    SSLRecord   rec;
} WaitingMessage;

typedef struct DNListElem
{   struct DNListElem   *next;
    SSLBuffer		    derDN;
} DNListElem;

#ifdef USE_CDSA_CRYPTO

/* Public part of asymmetric key. */
typedef struct SSLPubKey
{
    CSSM_KEY key;
    CSSM_CSP_HANDLE csp;                /* may not be needed, we figure this
                                         * one out by trial&error, right? */
} SSLPubKey;

/* Private part of asymmetric key. */
typedef struct SSLPrivKey
{
	SecKeyRef key;

} SSLPrivKey;

#else /* !USE_CDSA_CRYPTO */

#if TARGET_OS_IPHONE
typedef struct __SecKey SSLPubKey;
typedef struct __SecKey SSLPrivKey;
#else
typedef struct OpaqueSecKeyRef SSLPubKey;
typedef struct OpaqueSecKeyRef SSLPrivKey;
#endif
/*
 * Convert SSLPrivKey/SSLPubKey types to a platform SecKeyRef
 * (currently a no-op)
 */
#define SECKEYREF(sslkey) (sslkey)

#endif

typedef struct {
    SSLCipherSuite      		      cipherSpec;
    KeyExchangeMethod   		      keyExchangeMethod;
    uint8_t                           keySize;  /* size in bytes */
    uint8_t                           ivSize;
    uint8_t                           blockSize;
    uint8_t                           macSize;
    HMAC_Algs                         macAlg;
} SSLCipherSpecParams;

struct SSLContext
{
	CFRuntimeBase		_base;
    IOContext           ioCtx;

    const struct SSLRecordFuncs *recFuncs;
    SSLRecordContextRef recCtx;
    
	/* 
	 * Prior to successful protocol negotiation, negProtocolVersion
	 * is SSL_Version_Undetermined. Subsequent to successful
	 * negotiation, negProtocolVersion contains the actual over-the-wire
	 * protocol value.
	 *
	 * The Boolean versionEnable flags are set by
	 * SSLSetProtocolVersionEnabled or SSLSetProtocolVersion and
	 * remain invariant once negotiation has started. If there
	 * were a large number of these and/or we were adding new
	 * protocol versions on a regular basis, we'd probably want
	 * to implement these as a word of flags. For now, in the
	 * real world, this is the most straightforward implementation.
	 */
    SSLProtocolVersion  negProtocolVersion;	/* negotiated */
    SSLProtocolVersion  clientReqProtocol;	/* requested by client in hello msg */
    SSLProtocolVersion  minProtocolVersion;
    SSLProtocolVersion  maxProtocolVersion;
    Boolean             isDTLS;             /* if this is a Datagram Context */
    SSLProtocolSide     protocolSide;		/* ConnectionEnd enum { server, client } in rfc5246. */

    const struct _SslTlsCallouts *sslTslCalls; /* selects between SSLv3, TLSv1 and TLSv1.2 */

    SSLPrivKey          *signingPrivKeyRef;  /* our private signing key */
    SSLPubKey           *signingPubKey;      /* our public signing key */

    SSLPrivKey          *encryptPrivKeyRef;  /* our private encrypt key, for
                                              * server-initiated key exchange */
    SSLPubKey           *encryptPubKey;      /* public version of above */

    SSLPubKey           *peerPubKey;

#ifdef USE_SSLCERTIFICATE
  	/*
  	 * Various cert chains.
  	 * For all three, the root is the first in the chain.
  	 */
    SSLCertificate      *localCert;
    SSLCertificate		*encryptCert;
    SSLCertificate      *peerCert;
    CSSM_ALGORITHMS		ourSignerAlg;	/* algorithm of the signer of localCert */
#else
  	/*
  	 * Various cert chains.
  	 * For all three, the root is the last in the chain.
  	 */
	CFArrayRef			localCert;
	CFArrayRef			encryptCert;
	CFArrayRef			peerCert;
    CFIndex          ourSignerAlg;	/* algorithm of the signer of localCert */
#endif /* !USE_SSLCERTIFICATE */

	/*
	 * The arrays we are given via SSLSetCertificate() and SSLSetEncryptionCertificate().
	 * We keep them here, refcounted, solely for the associated getters.
	 */
	CFArrayRef			localCertArray;
	CFArrayRef			encryptCertArray;

	/* peer certs as SecTrustRef */
	SecTrustRef			peerSecTrust;

#ifdef USE_CDSA_CRYPTO

    /*
     * trusted root certs as specified in SSLSetTrustedRoots()
     */
    CFArrayRef			trustedCerts;

    /* for symmetric cipher and RNG */
    CSSM_CSP_HANDLE		cspHand;

    /* session-wide handles for Apple TP, CL */
    CSSM_TP_HANDLE		tpHand;
    CSSM_CL_HANDLE		clHand;
#else

#ifdef USE_SSLCERTIFICATE
    size_t				numTrustedCerts;
    SSLCertificate		*trustedCerts;
#else
    CFMutableArrayRef   trustedCerts;
    Boolean             trustedCertsOnly;
#endif /* !USE_SSLCERTIFICATE */

#endif /* !USE_CDSA_CRYPTO */

    /*
     * trusted leaf certs as specified in SSLSetTrustedLeafCertificates()
     */
    CFArrayRef			trustedLeafCerts;

	#if		APPLE_DH
    SSLBuffer			dhPeerPublic;
	SSLBuffer			dhExchangePublic;
	SSLBuffer			dhParamsEncoded;	/* PKCS3 encoded blob - prime + generator */
#ifdef USE_CDSA_CRYPTO
	CSSM_KEY_PTR		dhPrivate;
#else
	SecDHContext        secDHContext;
#endif /* !USE_CDSA_CRYPTO */
	#endif	/* APPLE_DH */

	/*
	 * ECDH support
	 *
	 * ecdhCurves[] is the set of currently configured curves; the number
	 * of valid curves is ecdhNumCurves.
	 */
	SSL_ECDSA_NamedCurve	ecdhCurves[SSL_ECDSA_NUM_CURVES];
	unsigned				ecdhNumCurves;

	SSLBuffer				ecdhPeerPublic;		/* peer's public ECDH key as ECPoint */
	SSL_ECDSA_NamedCurve	ecdhPeerCurve;		/* named curve associated with ecdhPeerPublic or
												 *    peerPubKey */
    SSLBuffer				ecdhExchangePublic;	/* Our public key as ECPoint */
#ifdef USE_CDSA_CRYPTO
	CSSM_KEY_PTR			ecdhPrivate;		/* our private key */
	CSSM_CSP_HANDLE			ecdhPrivCspHand;
#else
    ccec_full_ctx_decl(ccn_sizeof(521), ecdhContext);   // Big enough to hold a 521 bit ecdh key pair.
#endif /* !USE_CDSA_CRYPTO */

	Boolean					allowExpiredCerts;
	Boolean					allowExpiredRoots;
	Boolean					enableCertVerify;

    SSLBuffer           dtlsCookie;             /* DTLS ClientHello cookie */
    Boolean             cookieVerified;         /* Mark if cookie was verified */
    uint16_t            hdskMessageSeq;         /* Handshake Seq Num to be sent */
    uint32_t            hdskMessageRetryCount;  /* retry cont for a given flight of messages */
    uint16_t            hdskMessageSeqNext;     /* Handshake Seq Num to be received */
    SSLHandshakeMsg     hdskMessageCurrent;     /* Current Handshake Message */
    uint16_t            hdskMessageCurrentOfs;  /* Offset in current Handshake Message */

    SSLBuffer		    sessionID;

    SSLBuffer			peerID;
    SSLBuffer			resumableSession;

	char				*peerDomainName;
	size_t				peerDomainNameLen;
	
    uint8_t             readCipher_ready;
    uint8_t             writeCipher_ready;
    uint8_t             readPending_ready;
    uint8_t             writePending_ready;
    uint8_t             prevCipher_ready;             /* previous write cipher context, used for retransmit */
    
    uint16_t            selectedCipher;			/* currently selected */
    SSLCipherSpecParams selectedCipherSpecParams;     /* ditto */

    SSLCipherSuite		*validCipherSuites;		/* context's valid suites */
    size_t              numValidCipherSuites;	/* size of validCipherSuites */
#if ENABLE_SSLV2
	unsigned			numValidNonSSLv2Suites;	/* number of entries in validCipherSpecs that
												 * are *not* SSLv2 only */
#endif
    SSLHandshakeState   state;

	/* server-side only */
    SSLAuthenticate		clientAuth;				/* kNeverAuthenticate, etc. */
    Boolean				tryClientAuth;

	/* client and server */
	SSLClientCertificateState	clientCertState;

    DNListElem          *acceptableDNList;		/* client and server */
	CFMutableArrayRef	acceptableCAs;			/* server only - SecCertificateRefs */

    bool                certRequested;
    bool                certSent;
    bool                certReceived;
    bool                x509Requested;

    uint8_t             clientRandom[SSL_CLIENT_SRVR_RAND_SIZE];
    uint8_t             serverRandom[SSL_CLIENT_SRVR_RAND_SIZE];
    SSLBuffer   		preMasterSecret;
    uint8_t             masterSecret[SSL_MASTER_SECRET_SIZE];

	/* running digests of all handshake messages */
    SSLBuffer   		shaState, md5State, sha256State, sha512State;

    SSLBuffer		    fragmentedMessageCache;

    unsigned            ssl2ChallengeLength;
    unsigned			ssl2ConnectionIDLength;
    unsigned            sessionMatch;

    /* Queue a full flight of messages */
    WaitingMessage      *messageWriteQueue;
    Boolean             messageQueueContainsChangeCipherSpec;
    
	/* Transport layer fields */
    SSLBuffer			receivedDataBuffer;
    size_t              receivedDataPos;

	Boolean				allowAnyRoot;		// don't require known roots
	Boolean				sentFatalAlert;		// this session terminated by fatal alert
	Boolean				rsaBlindingEnable;
	Boolean				oneByteRecordEnable;    /* enable 1/n-1 data splitting for TLSv1 and SSLv3 */
	Boolean				wroteAppData;           /* at least one write completed with current writeCipher */
    Boolean             allowServerIdentityChange; /* allow server identity change on renegotiation
                                                    disallowed by default to avoid triple handshake attack */

	/* optional session cache timeout (in seconds) override - 0 means default */
	uint32_t 				sessionCacheTimeout;

	/* optional SessionTicket */
	SSLBuffer			sessionTicket;

	/* optional callback to obtain master secret, with its opaque arg */
	SSLInternalMasterSecretFunction	masterSecretCallback;
	const void 			*masterSecretArg;

	#if 	SSL_PAC_SERVER_ENABLE
	/* server PAC resume sets serverRandom early to allow for secret acquisition */
	uint8_t				serverRandomValid;
	#endif

	Boolean				anonCipherEnable;

	/* optional switches to enable additional returns from SSLHandshake */
    Boolean             breakOnServerAuth;
    Boolean             breakOnCertRequest;
    Boolean             breakOnClientAuth;
    Boolean             signalServerAuth;
    Boolean             signalCertRequest;
    Boolean             signalClientAuth;

	/* true iff ECDSA/ECDH ciphers are configured */
	Boolean				ecdsaEnable;

	/* List of server-specified client auth types */
	unsigned					numAuthTypes;
	SSLClientAuthenticationType	*clientAuthTypes;

	/* client auth type actually negotiated */
	SSLClientAuthenticationType	negAuthType;

    /* List of client-specified supported_signature_algorithms (for key exchange) */
	unsigned					 numClientSigAlgs;
	SSLSignatureAndHashAlgorithm *clientSigAlgs;
    /* List of server-specified supported_signature_algorithms (for client cert) */
	unsigned					 numServerSigAlgs;
	SSLSignatureAndHashAlgorithm *serverSigAlgs;


    /* Timeout for DTLS retransmit */
    CFAbsoluteTime      timeout_deadline;
    CFAbsoluteTime      timeout_duration;
    size_t              mtu;

    /* RFC 5746: Secure renegotiation */
    Boolean             secure_renegotiation;
    Boolean             secure_renegotiation_received;
    SSLBuffer           ownVerifyData;
    SSLBuffer           peerVerifyData;

    /* RFC 4279: TLS PSK */
    SSLBuffer           pskSharedSecret;
    SSLBuffer           pskIdentity;

    /* TLS False Start */
    Boolean             falseStartEnabled; //FalseStart enabled (by API call)
};

OSStatus SSLUpdateNegotiatedClientAuthType(SSLContextRef ctx);

Boolean sslIsSessionActive(const SSLContext *ctx);

static inline bool sslVersionIsLikeTls12(SSLContext *ctx)
{
    check(ctx->negProtocolVersion!=SSL_Version_Undetermined);
    return ctx->isDTLS ? ctx->negProtocolVersion > DTLS_Version_1_0 : ctx->negProtocolVersion >= TLS_Version_1_2;
}

#ifdef __cplusplus
}
#endif

#endif /* _SSLCONTEXT_H_ */
