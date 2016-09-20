//
//  tls_handshake_priv.h
//  libsecurity_ssl
//
//  Created by Fabrice Gautier on 9/3/13.
//
//

/* This header to be only included by handshake layer implementation files */

#ifndef _TLS_HANDSHAKE_PRIV_H_
#define _TLS_HANDSHAKE_PRIV_H_ 1

#include <tls_handshake.h>
#include <tls_ciphersuites.h>

#include "sslHandshake.h"
#include "sslBuildFlags.h"
#include "CipherSuite.h"

#include <corecrypto/ccec.h>
#include <corecrypto/ccdh.h>
#include <corecrypto/ccrsa.h>
#include <AssertMacros.h>


/* Composite type for public asymmetric key. */
#include <corecrypto/ccrsa.h>
#include <corecrypto/ccec.h>

typedef struct _SSLPubKey SSLPubKey;

struct _SSLPubKey {
    bool isRSA;
    union {
        ccrsa_pub_ctx_t rsa;
        ccec_pub_ctx_t ecc;
    };
};


struct _tls_private_key {
    tls_private_key_ctx_t ctx;
    tls_private_key_ctx_release ctx_release;
    tls_private_key_desc_t desc;
};


enum {
    errSSLSuccess                = 0,       /* No error. */
    errSSLUnimplemented          = -4,      /* Not implemented */
    errSSLParam                  = -50,     /* One or more parameters passed to a function were not valid. */
    errSSLAllocate               = -108,    /* Failed to allocate memory. */

	errSSLProtocol				= -9800,	/* SSL protocol error */
	errSSLNegotiation			= -9801,	/* Cipher Suite negotiation failure */
	errSSLFatalAlert			= -9802,	/* Fatal alert */
	errSSLWouldBlock			= -9803,	/* I/O would block (not fatal) */
    errSSLSessionNotFound 		= -9804,	/* attempt to restore an unknown session */
    errSSLClosedGraceful 		= -9805,	/* connection closed gracefully */
    errSSLClosedAbort 			= -9806,	/* connection closed via error */
    errSSLXCertChainInvalid 	= -9807,	/* invalid certificate chain */
    errSSLBadCert				= -9808,	/* bad certificate format */
	errSSLCrypto				= -9809,	/* underlying cryptographic error */
	errSSLInternal				= -9810,	/* Internal error */
	errSSLModuleAttach			= -9811,	/* module attach failure */
    errSSLUnknownRootCert		= -9812,	/* valid cert chain, untrusted root */
    errSSLNoRootCert			= -9813,	/* cert chain not verified by root */
	errSSLCertExpired			= -9814,	/* chain had an expired cert */
	errSSLCertNotYetValid		= -9815,	/* chain had a cert not yet valid */
	errSSLClosedNoNotify		= -9816,	/* server closed session with no notification */
	errSSLBufferOverflow		= -9817,	/* insufficient buffer provided */
	errSSLBadCipherSuite		= -9818,	/* bad SSLCipherSuite */

	/* fatal errors detected by peer */
	errSSLPeerUnexpectedMsg		= -9819,	/* unexpected message received */
	errSSLPeerBadRecordMac		= -9820,	/* bad MAC */
	errSSLPeerDecryptionFail	= -9821,	/* decryption failed */
	errSSLPeerRecordOverflow	= -9822,	/* record overflow */
	errSSLPeerDecompressFail	= -9823,	/* decompression failure */
	errSSLPeerHandshakeFail		= -9824,	/* handshake failure */
	errSSLPeerBadCert			= -9825,	/* misc. bad certificate */
	errSSLPeerUnsupportedCert	= -9826,	/* bad unsupported cert format */
	errSSLPeerCertRevoked		= -9827,	/* certificate revoked */
	errSSLPeerCertExpired		= -9828,	/* certificate expired */
	errSSLPeerCertUnknown		= -9829,	/* unknown certificate */
	errSSLIllegalParam			= -9830,	/* illegal parameter */
	errSSLPeerUnknownCA 		= -9831,	/* unknown Cert Authority */
	errSSLPeerAccessDenied		= -9832,	/* access denied */
	errSSLPeerDecodeError		= -9833,	/* decoding error */
	errSSLPeerDecryptError		= -9834,	/* decryption error */
	errSSLPeerExportRestriction	= -9835,	/* export restriction */
	errSSLPeerProtocolVersion	= -9836,	/* bad protocol version */
	errSSLPeerInsufficientSecurity = -9837,	/* insufficient security */
	errSSLPeerInternalError		= -9838,	/* internal error */
	errSSLPeerUserCancelled		= -9839,	/* user canceled */
	errSSLPeerNoRenegotiation	= -9840,	/* no renegotiation allowed */

