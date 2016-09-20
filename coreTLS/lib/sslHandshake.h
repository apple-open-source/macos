/*
 * Copyright (c) 2000-2001,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
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
 * sslHandshake.h - SSL Handshake Layer
 */

#ifndef _SSLHANDSHAKE_H_
#define _SSLHANDSHAKE_H_

// #include "sslRecord.h"
#include "sslBuildFlags.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{   SSL_HdskHelloRequest = 0,
    SSL_HdskClientHello = 1,
    SSL_HdskServerHello = 2,
    SSL_HdskHelloVerifyRequest = 3,
    SSL_HdskNewSessionTicket = 4,
    SSL_HdskCert = 11,
    SSL_HdskServerKeyExchange = 12,
    SSL_HdskCertRequest = 13,
    SSL_HdskServerHelloDone = 14,
    SSL_HdskCertVerify = 15,
    SSL_HdskClientKeyExchange = 16,
    SSL_HdskFinished = 20,
    SSL_HdskCertificateStatus = 22,
    SSL_HdskNPNEncryptedExtension = 67
} SSLHandshakeType;

/* Hello Extensions per RFC 3546 */
typedef enum
{
	SSL_HE_ServerName = 0,
	SSL_HE_MaxFragmentLength = 1,
	SSL_HE_ClientCertificateURL = 2,
	SSL_HE_TrustedCAKeys = 3,
	SSL_HE_TruncatedHMAC = 4,
	SSL_HE_StatusReguest = 5,

	/* ECDSA, RFC 4492 */
	SSL_HE_EllipticCurves  = 10,
	SSL_HE_EC_PointFormats = 11,

    /* TLS 1.2 */
    SSL_HE_SignatureAlgorithms = 13,

    /* ALPN, RFC TBD (http://tools.ietf.org/html/draft-ietf-tls-applayerprotoneg) */
    SSL_HE_ALPN = 16,

    /* SCT, RFC 6962 */
    SSL_HE_SCT = 18,

    /* Padding, https://tools.ietf.org/html/draft-ietf-tls-padding-01 */
    SSL_HE_Padding = 21,

    /* Extended Master secret RFC 7627 */
    SSL_HE_ExtendedMasterSecret = 23,
	/*
	 * This one is suggested but not formally defined in
	 * I.D.salowey-tls-ticket-07
	 */
	SSL_HE_SessionTicket = 35,

    /* RFC 5746 */
    SSL_HE_SecureRenegotation = 0xff01,

    /*
     * NPN support for SPDY
     * WARNING: This is NOT an extension registered with the IANA
     */
    SSL_HE_NPN = 13172
} SSLHelloExtensionType;

/* SSL_HE_ServerName NameType values */
typedef enum
{
	SSL_NT_HostName = 0
} SSLServerNameType;

/* SSL_HE_StatusReguest CertificateStatusType values */
typedef enum
{
    SSL_CST_Ocsp = 1
} SSLCertificateStatusType;
/*
 * The number of curves we support
 */
#define SSL_ECDSA_NUM_CURVES	3

/* SSL_HE_EC_PointFormats - point formats */
typedef enum
{
	SSL_PointFormatUncompressed = 0,
	SSL_PointFormatCompressedPrime = 1,
	SSL_PointFormatCompressedChar2 = 2,
} SSL_ECDSA_PointFormats;

/* CurveTypes in a Server Key Exchange msg */
typedef enum
{
	SSL_CurveTypeExplicitPrime = 1,
	SSL_CurveTypeExplicitChar2 = 2,
	SSL_CurveTypeNamed         = 3		/* the only one we support */
} SSL_ECDSA_CurveTypes;

typedef enum
{   SSL_read,
    SSL_write
} CipherSide;

typedef enum
{
	SSL_HdskStateUninit = 0,			/* only valid within SSLContextAlloc */
	SSL_HdskStateServerUninit,			/* no handshake yet */
	SSL_HdskStateClientUninit,			/* no handshake yet */
	SSL_HdskStateGracefulClose,
    SSL_HdskStateErrorClose,
	SSL_HdskStateNoNotifyClose,			/* server disconnected with no
										 *   notify msg */
    /* remainder must be consecutive */
    SSL_HdskStateServerHello,           /* must get server hello; client hello sent */
    SSL_HdskStateKeyExchange,           /* must get key exchange; cipher spec
										 *   requires it */
    SSL_HdskStateCert,               	/* may get certificate or certificate
										 *   request (if no cert request received yet) */
    SSL_HdskStateHelloDone,             /* must get server hello done; after key
										 *   exchange or fixed DH parameters */
    SSL_HdskStateClientCert,         	/* must get certificate or no cert alert
										 *   from client */
    SSL_HdskStateClientKeyExchange,     /* must get client key exchange */
    SSL_HdskStateClientCertVerify,      /* must get certificate verify from client */
    SSL_HdskStateNewSessionTicket,      /* must get a NewSessionTicket message */
    SSL_HdskStateChangeCipherSpec,      /* time to change the cipher spec */
    SSL_HdskStateFinished,              /* must get a finished message in the
										 *   new cipher spec */
    SSL_HdskStateServerReady,          /* ready for I/O; server side */
    SSL_HdskStateClientReady           /* ready for I/O; client side */
} SSLHandshakeState;

typedef struct
{   SSLHandshakeType    type;
    tls_buffer           contents;
} SSLHandshakeMsg;


