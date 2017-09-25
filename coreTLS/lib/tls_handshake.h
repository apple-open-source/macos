//
//  tls_handshake.h
//
//  Created by Fabrice Gautier on 8/8/13.
//
//


#ifndef _TLS_HANDSHAKE_H_
#define _TLS_HANDSHAKE_H_ 1

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>

#include "tls_types.h"

/* Various types : */
/*=================*/

/* DER encoded cert chain */
typedef struct SSLCertificate
{
    struct SSLCertificate   *next;
    tls_buffer               derCert;
} SSLCertificate;

/* DER encoded list of DNs */
typedef struct DNListElem
{
    struct DNListElem   *next;
    tls_buffer		    derDN;
} DNListElem;

/* Private key object */
typedef struct _tls_private_key *tls_private_key_t;
typedef void *tls_private_key_ctx_t;

typedef void
(*tls_private_key_ctx_retain)(tls_private_key_ctx_t ctx);

typedef void
(*tls_private_key_ctx_release)(tls_private_key_ctx_t ctx);

typedef int
(*tls_private_key_rsa_sign)(tls_private_key_ctx_t ctx, tls_hash_algorithm hash, const uint8_t *plaintext, size_t plaintextLen, uint8_t *sig, size_t *sigLen);
typedef int
(*tls_private_key_rsa_decrypt)(tls_private_key_ctx_t ctx, const uint8_t *ciphertext, size_t ciphertextLen, uint8_t *plaintext, size_t *plaintextLen);
typedef int
(*tls_private_key_ecdsa_sign)(tls_private_key_ctx_t ctx, const uint8_t *plaintext, size_t plaintextLen, uint8_t *sig, size_t *sigLen);


typedef enum {
    tls_private_key_type_rsa = 0,
    tls_private_key_type_ecdsa = 1,
} tls_private_key_type_t;

typedef struct
{
    size_t size;    /* modulus size */
    tls_private_key_rsa_sign sign;
    tls_private_key_rsa_decrypt decrypt;
} tls_private_key_desc_rsa_t;

typedef struct {
    size_t size;
    uint16_t curve;
    tls_private_key_ecdsa_sign sign;
} tls_private_key_desc_ecdsa_t;

typedef struct {
    tls_private_key_type_t type;
    union {
        tls_private_key_desc_rsa_t rsa;
        tls_private_key_desc_ecdsa_t ecdsa;
    };
} tls_private_key_desc_t;

tls_private_key_t tls_private_key_create(tls_private_key_desc_t *desc, tls_private_key_ctx_t ctx,
                                         tls_private_key_ctx_release ctx_release);

/* Deprecated way to create private keys, equivalent to tls_private_key_create with ctx_release==NULL */
tls_private_key_t tls_private_key_rsa_create(tls_private_key_ctx_t ctx, size_t size, tls_private_key_rsa_sign sign,
                                             tls_private_key_rsa_decrypt decrypt);
tls_private_key_t tls_private_key_ecdsa_create(tls_private_key_ctx_t ctx, size_t size,
                                               uint16_t curve, tls_private_key_ecdsa_sign sign);

tls_private_key_ctx_t tls_private_key_get_context(tls_private_key_t key);
void tls_private_key_destroy(tls_private_key_t key);


typedef void *tls_handshake_ctx_t;

typedef struct _tls_handshake_s *tls_handshake_t;

/* Array of ciphersuites enabled by default */
extern const unsigned CipherSuiteCount;
extern const uint16_t KnownCipherSuites[];

/* Array of curves we support */
extern const unsigned CurvesCount;
extern const uint16_t KnownCurves[];

/* Array of sigalgs we support */
extern const unsigned SigAlgsCount;
extern const tls_signature_and_hash_algorithm KnownSigAlgs[];