	/* non-fatal result codes */
	errSSLPeerAuthCompleted     = -9841,    /* peer cert is valid, or was ignored if verification disabled */
	errSSLClientCertRequested	= -9842,	/* server has requested a client cert */

	/* more errors detected by us */
	errSSLHostNameMismatch		= -9843,	/* peer host name mismatch */
	errSSLConnectionRefused		= -9844,	/* peer dropped connection before responding */
	errSSLDecryptionFail		= -9845,	/* decryption failure */
	errSSLBadRecordMac			= -9846,	/* bad MAC */
	errSSLRecordOverflow		= -9847,	/* record overflow */
	errSSLBadConfiguration		= -9848,	/* configuration error */
    errSSLUnexpectedRecord      = -9849,	/* unexpected (skipped) record in DTLS */
    errSSLWeakPeerEphemeralDHKey = -9850,	/* weak ephemeral dh key  */

};

typedef struct {
    uint16_t                          cipherSpec;
    KeyExchangeMethod   		      keyExchangeMethod;
    uint8_t                           keySize;  /* size in bytes */
    uint8_t                           ivSize;
    uint8_t                           blockSize;
    uint8_t                           macSize;
    HMAC_Algs                         macAlg;
} SSLCipherSpecParams;

typedef enum {
	kNeverAuthenticate,			/* skip client authentication */
	kAlwaysAuthenticate,		/* require it */
	kTryAuthenticate			/* try to authenticate, but not an error
								 * if client doesn't have a cert */
} SSLAuthenticate;

/*
 * Status of client certificate exchange (which is optional
 * for both server and client).
 */
typedef enum {
	/* Server hasn't asked for a cert. Client hasn't sent one. */
	kSSLClientCertNone,
	/* Server has asked for a cert, but client didn't send it. */
	kSSLClientCertRequested,
	/*
	 * Server side: We asked for a cert, client sent one, we validated
	 *				it OK. App can inspect the cert via
	 *				SSLGetPeerCertificates().
	 * Client side: server asked for one, we sent it.
	 */
	kSSLClientCertSent,
	/*
	 * Client sent a cert but failed validation. Server side only.
	 * Server app can inspect the cert via SSLGetPeerCertificates().
	 */
	kSSLClientCertRejected
} SSLClientCertificateState;


/***
 *** Each of {TLS, SSLv3} implements each of these functions.
 ***/

typedef int (*generateKeyMaterialFcn) (
                                            tls_buffer key, 					// caller mallocs and specifies length of
                                            //   required key material here
                                            tls_handshake_t ctx);

typedef int (*generateExportKeyAndIvFcn) (
                                               tls_handshake_t ctx,				// clientRandom, serverRandom valid
                                               const tls_buffer clientWriteKey,
                                               const tls_buffer serverWriteKey,
                                               tls_buffer finalClientWriteKey,	// RETURNED, mallocd by caller
                                               tls_buffer finalServerWriteKey,	// RETURNED, mallocd by caller
                                               tls_buffer finalClientIV,		// RETURNED, mallocd by caller
                                               tls_buffer finalServerIV);		// RETURNED, mallocd by caller

/*
 * On entry: clientRandom, serverRandom, preMasterSecret valid
 * On return: masterSecret valid
 */
typedef int (*generateMasterSecretFcn) (
                                             tls_handshake_t ctx);

typedef int (*computeFinishedMacFcn) (
                                           tls_handshake_t ctx,
                                           tls_buffer finished, 		// output - mallocd by caller
                                           bool isServer);

typedef int (*computeCertVfyMacFcn) (
                                          tls_handshake_t ctx,
                                          tls_buffer *finished,		// output - mallocd by caller
                                          tls_hash_algorithm hash);    //only used in TLS 1.2


typedef struct _SslTlsCallouts {
	generateKeyMaterialFcn		generateKeyMaterial;
	generateMasterSecretFcn		generateMasterSecret;
	computeFinishedMacFcn		computeFinishedMac;
	computeCertVfyMacFcn		computeCertVfyMac;
} SslTlsCallouts;


/* From ssl3Callouts.c and tls1Callouts.c */
extern const SslTlsCallouts	Ssl3Callouts;
extern const SslTlsCallouts	Tls1Callouts;
extern const SslTlsCallouts Tls12Callouts;


