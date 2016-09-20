//
//  tls_server.h
//  coretls_server
//

#ifndef __TLS_SERVER_H__
#define __TLS_SERVER_H__

#include <tls_handshake.h>
#include <tls_record.h>
#include <tls_stream_parser.h>

#include <CoreFoundation/CoreFoundation.h>


typedef struct {
    const char *hostname;
    int port;
    const char *config;
    int protocol_min;
    int protocol_max;
    const uint16_t *ciphersuites;
    int num_ciphersuites;
    bool allow_resumption;
    bool allow_renegotiation;
    uintptr_t session_id;
    SSLCertificate cert1;
    SSLCertificate cert2;
    tls_private_key_t key1;
    tls_private_key_t key2;
    tls_buffer *ocsp;
    tls_buffer *dh_parameters;
    const char *alpn_string;
    const char *request;
    bool rsa_server_key_exchange;
    bool client_auth;
    bool use_kext;
    bool dtls;
    bool verbose;
    bool allow_ext_master_secret;
} tls_server_params;


typedef struct {
    int sock;
    struct sockaddr_in address; //for udp only.

    dispatch_queue_t write_queue;
    tls_record_t rec;
    tls_handshake_t hdsk;
    tls_stream_parser_t parser;
    tls_server_params *params;

    int err;
    int read_ready_received;
    int write_ready_received;
    int certificate_requested;
    bool renegotiation_requested;
    bool response_sent;
} tls_server_ctx_t;


#endif /* __TLS_SERVER_H__ */