/* handshake message types */
typedef enum {
    tls_handshake_message_client_hello_request = 0,
    tls_handshake_message_client_hello = 1,
    tls_handshake_message_server_hello = 2,
    tls_handshake_message_hello_verify_request = 3,
    tls_handshake_message_certificate = 11,
    tls_handshake_message_server_key_exchange = 12,
    tls_handshake_message_certificate_request = 13,
    tls_handshake_message_server_hello_done = 14,
    tls_handshake_message_certificate_verify = 15,
    tls_handshake_message_client_key_exchange = 16,
    tls_handshake_message_finished = 20,
    tls_handshake_message_certificate_status = 22,
    tls_handshake_message_NPN_encrypted_extension = 67,
} tls_handshake_message_t;

/* certificate evaluation trust result */
typedef enum {
    tls_handshake_trust_ok = 0,
    tls_handshake_trust_unknown = 1,        /* default value if nobody calls set_peer_trust */
    tls_handshake_trust_unknown_root = 2,
    tls_handshake_trust_cert_expired = 3,
    tls_handshake_trust_cert_invalid = 4,
} tls_handshake_trust_t;

/* alert message levels */
typedef enum {
    tls_handshake_alert_level_warning = 1,
    tls_handshake_alert_level_fatal = 2,
} tls_alert_level_t;

/* alert message descriptions */
typedef enum {
    tls_handshake_alert_CloseNotify = 0,
    tls_handshake_alert_UnexpectedMsg = 10,
    tls_handshake_alert_BadRecordMac = 20,
    tls_handshake_alert_DecryptionFail_RESERVED = 21,  /* TLS */
    tls_handshake_alert_RecordOverflow = 22,           /* TLS */
    tls_handshake_alert_DecompressFail = 30,
    tls_handshake_alert_HandshakeFail = 40,
    tls_handshake_alert_NoCert_RESERVED = 41,
    tls_handshake_alert_BadCert = 42,                  /* SSLv3 only */
    tls_handshake_alert_UnsupportedCert = 43,
    tls_handshake_alert_CertRevoked = 44,
    tls_handshake_alert_CertExpired = 45,
    tls_handshake_alert_CertUnknown = 46,
    tls_handshake_alert_IllegalParam = 47,
    tls_handshake_alert_UnknownCA = 48,
    tls_handshake_alert_AccessDenied = 49,
    tls_handshake_alert_DecodeError = 50,
    tls_handshake_alert_DecryptError = 51,
    tls_handshake_alert_ExportRestriction_RESERVED = 60,
    tls_handshake_alert_ProtocolVersion = 70,
    tls_handshake_alert_InsufficientSecurity = 71,
    tls_handshake_alert_InternalError = 80,
    tls_handshake_alert_InappropriateFallback = 86,    /* RFC 7507 */
    tls_handshake_alert_UserCancelled = 90,
    tls_handshake_alert_NoRenegotiation = 100,
    tls_handshake_alert_UnsupportedExtension = 110,    /* TLS 1.2 */
} tls_alert_t;

/* common configurations */
typedef enum {
    /* No configuration - returned when custom ciphers or versions are set. */
    tls_handshake_config_none = -1,
    /* Default configuration - currently same as legacy */
    tls_handshake_config_default = 0,
    /* TLS v1.2 to SSLv3, with default + RC4 ciphersuites ciphersuites */
    tls_handshake_config_legacy = 1,
    /* TLS v1.2 to TLS v1.0, with default ciphersuites (no 3DES) */
    tls_handshake_config_standard = 2,
    /* TLS v1.2 to TLS v1.0, with defaults ciphersuites + RC4 */
    tls_handshake_config_RC4_fallback = 3,
    /* TLS v1.0, with defaults ciphersuites + fallback SCSV */
    tls_handshake_config_TLSv1_fallback = 4,
    /* TLS v1.0, with defaults ciphersuites + RC4 + fallback SCSV */
    tls_handshake_config_TLSv1_RC4_fallback = 5,
    /* TLS v1.2, only PFS ciphersuites */
    tls_handshake_config_ATSv1 = 6,
    /* TLS v1.2, include non PFS ciphersuites */
    tls_handshake_config_ATSv1_noPFS = 7,
    /* TLS v1.2 to SSLv3, defaults + RC4 + DHE ciphersuites */
    tls_handshake_config_legacy_DHE = 8,
    /* TLS v1.2 only, anonymous ciphersuites only, no RC4 or 3DES */
    tls_handshake_config_anonymous = 9,
    /* TLS v1.2 to TLS v1.0, with defaults ciphersuites + 3DES */
    tls_handshake_config_3DES_fallback = 10,
    /* TLS v1.0, with defaults ciphersuites + 3DES */
    tls_handshake_config_TLSv1_3DES_fallback = 11,
    /* TLS v1.3 to TLS v1.0, with default ciphersuites (no 3DES) */
    tls_handshake_config_standard_TLSv3 = 12,
    /* TLS v1.3, and TLS v1.2 with only PFS ciphersuites */
    tls_handshake_config_ATSv2 = 13,
} tls_handshake_config_t;