typedef struct WaitingMessage
{
    struct WaitingMessage *next;
    tls_buffer   rec;
    uint8_t     contentType;
} WaitingMessage;


/* The size of of client- and server-generated random numbers in hello messages. */
#define SSL_CLIENT_SRVR_RAND_SIZE		32
/* The size of the pre-master and master secrets. */
#define SSL_RSA_PREMASTER_SECRET_SIZE	48
#define SSL_MASTER_SECRET_SIZE			48

struct _tls_handshake_s {

	/*
	 * Prior to successful protocol negotiation, negProtocolVersion
	 * is tls_protocol_version_Undertermined. Subsequent to successful
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
    tls_protocol_version  negProtocolVersion;	/* negotiated */
    tls_protocol_version  clientReqProtocol;	/* requested by client in hello msg */
    tls_protocol_version  minProtocolVersion;
    tls_protocol_version  maxProtocolVersion;
    bool                isDTLS;             /* if this is a Datagram Context */
    bool                isServer;           /* if this is a server Context */

    const struct _SslTlsCallouts *sslTslCalls; /* selects between SSLv3, TLSv1 and TLSv1.2 */

    tls_private_key_t   signingPrivKeyRef;  /* our private signing key */
    SSLPubKey           peerPubKey;         /* Public key (extracted from the peer certificate) */

  	/*
  	 * Various cert chains.
  	 * For all three, the root is the first in the chain.
  	 */
    SSLCertificate      *localCert;
    SSLCertificate      *peerCert;

    /* result of peer cert evaluation */
    tls_handshake_trust_t peerTrust;

#if		APPLE_DH
    unsigned            dhMinGroupSize;     /* Minimum allowed DH group size */
    tls_buffer          dhPeerPublic;       /* Peer public DH key */
    ccdh_gp_t           dhParams;           /* native dh parameter object */
    ccdh_full_ctx_t     dhContext;          /* Our private DH key */
#endif	/* APPLE_DH */

	/*
	 * ECDH support
	 *
	 * ecdhCurves[] is the set of currently configured curves; the number
	 * of valid curves is ecdhNumCurves.
	 */
	uint16_t            *ecdhCurves;
    unsigned			ecdhNumCurves;

	tls_buffer			ecdhPeerPublic;         /* peer's public ECDH key as ECPoint */
	tls_named_curve     ecdhPeerCurve;          /* named curve associated with ecdhPeerPublic or
												 *    peerPubKey */
    ccec_full_ctx_t     ecdhContext;            /* Our private, ephemeral, ECDH Key */

#if ALLOW_RSA_SERVER_KEY_EXCHANGE
    /*
     * RSA Ephemeral Hack
     */
    bool                forceRsaServerKeyExchange;
    SSLPubKey           rsaEncryptPubKey;
#endif


    tls_buffer          dtlsCookie;             /* DTLS ClientHello cookie */
    bool                cookieVerified;         /* Mark if cookie was verified */
    uint16_t            hdskMessageSeq;         /* Handshake Seq Num to be sent */
    uint32_t            hdskMessageRetryCount;  /* retry cont for a given flight of messages */
    uint16_t            hdskMessageSeqNext;     /* Handshake Seq Num to be received */
    SSLHandshakeMsg     hdskMessageCurrent;     /* Current Handshake Message */
    uint16_t            hdskMessageCurrentOfs;  /* Offset in current Handshake Message */

    tls_buffer		    peerID;                 /* For the client only - used as the key to save session information in the session cache */
    tls_buffer          resumableSession;       /* Both client and server - session data restored from the session cache */
    bool                allowResumption;        /* allowSessionResumption */
    tls_buffer          proposedSessionID;      /* SessionID proposed by client */
    tls_buffer		    sessionID;              /* This is the current session, as sent by the server */
    bool                sessionMatch;

    bool                allowRenegotiation;     /* allow renegotiation */

    bool                readCipher_ready;
    bool                writeCipher_ready;
    bool                readPending_ready;
    bool                writePending_ready;
    bool                prevCipher_ready;           /* previous write cipher context, used for retransmit */

    uint16_t            selectedCipher;             /* currently selected */
    SSLCipherSpecParams selectedCipherSpecParams;   /* ditto */

