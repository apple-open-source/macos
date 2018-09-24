//
//  sslHandshake_priv.h
//  coretls
//

#ifndef sslHandshake_priv_h
#define sslHandshake_priv_h

#include "sslBuildFlags.h"

#ifdef __cplusplus
extern "C" {
#endif

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

int sslGetMaxProtVersion(tls_handshake_t ctx, tls_protocol_version    *version);    // RETURNED

#ifdef    NDEBUG
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

#endif /* sslHandshake_priv_h */