typedef struct tls_message {
    struct tls_message *next;
    tls_buffer          data;
} tls_message;


/* Callbacks types: */
/*==================*/

typedef int
(*tls_handshake_write_callback_t) (tls_handshake_ctx_t ctx, const tls_buffer data, uint8_t content_type);


typedef int
(*tls_handshake_message_callback_t) (tls_handshake_ctx_t ctx, tls_handshake_message_t message);

typedef void
(*tls_handshake_ready_callback_t) (tls_handshake_ctx_t ctx, bool write, bool ready);

typedef int
(*tls_handshake_set_retransmit_timer_callback_t) (tls_handshake_ctx_t ctx, int attempt);

/* Session layer */
typedef int
(*tls_handshake_save_session_data_callback_t) (tls_handshake_ctx_t ctx, tls_buffer sessionKey, tls_buffer sessionData);

typedef int
(*tls_handshake_load_session_data_callback_t) (tls_handshake_ctx_t ctx, tls_buffer sessionKey, tls_buffer *sessionData);

typedef int
(*tls_handshake_delete_session_data_callback_t) (tls_handshake_ctx_t ctx, tls_buffer sessionKey);

typedef int
(*tls_handshake_delete_all_sessions_callback_t) (tls_handshake_ctx_t ctx);

/* Record layer control */
typedef int
(*tls_handshake_init_pending_cipher_callback_t) (tls_handshake_ctx_t ctx, uint16_t selectedCipher, bool server, tls_buffer key);

typedef int
(*tls_handshake_advance_write_cipher_callback_t) (tls_handshake_ctx_t ctx);

typedef int
(*tls_handshake_rollback_write_cipher_callback_t) (tls_handshake_ctx_t ctx);

typedef int
(*tls_handshake_advance_read_cipher_callback_t) (tls_handshake_ctx_t ctx);

typedef int
(*tls_handshake_set_protocol_version_callback_t) (tls_handshake_ctx_t ctx, tls_protocol_version protocolVersion);

typedef int
(*tls_handshake_set_record_splitting_callback_t) (tls_handshake_ctx_t ctx, bool enable);

typedef struct {
    tls_handshake_write_callback_t write;
    tls_handshake_message_callback_t message;
    tls_handshake_ready_callback_t ready;
    tls_handshake_set_retransmit_timer_callback_t set_retransmit_timer;
    tls_handshake_save_session_data_callback_t save_session_data;
    tls_handshake_load_session_data_callback_t load_session_data;
    tls_handshake_delete_session_data_callback_t delete_session_data;
    tls_handshake_delete_all_sessions_callback_t delete_all_sessions;
    tls_handshake_init_pending_cipher_callback_t init_pending_cipher;
    tls_handshake_advance_write_cipher_callback_t advance_write_cipher;
    tls_handshake_rollback_write_cipher_callback_t rollback_write_cipher;
    tls_handshake_advance_read_cipher_callback_t advance_read_cipher;
    tls_handshake_set_protocol_version_callback_t set_protocol_version;
    tls_handshake_set_record_splitting_callback_t set_record_splitting;
} tls_handshake_callbacks_t;