    uint16_t            *enabledCipherSuites;       /* context's valid suites */
    unsigned            numEnabledCipherSuites;     /* size of validCipherSuites */
    uint16_t            *requestedCipherSuites;     /* requested ciphersuites in the clientHello - server only */
    unsigned            numRequestedCipherSuites;   /* count of requestedCipherSuites - server only */
    SSLHandshakeState   state;

    DNListElem          *acceptableDNList;
	tls_buffer          peerDomainName;
    char                *userAgent;             /* User Agent for diagnostic purpose */

    bool                advanceHandshake;       /* true if message was processed but state was not advanced */
    tls_handshake_message_t currentMessage;     /* which message was last processed before callback paused the handshake */

	/* server-side only */
    bool				tryClientAuth;

	/* client and server */
	SSLClientCertificateState	clientCertState;

    bool                certRequested;
    bool                certSent;
    bool                certReceived;
    bool                x509Requested;

    uint8_t             clientRandom[SSL_CLIENT_SRVR_RAND_SIZE];
    uint8_t             serverRandom[SSL_CLIENT_SRVR_RAND_SIZE];
    tls_buffer   		preMasterSecret;
    uint8_t             masterSecret[SSL_MASTER_SECRET_SIZE];

	/* running digests of all handshake messages */
    tls_buffer   		shaState, md5State, sha256State, sha384State, sha512State;

    tls_buffer		    fragmentedMessageCache;

    /* Queue a full flight of messages */
    WaitingMessage      *messageWriteQueue;
    bool                messageQueueContainsChangeCipherSpec;

	/* Transport layer fields */
    tls_buffer			receivedDataBuffer;
    size_t              receivedDataPos;

	bool                sentFatalAlert;		// this session terminated by fatal alert

	/* SessionTicket support (RFC 5077) */
    bool                sessionTicket_enabled;    /* Client/Server: sessionTicket extension is enabled */
    bool                sessionTicket_announced;  /* Client: sessionTicket extension was sent, Server: sessionTicket extension was received */
    bool                sessionTicket_confirmed;  /* Client: sessionTicket extension was received, Server: sessionTicket extension was sent */
	tls_buffer			sessionTicket;            /* Session Ticket (as sent by server) */
    uint32_t			sessionTicket_lifetime;   /* Session Ticket lifetime hint (as sent by server) */

    /* Externaly set SessionTicket (eg: for EAP-FAST) */
    tls_buffer			externalSessionTicket;            /* Session Ticket (as sent by server) */

	/* optional callback to obtain master secret, with its opaque arg */
	tls_handshake_master_secret_function_t	masterSecretCallback;
	const void 			*masterSecretArg;

    /* Extended master secret support RFC 7627 */
    bool                extMSEnabled;
    bool                extMSReceived;
#if 	SSL_PAC_SERVER_ENABLE
	/* server PAC resume sets serverRandom early to allow for secret acquisition */
	uint8_t				serverRandomValid;
#endif

	bool				anonCipherEnable;

	/* true iff ECDSA/ECDH ciphers are configured */
	bool                    ecdsaEnable;

	/* List of server-specified client auth types */
	unsigned                numAuthTypes;
	tls_client_auth_type    *clientAuthTypes;

	/* client auth type actually negotiated */
	tls_client_auth_type	negAuthType;

    /* List of peer supported_signature_algorithms */
	unsigned                         numPeerSigAlgs;
	tls_signature_and_hash_algorithm *peerSigAlgs;

    /* List of locally supported_signature_algorithms */
    unsigned                         numLocalSigAlgs;
    tls_signature_and_hash_algorithm *localSigAlgs;

    tls_signature_and_hash_algorithm certSigAlg;  /* selected by client */
    tls_signature_and_hash_algorithm kxSigAlg;  /* selected by server */



    /* Retransmit attempt number for DTLS */
    int                 retransmit_attempt;
    size_t              mtu;

    /* RFC 5746: Secure renegotiation */
    bool                secure_renegotiation;
    bool                secure_renegotiation_received;
    bool                empty_renegotation_info_scsv;
    tls_buffer           ownVerifyData;
    tls_buffer           peerVerifyData;

    /* https://secure-resumption.com */
    bool                allowServerIdentityChange;

    /* RFC 4279: TLS PSK */
    tls_buffer           pskSharedSecret;
    tls_buffer           pskIdentity;
    
    /* TLS False Start */
    bool                falseStartEnabled; //FalseStart enabled (by API call)