uint8_t *SSLEncodeHandshakeHeader(
    tls_handshake_t ctx,
    tls_buffer *rec,
    SSLHandshakeType type,
    size_t msglen);


#define SSL_Finished_Sender_Server  0x53525652
#define SSL_Finished_Sender_Client  0x434C4E54

/** sslHandshake.c **/
typedef int (*EncodeMessageFunc)(tls_buffer *rec, tls_handshake_t ctx);
int SSLProcessHandshakeRecordInner(tls_buffer rec, tls_handshake_t ctx);
int SSLProcessHandshakeRecord(tls_buffer rec, tls_handshake_t ctx);
int SSLProcessSSL2Message(tls_buffer rec, tls_handshake_t ctx);
int SSLPrepareAndQueueMessage(EncodeMessageFunc msgFunc, uint8_t contentType, tls_handshake_t ctx);
int SSLAdvanceHandshake(SSLHandshakeType processed, tls_handshake_t ctx);
int DTLSProcessHandshakeRecord(tls_buffer rec, tls_handshake_t ctx);
int DTLSRetransmit(tls_handshake_t ctx);
int SSLResetFlight(tls_handshake_t ctx);
int SSLSendFlight(tls_handshake_t ctx);

int sslGetMaxProtVersion(tls_handshake_t ctx, tls_protocol_version	*version);	// RETURNED

#ifdef	NDEBUG
#define SSLChangeHdskState(ctx, newState) { ctx->state=newState; }
#define SSLLogHdskMsg(msg, sent)
#else
void SSLChangeHdskState(tls_handshake_t ctx, SSLHandshakeState newState);
void SSLLogHdskMsg(SSLHandshakeType msg, char sent);
char *hdskStateToStr(SSLHandshakeState state);
#endif

/** sslChangeCipher.c **/
int SSLEncodeChangeCipherSpec(tls_buffer *rec, tls_handshake_t ctx);
int SSLProcessChangeCipherSpec(tls_buffer rec, tls_handshake_t ctx);

/** sslCert.c **/
int SSLFreeCertificates(SSLCertificate *certs);
int SSLFreeDNList(DNListElem *dn);
int SSLEncodeCertificate(tls_buffer *certificate, tls_handshake_t ctx);
int SSLProcessCertificate(tls_buffer message, tls_handshake_t ctx);
int SSLEncodeCertificateStatus(tls_buffer *status, tls_handshake_t ctx);
int SSLProcessCertificateStatus(tls_buffer message, tls_handshake_t ctx);
int SSLEncodeCertificateRequest(tls_buffer *request, tls_handshake_t ctx);
int SSLProcessCertificateRequest(tls_buffer message, tls_handshake_t ctx);
int SSLEncodeCertificateVerify(tls_buffer *verify, tls_handshake_t ctx);
int SSLProcessCertificateVerify(tls_buffer message, tls_handshake_t ctx);

/** sslHandshakeHello.c **/
int SSLEncodeServerHelloRequest(tls_buffer *helloDone, tls_handshake_t ctx);
int SSLEncodeServerHello(tls_buffer *serverHello, tls_handshake_t ctx);
int SSLProcessServerHello(tls_buffer message, tls_handshake_t ctx);
int SSLEncodeClientHello(tls_buffer *clientHello, tls_handshake_t ctx);
int SSLProcessClientHello(tls_buffer message, tls_handshake_t ctx);
int SSLProcessSSL2ClientHello(tls_buffer message, tls_handshake_t ctx);
int SSLProcessNewSessionTicket(tls_buffer message, tls_handshake_t ctx);

int SSLInitMessageHashes(tls_handshake_t ctx);
int SSLEncodeRandom(unsigned char *p, tls_handshake_t ctx);
#if ENABLE_DTLS
int SSLEncodeServerHelloVerifyRequest(tls_buffer *helloVerifyRequest, tls_handshake_t ctx);
int SSLProcessServerHelloVerifyRequest(tls_buffer message, tls_handshake_t ctx);
#endif

/** sslKeyExchange.c **/
int SSLEncodeServerKeyExchange(tls_buffer *keyExch, tls_handshake_t ctx);
int SSLProcessServerKeyExchange(tls_buffer message, tls_handshake_t ctx);
int SSLEncodeKeyExchange(tls_buffer *keyExchange, tls_handshake_t ctx);
int SSLProcessKeyExchange(tls_buffer keyExchange, tls_handshake_t ctx);
int SSLInitPendingCiphers(tls_handshake_t ctx);

/** sslHandshakeFinish.c **/
int SSLEncodeFinishedMessage(tls_buffer *finished, tls_handshake_t ctx);
int SSLProcessFinished(tls_buffer message, tls_handshake_t ctx);
int SSLEncodeServerHelloDone(tls_buffer *helloDone, tls_handshake_t ctx);
int SSLProcessServerHelloDone(tls_buffer message, tls_handshake_t ctx);
int SSLCalculateFinishedMessage(tls_buffer finished, tls_buffer shaMsgState, tls_buffer md5MsgState, uint32_t senderID, tls_handshake_t ctx);
int SSLEncodeNPNEncryptedExtensionMessage(tls_buffer *npn, tls_handshake_t ctx);
int SSLProcessEncryptedExtension(tls_buffer message, tls_handshake_t ctx);

#ifdef __cplusplus
}
#endif

#endif /* _SSLHANDSHAKE_H_ */