/* Create & Destroy : */
/*====================*/

tls_handshake_t
tls_handshake_create(bool dtls, bool server);

void
tls_handshake_destroy(tls_handshake_t ctx);


/* Setup : */
/*=========*/
int
tls_handshake_set_callbacks(tls_handshake_t filter,
                                   tls_handshake_callbacks_t *callbacks,
                                   tls_handshake_ctx_t ctx);

/* Operations : */
/*==============*/

/* Process Incoming message */
int
tls_handshake_process(tls_handshake_t filter,
                             const tls_buffer message, uint8_t content_type);

/* Continue a handshake that was paused by a callback */
int
tls_handshake_continue(tls_handshake_t filter);

/* Trigger first or subsequent handshake - client only */
int
tls_handshake_negotiate(tls_handshake_t filter, tls_buffer *peerID);

/* Request renegotiation - server only */
int
tls_handshake_request_renegotiation(tls_handshake_t filter);

/* Close the session */
int
tls_handshake_close(tls_handshake_t filter);

/* Retransmit timer expired - DTLS only */
int
tls_handshake_retransmit_timer_expired(tls_handshake_t filter);

/* Send a TLS alert message */
int
tls_handshake_send_alert(tls_handshake_t filter, tls_alert_level_t level, tls_alert_t description);

/* Set/Get options and parameters : */
/*==============================*/

/* Allow session to be cached for later resumption */
int
tls_handshake_set_resumption(tls_handshake_t filter, bool allow);

/* Enabled Session Tickets support (RFC 5077) */
int
tls_handshake_set_session_ticket_enabled(tls_handshake_t filter, bool enabled);

/* Allow session renegotiation */
int
tls_handshake_set_renegotiation(tls_handshake_t filter, bool allow);

/* Set enabled ciphersuites */
/* Unsupported ciphersuites will be filtered out */
int
tls_handshake_set_ciphersuites(tls_handshake_t filter, const uint16_t *ciphersuite, unsigned n);

/* Get enabled ciphersuites */
/* This will not return any unsupported ciphersuites,
   the returned ciphersuites pointer becomes invalid when the handshake object is free or
   when tls_handshake_set_ciphersuites is called.
 */
int
tls_handshake_get_ciphersuites(tls_handshake_t filter, const uint16_t **ciphersuites, unsigned *n);

/* Set/Get minimal enabled TLS version */
int
tls_handshake_set_min_protocol_version(tls_handshake_t filter, tls_protocol_version min);
int
tls_handshake_get_min_protocol_version(tls_handshake_t filter, tls_protocol_version *min);

/* Set/Get maximal enabled TLS version */
int
tls_handshake_set_max_protocol_version(tls_handshake_t filter, tls_protocol_version max);
int
tls_handshake_get_max_protocol_version(tls_handshake_t filter, tls_protocol_version *max);

/* Set the enabled EC Curves */
int
tls_handshake_set_curves(tls_handshake_t filter, const uint16_t *curves, unsigned n);

/* Set the transport MTU - DTLS only */
int
tls_handshake_set_mtu(tls_handshake_t filter, size_t mtu);

/* Set/get minimum group size for DHE ciphersuites - Client only */
/* bits should be between 512 and 2048, other values will be adjusted accordingly */
int
tls_handshake_set_min_dh_group_size(tls_handshake_t filter, unsigned nbits);
int
tls_handshake_get_min_dh_group_size(tls_handshake_t filter, unsigned *nbits);

/* Set DH parameters - Server only */
int
tls_handshake_set_dh_parameters(tls_handshake_t filter, tls_buffer *params);

/* Set the local identity (cert chain and private key) */
int
tls_handshake_set_identity(tls_handshake_t filter, SSLCertificate *certs, tls_private_key_t key);