    /* NPN */
    bool                npn_enabled;    /* Client/Server: npn is enabled */
    bool                npn_announced;  /* Client: npn extension was sent, Server: npn extension was received */
    bool                npn_confirmed;  /* Client: npn extension was received, Server: npn extension was sent */
    bool                npn_received;   /* Server: npn message was received */
    tls_buffer          npnOwnData;     /* Client: selected protocol sent, Server: list of supported protocol sent */
    tls_buffer          npnPeerData;    /* Client: list of supported protocol received, Server: selected protocol received */

    /* ALPN */
    bool                alpn_enabled;    /* Client: alpn is enabled */
    bool                alpn_announced;  /* Client: alpn extension was sent, Server: alpn extension was received */
    bool                alpn_confirmed;  /* Client: alpn extension was received, Server: alpn extension was sent */
    bool                alpn_received;   /* Server: alpn message was received */
    tls_buffer          alpnOwnData;     /* Client: supported protocols sent, Server: selected protocol sent */
    tls_buffer          alpnPeerData;    /* Client: select protocol received, Server: list of supported protocol received */

    /* OCSP Stapling */
    bool                ocsp_enabled;            /* Client/Server: ocsp stapling is enabled */
    bool                ocsp_peer_enabled;       /* Client/Server: ocsp extension was received from peer */
    tls_buffer          ocsp_request_extensions; /* Sent by client, received by server */
    tls_buffer_list_t   *ocsp_responder_id_list; /* Sent by client, received by server */
    bool                ocsp_response_received;  /* Client: ocsp response was received */
    tls_buffer          ocsp_response;           /* Sent by server, received by client */

    /* Fallback behavior */
    bool                fallback;       /* Client: enable fallback behaviors */

    /* RFC 7507 Fallback SCSV */
    bool                tls_fallback_scsv;  /* Server: Fallback SCSV received */

    /* SCT extension (RFC 6962) */
    bool                sct_enabled;        /* Client: send sct extension */
    bool                sct_peer_enabled;   /* Server: Client has sent extension */
    tls_buffer_list_t   *sct_list;          /* Sent by Server, received by Client */

    /* RFC 4492 ECC extensions */
    /* server only - list of client-specified supported EC curves */
    uint16_t            *requested_ecdh_curves;
    unsigned            num_ec_curves;


    /* config */
    tls_handshake_config_t config;          /* preset configuration */
    /* callbacks, context, etc... */

    tls_handshake_ctx_t ctx;
    tls_handshake_ctx_t *callback_ctx;
    tls_handshake_callbacks_t *callbacks;
};

static inline bool sslVersionIsLikeTls12(tls_handshake_t ctx)
{
    check(ctx->negProtocolVersion!=tls_protocol_version_Undertermined);
    return ctx->isDTLS ? ctx->negProtocolVersion > tls_protocol_version_DTLS_1_0 : ctx->negProtocolVersion >= tls_protocol_version_TLS_1_2;
}


static inline size_t getMaxDataGramSize(tls_handshake_t ctx)
{
    size_t max_fragment_size = ctx->mtu-13; /* 13 = dtls record header */

    SSLCipherSpecParams *currCipher = &ctx->selectedCipherSpecParams;

    size_t blockSize = currCipher->blockSize;
    size_t macSize = currCipher->macSize;

    if (blockSize > 0) {
        /* max_fragment_size must be a multiple of blocksize */
        max_fragment_size = max_fragment_size & ~(blockSize-1);
        max_fragment_size -= blockSize; /* 1 block for IV */
        max_fragment_size -= 1; /* 1 byte for pad length */
    }

    /* less the mac size */
    max_fragment_size -= macSize;

    /* Thats just a sanity check */
    assert(max_fragment_size<ctx->mtu);

    return max_fragment_size;
}

static inline
int SSLHandshakeHeaderSize(tls_handshake_t ctx)
{
    if(ctx->isDTLS)
        return 12;
    else
        return 4;
}

static inline
void sslReadReady(tls_handshake_t ctx, bool ready)
{
    //check(ctx->readCipher_ready!=ready);
    if(ctx->readCipher_ready!=ready) {
        ctx->readCipher_ready=ready;
        ctx->callbacks->ready(ctx->callback_ctx, false, ready);
    }
}

static inline
void sslWriteReady(tls_handshake_t ctx, bool ready)
{
    //check(ctx->writeCipher_ready!=ready);
    if(ctx->writeCipher_ready!=ready) {
        ctx->writeCipher_ready=ready;
        ctx->callbacks->ready(ctx->callback_ctx, true, ready);
    }
}

#endif
