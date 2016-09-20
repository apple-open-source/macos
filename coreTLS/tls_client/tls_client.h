//
//  tls_client.h
//  coretls_client
//

#ifndef __TLS_CLIENT_H__
#define __TLS_CLIENT_H__

#include <tls_handshake.h>
#include <tls_record.h>
#include <tls_stream_parser.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecTrust.h>

typedef struct {
    const char *hostname;
    const char *service; // service string or port number
    const char *config;
    bool dtls;
    int protocol_min;
    int protocol_max;
    const uint16_t *ciphersuites;
    int num_ciphersuites;
    bool allow_resumption;
    bool session_tickets_enabled;
    bool ocsp_enabled;
    uintptr_t peer_id;
    SSLCertificate certs;
    tls_private_key_t key;
    const char *alpn_string;
    const char *request;
    unsigned min_dh_size;
    bool fallback;
    bool allow_ext_master_secret;
} tls_client_params;


typedef struct {
    int sock;
    dispatch_queue_t write_queue;
    tls_record_t rec;
    tls_handshake_t hdsk;
    tls_stream_parser_t parser;
    tls_client_params *params;

    int err;
    int read_ready_received;
    int write_ready_received;
    int certificate_requested;

    SecTrustRef trustRef;

} tls_client_ctx_t;


#endif /* __TLS_CLIENT_H__ */