/* Set the encryption public key - this is only used to force the server into unhealthy behavior, for test purpose */
int
tls_handshake_set_encrypt_rsa_public_key(tls_handshake_t filter, const tls_buffer *modulus, const tls_buffer *exponent);

/* Set the PSK identity - Client only */
int
tls_handshake_set_psk_identity(tls_handshake_t filter, tls_buffer *psk_identity);

/* Set the PSK identity hint - Server only */
int
tls_handshake_set_psk_identity_hint(tls_handshake_t filter, tls_buffer *psk_identity_hint);

/* Set the PSK secret for PSK cipher suites */
int
tls_handshake_set_psk_secret(tls_handshake_t filter, tls_buffer *psk_secret);

/* Set client side auth type - Client only - DEPRECATE */
int
tls_handshake_set_client_auth_type(tls_handshake_t filter, tls_client_auth_type auth_type);

/* Set peer hostname - Client only */
int
tls_handshake_set_peer_hostname(tls_handshake_t filter, const char *hostname, size_t len);
int
tls_handshake_get_peer_hostname(tls_handshake_t filter, const char **hostname, size_t *len);

/* Request client auth - Server Only */
int
tls_handshake_set_client_auth(tls_handshake_t filter, bool request);

/* Set/Get acceptable CA for client side auth - Server only */
int
tls_handshake_set_acceptable_dn_list(tls_handshake_t filter, DNListElem *);
int
tls_handshake_get_acceptable_dn_list(tls_handshake_t filter, DNListElem **);

/* Set acceptable sig algs */
int
tls_handshake_set_sigalgs(tls_handshake_t filter, const tls_signature_and_hash_algorithm *sigalgs, unsigned n);

/* Set acceptable type for client side auth - Server only */
/* FIXME: auth_types should be const, but internally we use the same field for the server side, which is not const */
int
tls_handshake_set_acceptable_client_auth_type(tls_handshake_t filter, tls_client_auth_type *auth_types, unsigned n);

/* Set the peer public key data, called by the client upon processing the peer cert */
int
tls_handshake_set_peer_rsa_public_key(tls_handshake_t filter, const tls_buffer *modulus, const tls_buffer *exponent);

/* Set the peer public key data, called by the client upon processing the peer cert */
int
tls_handshake_set_peer_ec_public_key(tls_handshake_t filter, tls_named_curve namedCurve, const tls_buffer *pubKeyBits);

/* Set the result of peer certificate evaluation.
   For the Client side, this needs to be called after receiving the ServerHelloDone message at the latest.
   For the Server side, this needs to be called after receiving the Finished message at the latest.
   Behavior when there is no certificate message involved in the handshake is undefined. This includes
   the case when the selected ciphersuite does not require a certificate, and the case of session resumption.
*/
int
tls_handshake_set_peer_trust(tls_handshake_t filter, tls_handshake_trust_t trust);

/* Enable false start - Client only */
int
tls_handshake_set_false_start(tls_handshake_t filter, bool enabled);
int
tls_handshake_get_false_start(tls_handshake_t filter, bool *enabled);

/* Client only - Enable NPN */
int
tls_handshake_set_npn_enable(tls_handshake_t filter, bool enabled);

/* Server: Set supported Application Protocols */
/* Client: Set selected Application protocol */
int
tls_handshake_set_npn_data(tls_handshake_t filter, tls_buffer npn_data);

/* Client: Set supported Application Protocols */
/* Server: Set selected Application protocol */
int
tls_handshake_set_alpn_data(tls_handshake_t filter, tls_buffer alpn_data);

/* Client: set allowing server to change identity */
int
tls_handshake_set_server_identity_change(tls_handshake_t filter, bool allowed);
int
tls_handshake_get_server_identity_change(tls_handshake_t filter, bool *allowed);

/* Client/Server: enable ocsp stapling */
int
tls_handshake_set_ocsp_enable(tls_handshake_t filter, bool enabled);

/* Client: set ocsp responder_id_list */
int
tls_handshake_set_ocsp_responder_id_list(tls_handshake_t filter, tls_buffer_list_t *ocsp_responder_id_list);

/* Client: set ocsp request_extensions */
int
tls_handshake_set_ocsp_request_extensions(tls_handshake_t filter, tls_buffer ocsp_request_extensions);

/* Server: set ocsp response data */
int
tls_handshake_set_ocsp_response(tls_handshake_t filter, tls_buffer *ocsp_response);

/* Client: enable SCT extension */
int
tls_handshake_set_sct_enable(tls_handshake_t filter, bool enabled);

/* Server: set SCT list */
int
tls_handshake_set_sct_list(tls_handshake_t filter, tls_buffer_list_t *sct_list);

/* Client only: indicate this is a fallback attempt, apply proper countermeasure */
int
tls_handshake_set_fallback(tls_handshake_t filter, bool enabled);

/* Client only: get the fallback state */
int
tls_handshake_get_fallback(tls_handshake_t filter, bool *enabled);

/* Set TLS user agent string, for diagnostic purposes */
int
tls_handshake_set_user_agent(tls_handshake_t filter, const char *user_agent);

/* Set TLS config */
int
tls_handshake_set_config(tls_handshake_t filter, tls_handshake_config_t config);

int
tls_handshake_get_config(tls_handshake_t filter, tls_handshake_config_t *config);

int
tls_handshake_set_ems_enable(tls_handshake_t filter, bool enabled);

/* Get session attributes : */
/*==========================*/


/* Established session attributes */
/*--------------------------------*/

/* The following are attributes of an established session
   and are available after getting the read ready callback */

const uint8_t *
tls_handshake_get_server_random(tls_handshake_t filter);

const uint8_t *
tls_handshake_get_client_random(tls_handshake_t filter);

/* Return true if the client sent a session ID,
   and return the sessionID sent */
bool
tls_handshake_get_session_proposed(tls_handshake_t filter, tls_buffer *sessionID);

/* Return true if the server resumed the session, 
   and return the resumed or new sessionID */
bool
tls_handshake_get_session_match(tls_handshake_t filter, tls_buffer *sessionID);

const uint8_t *
tls_handshake_get_master_secret(tls_handshake_t filter);

bool
tls_handshake_get_negotiated_ems(tls_handshake_t filter);

/* Negotiation attributes */
/*------------------------*/

/* The following are session attribute that are available during the handshake
   and maybe required to continue the handshake. They are available after a
   certain protocol message is processed. For example peer_acceptable_dn_list
   maybe necessary to select the proper client cert, and is available after the
   certificate_request message is processed */

/* Available after receiving the client_hello or server_hello message: */
tls_protocol_version
tls_handshake_get_negotiated_protocol_version(tls_handshake_t filter);

/* Available after sending or receiving server_hello message */
uint16_t
tls_handshake_get_negotiated_cipherspec(tls_handshake_t filter);

/* Available after sending or receiving server_hello message */
uint16_t
tls_handshake_get_negotiated_curve(tls_handshake_t filter);

/* Server only:  get the SNI hostname if provided by client - available after receiving the client_hello message */
const tls_buffer *
tls_handshake_get_sni_hostname(tls_handshake_t filter);

/* Available after receiving the certificate message */
const SSLCertificate *
tls_handshake_get_peer_certificates(tls_handshake_t filter);

/* Client only - available after receiving the certificate_request message */
const DNListElem *
tls_handshake_get_peer_acceptable_dn_list(tls_handshake_t filter);

/* Client only - available after receiving the certificate_request message */
const tls_client_auth_type *
tls_handshake_get_peer_acceptable_client_auth_type(tls_handshake_t filter, unsigned *num);

/* Server only : available after receiving the client_hello message */
const uint16_t *
tls_handshake_get_peer_requested_ciphersuites(tls_handshake_t filter, unsigned *num);

/* Server: available after receiving the client_hello message
   Client: available after receiving the certificate_request message */
const tls_signature_and_hash_algorithm *
tls_handshake_get_peer_signature_algorithms(tls_handshake_t filter, unsigned *num);

/* Client only - available after receiving the server_key_exchange message */
const tls_buffer *
tls_handshake_get_peer_psk_identity_hint(tls_handshake_t filter);

/* Server only - available after receiving the client_key_exchange message */
const tls_buffer *
tls_handshake_get_peer_psk_identity(tls_handshake_t filter);

/* Server only - available after receiving the client_hello message */
bool
tls_handshake_get_peer_npn_enabled(tls_handshake_t filter);

/* Server: available after receiving the NPN_encrypted_extension message
   Client: available after receiving the server_hello message */
const tls_buffer *
tls_handshake_get_peer_npn_data(tls_handshake_t filter);

/* Client: available after receiving the server_hello message
   Server: available after receiving the client_hello message */
const tls_buffer *
tls_handshake_get_peer_alpn_data(tls_handshake_t filter);

/* Client: available after receiving the server_hello message
 Server: available after receiving the client_hello message */
bool
tls_handshake_get_peer_ocsp_enabled(tls_handshake_t filter);

/* Client: available after receiving the certificate_status message */
const tls_buffer *
tls_handshake_get_peer_ocsp_response(tls_handshake_t filter);

/* Server: available after receiving the client hello message */
const tls_buffer_list_t *
tls_handshake_get_peer_ocsp_responder_id_list(tls_handshake_t filter);

/* Server: available after receiving the client hello message */
const tls_buffer *
tls_handshake_get_peer_ocsp_request_extensions(tls_handshake_t filter);

/* Server: available after receiving the client_hello message */
bool
tls_handshake_get_peer_sct_enabled(tls_handshake_t filter);

/* Client: available after receiving the server_hello message */
const tls_buffer_list_t *
tls_handshake_get_peer_sct_list(tls_handshake_t filter);

/* Server only : available after receiving the client_hello message */
const uint16_t *
tls_handshake_get_peer_requested_ecdh_curves(tls_handshake_t filter, unsigned *num);


/* Special functions */
/*====================*/

/* TLS Internal PRF - not valid for SSL 3 connections */
int tls_handshake_internal_prf(tls_handshake_t ctx,
                               const void *vsecret,
                               size_t secretLen,
                               const void *label,		// optional, NULL implies that seed contains the label
                               size_t labelLen,
                               const void *seed,
                               size_t seedLen,
                               void *vout,              // mallocd by caller, length >= outLen
                               size_t outLen);



/*
 * Callback function for EAP-style PAC-based session resumption.
 * This function is called by coreTLS to obtain the
 * master secret.
 */
typedef void (*tls_handshake_master_secret_function_t)(const void *arg,         /* opaque to coreTLS; app-specific */
                                                       void *secret,			/* mallocd by caller, SSL_MASTER_SECRET_SIZE */
                                                       size_t *secretLength);   /* in/out */

int
tls_handshake_internal_set_master_secret_function(tls_handshake_t ctx, tls_handshake_master_secret_function_t mFunc, const void *arg);

int
tls_handshake_internal_set_session_ticket(tls_handshake_t ctx, const void *ticket, size_t ticketLength);

int
tls_handshake_internal_master_secret(tls_handshake_t ctx,
                                     void *secret,          // mallocd by caller, SSL_MASTER_SECRET_SIZE
                                     size_t *secretSize);   // in/out

int
tls_handshake_internal_server_random(tls_handshake_t ctx,
                                     void *randBuf, 		// mallocd by caller, SSL_CLIENT_SRVR_RAND_SIZE
                                     size_t *randSize);     // in/out

int
tls_handshake_internal_client_random(tls_handshake_t ctx,
                                     void *randBuf,  	// mallocd by caller, SSL_CLIENT_SRVR_RAND_SIZE
                                     size_t *randSize);	// in/out

#endif
