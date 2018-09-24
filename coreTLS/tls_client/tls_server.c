//
//  tls_server.c
//  coretls_server
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <AssertMacros.h>


/* extern struct ccrng_state *ccDRBGGetRngState(); */
#include <CommonCrypto/CommonRandomSPI.h>
#define CCRNGSTATE() ccDRBGGetRngState()

#include <tls_handshake.h>
#include <tls_ciphersuites.h>
#include <tls_helpers.h>
#include <tls_cache.h>

#include <Security/CipherSuite.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecIdentity.h>

#include "appleSession.h"
#include "secCrypto.h"
#include "tls_server.h"
#include "tls_alloc.h"
#include "sockets.h"

static
__attribute__((format(printf, 3, 4)))
void _log(const tls_server_ctx_t *sc, const char *function, const char *str, ...)
{
    va_list ap;

    va_start(ap, str);

    if(sc) {
        printf("[%p] ", sc);
    }
    printf("%s: ", function);
    vprintf(str, ap);
}

#define session_log(...) _log(sc, __FUNCTION__, __VA_ARGS__);
#define server_log(...) _log(NULL, __FUNCTION__, __VA_ARGS__);

#define DEBUG_ONLY __attribute__((unused))

#define TRIPLE_HANDSHAKE_TEST 0



typedef struct _CipherSuiteName {
    uint16_t cipher;
    const char *name;
} CipherSuiteName;

#define CIPHER(cipher) {cipher, #cipher}

const CipherSuiteName ciphers[] = {
    //SSL_NULL_WITH_NULL_NULL, unsupported
#if 1
    /* RSA cipher suites */
    CIPHER(SSL_RSA_WITH_NULL_MD5),
    CIPHER(SSL_RSA_WITH_NULL_SHA),
    CIPHER(TLS_RSA_WITH_NULL_SHA256),
#endif

#if 1
    CIPHER(SSL_RSA_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_RSA_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_RSA_WITH_AES_128_CBC_SHA256),
    CIPHER(TLS_RSA_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_RSA_WITH_AES_256_CBC_SHA256),
#endif

#if 1
    /* DHE_RSA ciphers suites */
    CIPHER(SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_DHE_RSA_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_DHE_RSA_WITH_AES_128_CBC_SHA256),
    CIPHER(TLS_DHE_RSA_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_DHE_RSA_WITH_AES_256_CBC_SHA256),
#endif


#if 1
    /* DH_anon cipher suites */
    CIPHER(SSL_DH_anon_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_DH_anon_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_DH_anon_WITH_AES_128_CBC_SHA256),
    CIPHER(TLS_DH_anon_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_DH_anon_WITH_AES_256_CBC_SHA256),
#endif

#if 1
    /* ECDHE_ECDSA cipher suites */
    CIPHER(TLS_ECDHE_ECDSA_WITH_NULL_SHA),
    CIPHER(TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256),
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384),
#endif

#if 1
    /* ECDHE_RSA cipher suites */
    CIPHER(TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256),
    CIPHER(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384), // Not supported by either gnutls or openssl
#endif

#if 1
    CIPHER(TLS_PSK_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_PSK_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_PSK_WITH_AES_256_CBC_SHA),
    CIPHER(TLS_PSK_WITH_AES_128_CBC_SHA256),
    CIPHER(TLS_PSK_WITH_AES_256_CBC_SHA384),
    CIPHER(TLS_PSK_WITH_NULL_SHA),
    CIPHER(TLS_PSK_WITH_NULL_SHA256),
    CIPHER(TLS_PSK_WITH_NULL_SHA384),
#endif

#if 1
    CIPHER(TLS_RSA_WITH_AES_128_GCM_SHA256),
    CIPHER(TLS_RSA_WITH_AES_256_GCM_SHA384),

    CIPHER(TLS_DHE_RSA_WITH_AES_128_GCM_SHA256),
    CIPHER(TLS_DHE_RSA_WITH_AES_256_GCM_SHA384),

    CIPHER(TLS_DH_anon_WITH_AES_128_GCM_SHA256),
    CIPHER(TLS_DH_anon_WITH_AES_256_GCM_SHA384),

    CIPHER(TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256),
    CIPHER(TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384),

    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256),
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384),

#endif

    { -1, NULL, }
};


static uint16_t sslcipher_atoi(char* cipherstring){
    const CipherSuiteName *a = ciphers;
    while (a->name) {
        if (strcmp(cipherstring, a->name) == 0) break;
        a++;
    }
    return a->cipher;
}

static const char *sslcipher_itoa(uint16_t cs)
{
    const CipherSuiteName *a = ciphers;
    while(a->name) {
        if (cs == a->cipher) break;
        a++;
    }
    return a->name;
}

static int tcp_write(tls_server_ctx_t *sc, tls_buffer out)
{
    while(out.length) {
        ssize_t nwr;
        nwr = send(sc->sock, out.data, out.length, 0);
        if (nwr == -1) {
            session_log("Error writing %zd bytes to socket : %s\n", out.length, strerror(errno));
            return errno;
        }
        out.data += nwr;
        out.length -= nwr;
    }
    return 0;
}

static int udp_write(tls_server_ctx_t *sc, tls_buffer out)
{
    ssize_t nwr;
    nwr = sendto(sc->sock, out.data, out.length, 0, (struct sockaddr *)&(sc->address), sizeof(sc->address));
    if (nwr == -1) {
        session_log("Error writing %zd bytes to socket : %s\n", out.length, strerror(errno));
        return errno;
    }
    return 0;
}


static
int encrypt_and_write(tls_server_ctx_t *sc, const tls_buffer data, uint8_t content_type)
{
    int err;
    tls_buffer encrypted = {0, };

    err=mySSLAlloc(&encrypted, tls_record_encrypted_size(sc->rec, content_type, data.length));
    require_noerr(err, fail);

    err=tls_record_encrypt(sc->rec, data, content_type, &encrypted);
    require_noerr(err, fail);

    session_log("Writing %5zd encrypted bytes\n", encrypted.length);

    if(sc->params->dtls) {
        err = udp_write(sc, encrypted);
    } else {
        err = tcp_write(sc, encrypted);
    }

fail:
    mySSLFree(&encrypted);
    return err;
}

static
int tls_handshake_write_callback(tls_handshake_ctx_t ctx, const tls_buffer data, uint8_t content_type)
{
    tls_server_ctx_t *sc = (tls_server_ctx_t *)ctx;
    session_log("Type = %d, Record len = %5zd\n", content_type, data.length);
    return encrypt_and_write(sc, data, content_type);
}


static int
tls_handshake_message_callback(tls_handshake_ctx_t ctx, tls_handshake_message_t event)
{
    tls_server_ctx_t *sc = (tls_server_ctx_t *)ctx;
    int err = 0;
    const tls_buffer *sni;
    SecTrustRef trustRef = NULL;

    session_log("event = %d\n", event);

    switch(event) {
        case tls_handshake_message_client_hello:
            sni = tls_handshake_get_sni_hostname(sc->hdsk);
            if(sni)
                session_log("Received SNI: %s\n", sni->data);
            break;
        case tls_handshake_message_certificate:
            require_noerr((err = tls_handshake_set_peer_trust(sc->hdsk, tls_handshake_trust_ok)), fail);
            require_noerr((err = tls_helper_set_peer_pubkey(sc->hdsk)), fail);
            break;
        default:
            break;
    }
fail:
    CFReleaseSafe(trustRef);
    return err;
}

static void
tls_handshake_ready_callback(tls_handshake_ctx_t ctx, bool write, bool ready)
{
    tls_server_ctx_t *sc = (tls_server_ctx_t *)ctx;

    session_log("%s ready=%d\n", write?"write":"read", ready);

    if(ready) {
        if(write) {
            sc->write_ready_received++;
        } else {
            uint16_t cs = tls_handshake_get_negotiated_cipherspec(sc->hdsk);
            session_log("Negotiated ciphersuite: %s (%04x)\n", sslcipher_itoa(cs), cs);
            sc->read_ready_received++;
        }
    }
}

static int
tls_handshake_set_retransmit_timer_callback(tls_handshake_ctx_t ctx, int attempt)
{
    tls_server_ctx_t DEBUG_ONLY *sc = (tls_server_ctx_t *)ctx;

    session_log("attempt=%d\n", attempt);

    return ENOTSUP;
}

static
int mySSLRecordInitPendingCiphersFunc(tls_handshake_ctx_t ref,
                                      uint16_t            selectedCipher,
                                      bool                server,
                                      tls_buffer           key)
{
    tls_server_ctx_t *sc = (tls_server_ctx_t *)ref;
    session_log("cipher=%04x, server=%d\n", selectedCipher, server);
    return tls_record_init_pending_ciphers(sc->rec, selectedCipher, server, key);
}

static
int mySSLRecordAdvanceWriteCipherFunc(tls_handshake_ctx_t ref)
{
    tls_server_ctx_t *sc = (tls_server_ctx_t *)ref;
    session_log("\n");
    return tls_record_advance_write_cipher(sc->rec);
}

static
int mySSLRecordRollbackWriteCipherFunc(tls_handshake_ctx_t ref)
{
    tls_server_ctx_t *sc = (tls_server_ctx_t *)ref;
    session_log("\n");
    return tls_record_rollback_write_cipher(sc->rec);
}

static
int mySSLRecordAdvanceReadCipherFunc(tls_handshake_ctx_t ref)
{
    tls_server_ctx_t *sc = (tls_server_ctx_t *)ref;
    session_log("\n");
    return tls_record_advance_read_cipher(sc->rec);
}

static
int mySSLRecordSetProtocolVersionFunc(tls_handshake_ctx_t ref,
                                      tls_protocol_version  protocolVersion)
{
    tls_server_ctx_t *sc = (tls_server_ctx_t *)ref;
    session_log("pv=%04x\n", protocolVersion);
    return tls_record_set_protocol_version(sc->rec, protocolVersion);
}

static tls_cache_t g_cache = NULL;

static int
tls_handshake_save_session_data_callback(tls_handshake_ctx_t ctx, tls_buffer sessionKey, tls_buffer sessionData)
{
    tls_server_ctx_t *sc = (tls_server_ctx_t *)ctx;
    session_log("key = %016llx... data=[%p,%zd]\n", *(uint64_t *)sessionKey.data, sessionData.data, sessionData.length);
    return tls_cache_save_session_data(g_cache, &sessionKey, &sessionData, 0);
}

static int
tls_handshake_load_session_data_callback(tls_handshake_ctx_t ctx, tls_buffer sessionKey, tls_buffer *sessionData)
{
    tls_server_ctx_t *sc = (tls_server_ctx_t *)ctx;
    int err = tls_cache_load_session_data(g_cache,&sessionKey, sessionData);
    session_log("key = %s data=[%p,%zd], err=%d\n", sessionKey.data, sessionData->data, sessionData->length, err);
    return err;
}

static int
tls_handshake_delete_session_data_callback(tls_handshake_ctx_t ctx, tls_buffer sessionKey)
{
    tls_server_ctx_t *sc = (tls_server_ctx_t *)ctx;
    session_log("\n");
    return tls_cache_delete_session_data(g_cache,&sessionKey);
}

static int
tls_handshake_delete_all_sessions_callback(tls_handshake_ctx_t ctx)
{
    tls_server_ctx_t *sc = (tls_server_ctx_t *)ctx;
    session_log("\n");
    return -1;
}

/* TLS callbacks */
tls_handshake_callbacks_t tls_handshake_callbacks = {
    .write = tls_handshake_write_callback,
    .message = tls_handshake_message_callback,
    .ready = tls_handshake_ready_callback,
    .set_retransmit_timer = tls_handshake_set_retransmit_timer_callback,
    .init_pending_cipher = mySSLRecordInitPendingCiphersFunc,
    .advance_write_cipher = mySSLRecordAdvanceWriteCipherFunc,
    .rollback_write_cipher = mySSLRecordRollbackWriteCipherFunc,
    .advance_read_cipher = mySSLRecordAdvanceReadCipherFunc,
    .set_protocol_version = mySSLRecordSetProtocolVersionFunc,
    .load_session_data = tls_handshake_load_session_data_callback,
    .save_session_data = tls_handshake_save_session_data_callback,
    .delete_session_data = tls_handshake_delete_session_data_callback,
    .delete_all_sessions = tls_handshake_delete_all_sessions_callback,
};


static
int tls_record_process(tls_server_ctx_t *sc, tls_buffer record)
{
    int err = errSecAllocate;
    tls_buffer out;
    uint8_t content_type;

    session_log("len = %zu\n", record.length);

    size_t dlen = tls_record_decrypted_size(sc->rec, record.length);

    mySSLAlloc(&out, dlen+1); // 1 extra byte for \0
    require(out.data, fail);

    require_noerr((err=tls_record_decrypt(sc->rec, record, &out, &content_type)), fail);

    if(content_type!=tls_record_type_AppData) {
        session_log("processing protocol message of type %d, len=%zu\n", content_type, out.length);
        require_noerr_quiet((err = tls_handshake_process(sc->hdsk, out, content_type)), fail);
    } else {
        if(sc->read_ready_received<0)
            session_log("Received data before read_ready\n");
        session_log("received data record, len = %zu\n", out.length);
        out.data[out.length]=0;
        printf("*** DATA ***\n%s\n*** END DATA ***\n", out.data);

#if TRIPLE_HANDSHAKE_TEST
        if(!sc->renegotiation_requested) {
            sc->renegotiation_requested = true;
            session_log("Changing Server identity\n");
            err=tls_handshake_set_identity(sc->hdsk, &sc->params->cert2, sc->params->key2);
            if(err) {
                session_log("Changing Server identity failed, err=%d\n", err);
            }
            err = tls_handshake_request_renegotiation(sc->hdsk);
            session_log("Renegotiation requested, err= %d\n", err);
        }
#else
        // Echo the data
        require_noerr((err=tls_handshake_callbacks.write(sc, out, tls_record_type_AppData)), fail);
#endif
    }

#if TRIPLE_HANDSHAKE_TEST
    // Send data after the second handshake
    if(sc->write_ready_received==2 && !sc->response_sent) {

        char *content = "<HTML><BODY>This should not work</BODY></HTML>";
        char response_data[2048];
        size_t response_len;
        response_len = snprintf(response_data, sizeof(response_data),
                                "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/html\r\n"
                                "Content-Length: %lu\r\n\r\n%s\r\n",
                                strlen(content), content);

        tls_buffer response = {
            .data = (uint8_t *)response_data,
            .length = response_len,
        };

        session_log("Sending HTTP response\n");

        sc->response_sent = true;
        require_noerr((err=tls_handshake_callbacks.write(c, response, tls_record_type_AppData)), fail);
    }
#endif


fail:
    mySSLFree(&out);
    sc->err = err; // set the last error in context
    return err;
}

static
int tls_stream_parser_process(tls_stream_parser_ctx_t ctx, tls_buffer record)
{
    tls_server_ctx_t *sc = (tls_server_ctx_t *)ctx;

    session_log("len = %zu\n", record.length);

    if(record.data[0]&0x80) {
        session_log("processing SSL2 record\n");
        return tls_handshake_process(sc->hdsk, record, tls_record_type_SSL2);
    }

    return tls_record_process(sc, record);
}

static
int init_connection(tls_server_ctx_t **psc, int fd, struct sockaddr_in address, tls_server_params *params)
{
    int err = errSecAllocate;
    tls_server_ctx_t *sc;

    struct ccrng_state *rng = CCRNGSTATE();
    if (!rng) {
        abort();
    }

    require((sc = malloc(sizeof(tls_server_ctx_t))), fail);

    memset(sc, 0, sizeof(tls_server_ctx_t));
    sc->sock = fd;
    sc->params = params;
    sc->address = address;

    require((sc->rec = tls_record_create(params->dtls, rng)), fail);
    require((sc->hdsk = tls_handshake_create(params->dtls, true)), fail);
    require((sc->parser = tls_stream_parser_create(sc, tls_stream_parser_process)), fail);

    require_noerr((err=tls_handshake_set_callbacks(sc->hdsk,
                                                   &tls_handshake_callbacks,
                                                   sc)),
                  fail);
    //require_noerr((err=tls_handshake_set_npn_enable(sc->hdsk, true)), fail);
    //require_noerr((err=tls_handshake_set_alpn_data(sc->hdsk, alpnData)), fail);

    require_noerr((err = tls_handshake_set_identity(sc->hdsk, &params->cert1, params->key1)), fail);

    if(params->config)
        require_noerr((err=tls_handshake_set_config(sc->hdsk, atoi(params->config))), fail);
    if(params->num_ciphersuites)
        require_noerr((err=tls_handshake_set_ciphersuites(sc->hdsk, params->ciphersuites, params->num_ciphersuites)), fail);
    if(params->protocol_min)
        require_noerr((err=tls_handshake_set_min_protocol_version(sc->hdsk, params->protocol_min)), fail);
    if(params->protocol_max)
        require_noerr((err=tls_handshake_set_max_protocol_version(sc->hdsk, params->protocol_max)), fail);
    require_noerr((err=tls_handshake_set_resumption(sc->hdsk,params->allow_resumption)), fail);
    require_noerr(tls_handshake_set_renegotiation(sc->hdsk, params->allow_renegotiation), fail);
    require_noerr(tls_handshake_set_client_auth(sc->hdsk, params->client_auth), fail);

    if(params->ocsp) {
        require_noerr((err=tls_handshake_set_ocsp_enable(sc->hdsk, true)), fail);
        require_noerr((err=tls_handshake_set_ocsp_response(sc->hdsk, params->ocsp)), fail);
    }

    if (params->dh_parameters)
        require_noerr((err=tls_handshake_set_dh_parameters(sc->hdsk, params->dh_parameters)), fail);
    if(params->rsa_server_key_exchange)
        require_noerr((err=tls_set_encrypt_pubkey(sc->hdsk, &params->cert1)), fail);

    require_noerr(tls_handshake_set_ems_enable(sc->hdsk, params->allow_ext_master_secret), fail);
    /* success, print ciphersuites, return allocated context */

    if(params->verbose) {
        unsigned enabled_ciphers_count = 0;
        const uint16_t *enabled_ciphers = NULL;
        tls_handshake_get_ciphersuites(sc->hdsk, &enabled_ciphers, &enabled_ciphers_count);
        session_log("Enabled %d ciphersuites:\n", enabled_ciphers_count);
        for(unsigned i=0; i<enabled_ciphers_count; i++) {
            session_log("%02d: (%04x) %s\n", i, enabled_ciphers[i], sslcipher_itoa(enabled_ciphers[i]));
        }
    }

    *psc = sc;
    return 0;
fail:
    return err;
}

static
void clean_connection(tls_server_ctx_t *sc)
{
    if(sc->hdsk) tls_handshake_destroy(sc->hdsk);
    if(sc->rec) tls_record_destroy(sc->rec);
    if(sc->parser) tls_stream_parser_destroy(sc->parser);
    // close(sc->sock) -- closed by the cancel ?;
}

#if 0

#define MAX_READ 2048
static unsigned char databuffer[MAX_READ];
static 
int read_and_process_socket(tls_server_ctx_t *sclient)
{
    ssize_t nr;
    tls_buffer readbuffer;

    nr = recv(client->sock, databuffer, MAX_READ, 0);
    if(nr<=0) return (int)nr;

    readbuffer.data = databuffer;
    readbuffer.length = nr;
    printf("recvd %zd bytes, parse it\n", nr);
    return tls_stream_parser_parse(client->parser, readbuffer);
}

static
int read_and_process_stdin(tls_server_ctx_t *sclient)
{
    ssize_t nr;
    tls_buffer readbuffer;

    nr = read(STDIN_FILENO, databuffer, MAX_READ);
    if(nr<=0) return (int)nr;

    readbuffer.data = databuffer;
    readbuffer.length = nr;
    printf("input %zd bytes, send it\n", nr);

    return tls_handshake_callbacks.write(client, readbuffer, tls_record_type_AppData);
}

#endif

static
void list_connections(CFArrayRef connections)
{
    CFIndex idx;
    printf("***** Active connections *****\n");
    for(idx=0; idx<CFArrayGetCount(connections); idx++) {
        const tls_server_ctx_t *sc = CFArrayGetValueAtIndex(connections, idx);
        printf("%ld: sc=%p, from=%s:%d\n", idx, sc,
               inet_ntoa(sc->address.sin_addr), sc->address.sin_port);
    }
    printf("*** End Active Connections ***\n");
}


static CFMutableArrayRef connections;
static dispatch_queue_t read_queue = NULL;


static const tls_server_ctx_t *select_session(void)
{
    char line[80];

    list_connections(connections);
    fgets(line, sizeof(line), stdin);
    printf("User input: %s\n", line);
    CFIndex idx = strtol(line, NULL, sizeof(line));

    if(idx>=CFArrayGetCount(connections))
    {
        return NULL;
    }

    return (const tls_server_ctx_t *)CFArrayGetValueAtIndex(connections, idx);

}

static int renegotiate_session(void)
{
    const tls_server_ctx_t *sc = NULL;
    __block int err;

    printf("Renegotiating: which session ?\n");
    sc = select_session();

    if(sc) {
        session_log("Changing Server identity\n");
        err=tls_handshake_set_identity(sc->hdsk, &sc->params->cert2, sc->params->key2);
        if(err) {
            session_log("Changing Server identity failed, err=%d\n", err);
        }

        dispatch_sync(read_queue, ^{
            err = tls_handshake_request_renegotiation(sc->hdsk);
        });
        session_log("Renegotiation requested, err= %d\n", err);
    } else {
        printf("No session selected.\n");
        err = -1;
    }
    return err;
}

static int close_session(void)
{
    const tls_server_ctx_t *sc;
    __block int err;

    printf("Closing: which session ?\n");
    sc = select_session();

    if(sc) {
        dispatch_sync(read_queue, ^{
            err = tls_handshake_close(sc->hdsk);
        });
        session_log("Closed session err= %d\n", err);
    } else {
        printf("No session selected.\n");
        err = -1;
    }
    return err;
}

static void tls_handle_event(int server_sock, unsigned long data, tls_server_params *params)
{
    int err;
    struct sockaddr_in my_sock;
    socklen_t my_socklen;
    int fd;
    dispatch_source_t fd_source;
    tls_server_ctx_t *sc = NULL;

    my_socklen = sizeof(my_sock);

    fd = accept(server_sock, (struct sockaddr *)&my_sock, &my_socklen);
    require((fd>=0), connect_fail);
    fd_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, (uintptr_t)fd, 0, read_queue);
    require_noerr((err=init_connection(&sc, fd, my_sock, params)), connect_fail);
    server_log("Created connection sc=%p\n", sc);

    // Adding connection to Array
    CFArrayAppendValue(connections, sc);

    list_connections(connections);

    dispatch_source_set_cancel_handler(fd_source, ^{
        CFIndex idx;
        session_log("Cancelling connection\n");
        close(sc->sock);
        clean_connection(sc);
        idx = CFArrayGetFirstIndexOfValue(connections,
                                          CFRangeMake(0, CFArrayGetCount(connections)),
                                          sc);
        CFArrayRemoveValueAtIndex(connections, idx);
        list_connections(connections);
    });

    dispatch_source_set_event_handler(fd_source, ^{
        ssize_t nr;
        int err = -1;
        tls_buffer readbuffer = {0,};

        unsigned long data = dispatch_source_get_data(fd_source);

        session_log("source event data = %lu\n", data);

        if(data==0) {
            session_log("EOF? Socket closed ?\n");
            err = -1;
            goto done;
        }

        require_noerr(mySSLAlloc(&readbuffer, data),done);

        nr = recv(fd, readbuffer.data, readbuffer.length, 0);
        require(nr>0, done);

        readbuffer.length = nr;
        session_log("recvd %zd bytes, parse it\n", nr);
        require_noerr_quiet((err=tls_stream_parser_parse(sc->parser, readbuffer)), done);

    done:
        session_log("done, err=%d\n", err);

        mySSLFree(&readbuffer);
        if(err) {
            session_log("Error while parsing incoming data, err = %d\n", err);
            session_log("Cancelling connection\n");
            dispatch_source_cancel(fd_source);
        }
    });

    dispatch_resume(fd_source);

    return;
connect_fail:
    server_log("new connection failed\n");
    return;
}

static tls_server_ctx_t *find_dtls_session(CFArrayRef connections, struct sockaddr_in *address)
{
    CFIndex idx;
    tls_server_ctx_t *sc;

    for(idx=0; idx<CFArrayGetCount(connections); idx++) {
        sc = (tls_server_ctx_t *)CFArrayGetValueAtIndex(connections, idx);
        if(memcmp(address, &sc->address, sizeof(struct sockaddr_in))==0)
            return sc;
    }
    return NULL;
}

static void dtls_handle_event(int server_sock, unsigned long data, tls_server_params *params)
{
    ssize_t nr;
    int err = -1;
    tls_buffer readbuffer = {0,};
    struct sockaddr_in address = {0,};
    socklen_t address_len = sizeof(address);

    server_log("source event data = %lu\n", data);

    if(data==0) {
        server_log("EOF? Socket closed ?\n");
        err = -1;
        goto done;
    }

    require_noerr(mySSLAlloc(&readbuffer, data),done);

    nr = recvfrom(server_sock, readbuffer.data, readbuffer.length, 0,
                  (struct sockaddr *)&address, &address_len);
    if(nr<0) {
        perror("recvfrom");
        goto done;
    }
    if(nr==0) {
        server_log("No data?\n");
        err = -1;
        goto done;
    }

    readbuffer.length = nr;
    server_log("recvd %zd bytes from src=%s:%d\n", nr, inet_ntoa(address.sin_addr), address.sin_port);


    tls_server_ctx_t *sc = find_dtls_session(connections, &address);

    if(sc==NULL) {
        err = init_connection(&sc, server_sock, address, params);
        if (err) {
            server_log("Failed to create dtls connection, src=%s:%d\n", inet_ntoa(address.sin_addr), address.sin_port);
        } else {
            server_log("Created dtls connection, src=%s:%d, sc=%p\n", inet_ntoa(address.sin_addr), address.sin_port, sc);

            // Adding connection to dictionary, indexed by ip addr:port
            CFArrayAppendValue(connections, sc);

            list_connections(connections);
        }
    }

    if(sc) {
        err=tls_record_process(sc, readbuffer);
    }

done:
    server_log("done, err=%d\n", err);

    mySSLFree(&readbuffer);
    if(err) {
        server_log("Error while parsing incoming data, src=%s:%d, err = %d\n",
                   inet_ntoa(address.sin_addr), address.sin_port, err);
    }
}

static int start_server(tls_server_params *params)
{
    dispatch_source_t socket_source  = NULL;

    g_cache = tls_cache_create();

    int server_sock = SocketBind(params->port, params->dtls);
    if(server_sock<0)
        return server_sock;

    server_log("Bound to socket: %d\n", server_sock);

    if(!params->dtls && (listen(server_sock, 5)==-1))
    {
        perror("listen");
        return -errno;
    }

    require((read_queue = dispatch_queue_create("server read queue", DISPATCH_QUEUE_SERIAL)), fail);
    require((socket_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, (uintptr_t) server_sock, 0, read_queue)), fail);

    dispatch_source_set_cancel_handler(socket_source, ^{
        close(server_sock);
    });

    dispatch_source_set_event_handler(socket_source, ^{
        unsigned long data = dispatch_source_get_data(socket_source);

        if(params->dtls) {
            dtls_handle_event(server_sock, data, params);
        } else {
            tls_handle_event(server_sock, data, params);
        }
    });

    /* Init dictionnary of connections for UI */
    connections = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);

    dispatch_resume(socket_source);

    server_log("Main (D)TLS server thread is now spinning...\n");

    char  line[80];
    do {
        fgets(line, sizeof(line), stdin);

        switch(line[0]) {
            case 'r': /* renegotiate */
                renegotiate_session();
                break;
            case 'c': /* close */
                close_session();
                break;
            case 'q':
                break;
            default:
                server_log("Unknown command '%s'\n", line);
                break;
        }

    } while(line[0]!='q');

fail:
    if(read_queue)
        dispatch_release(read_queue);
    
    if(socket_source) {
        dispatch_source_cancel(socket_source);
        dispatch_release(socket_source);
    }
    
    tls_cache_destroy(g_cache);

    return 0;
}


/* For the short-dh test */
uint16_t only_dhe_rsa_ciphers[] = {
    SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,
};

/* For the freak test */
uint16_t only_rsa_ciphers[] = {
    SSL_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_RSA_WITH_AES_128_CBC_SHA,
    TLS_RSA_WITH_AES_128_CBC_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA,
};

/* Short DH parameters
 Diffie-Hellman-Parameters: (16 bit)
 prime: 35963 (0x8c7b)
 generator: 2 (0x2)
 */

static uint8_t dh_parameters_16_data[] = {
    0x30, 0x08, 0x02, 0x03, 0x00, 0x8c, 0x7b, 0x02, 0x01, 0x02,
};

static tls_buffer dh_parameters_16 = {
    .data = dh_parameters_16_data,
    .length = sizeof(dh_parameters_16_data),
};


static uint8_t dh_parameters_512_data[] = {
    0x30, 0x46, 0x02, 0x41, 0x00, 0x85, 0xcd, 0xc1, 0x7e, 0x26, 0xeb, 0x37,
    0x84, 0x13, 0xd0, 0x3b, 0x07, 0xc1, 0x57, 0x7d, 0xf3, 0x55, 0x8d, 0xa0,
    0xc4, 0xa5, 0x03, 0xc4, 0x2c, 0xc6, 0xd5, 0xa6, 0x31, 0xcb, 0x68, 0xdf,
    0x5d, 0x96, 0x20, 0x1a, 0x15, 0x57, 0x49, 0x7d, 0xd7, 0x51, 0x65, 0x6e,
    0x37, 0xa8, 0xe3, 0xe9, 0xe1, 0x59, 0x2e, 0xd4, 0x57, 0x4a, 0xf0, 0xcb,
    0x0e, 0x85, 0x07, 0xdd, 0x35, 0xa7, 0xe3, 0xc6, 0xbb, 0x02, 0x01, 0x02
};

static tls_buffer dh_parameters_512 = {
    .data = dh_parameters_512_data,
    .length = sizeof(dh_parameters_512_data),
};

static uint8_t dh_parameters_768_data[] = {
    0x30, 0x66, 0x02, 0x61, 0x00, 0xe1, 0xa2, 0x50, 0xab, 0xb0, 0xdc, 0xef,
    0xe1, 0x2f, 0xd9, 0xde, 0x59, 0x86, 0x24, 0x43, 0x3b, 0xf3, 0x40, 0x9d,
    0x02, 0xcc, 0xe2, 0x70, 0x63, 0x46, 0x8d, 0x0f, 0xf3, 0x8a, 0xc6, 0xa0,
    0x1d, 0x7b, 0x30, 0x83, 0x10, 0x48, 0x40, 0x28, 0xa4, 0x3e, 0xbe, 0x4d,
    0xb6, 0xea, 0x90, 0x02, 0xae, 0x25, 0x93, 0xc0, 0xe8, 0x36, 0x5c, 0xc8,
    0xc8, 0x0b, 0x04, 0xd5, 0x05, 0xac, 0x67, 0x24, 0x4b, 0xa9, 0x42, 0x5a,
    0x03, 0x65, 0x4d, 0xd0, 0xc0, 0xbd, 0x78, 0x32, 0xd0, 0x8c, 0x0a, 0xf4,
    0xbf, 0xd1, 0x61, 0x86, 0x13, 0x13, 0x3b, 0x83, 0xce, 0xbf, 0x3b, 0xbc,
    0x8f, 0xf9, 0x4e, 0x50, 0xe3, 0x02, 0x01, 0x02
};

static tls_buffer dh_parameters_768 = {
    .data = dh_parameters_768_data,
    .length = sizeof(dh_parameters_768_data),
};

static uint8_t dh_parameters_1024_data[] = {
    0x30, 0x81, 0x87, 0x02, 0x81, 0x81, 0x00, 0xd5, 0x06, 0x69, 0xc6, 0xd4,
    0x98, 0x2b, 0xe3, 0x49, 0xe2, 0xa1, 0x9b, 0x82, 0xaf, 0x3f, 0xaa, 0xc3,
    0x86, 0x2a, 0x7a, 0xfa, 0x62, 0x12, 0x33, 0x45, 0x9f, 0x34, 0x57, 0xc6,
    0x6c, 0x88, 0x81, 0xa6, 0x5d, 0xa3, 0x43, 0xe5, 0x4d, 0x87, 0x4f, 0x69,
    0x3d, 0x2b, 0xc8, 0x18, 0xb6, 0xd7, 0x29, 0x53, 0x94, 0x0d, 0x73, 0x9b,
    0x08, 0x22, 0x73, 0x84, 0x7b, 0x5a, 0x03, 0x2e, 0xfc, 0x10, 0x9b, 0x35,
    0xc6, 0xa1, 0xca, 0x36, 0xd0, 0xcc, 0x3e, 0xa2, 0x04, 0x3a, 0x8a, 0xe8,
    0x87, 0xe8, 0x60, 0x72, 0xee, 0x99, 0xf3, 0x04, 0x0a, 0xd8, 0x1a, 0xe6,
    0xfc, 0xbc, 0xe1, 0xc5, 0x9d, 0x3a, 0xca, 0xf9, 0xfd, 0xbf, 0x58, 0xd3,
    0x4d, 0xde, 0x8b, 0x4a, 0xb5, 0x37, 0x1e, 0x6d, 0xf4, 0x22, 0x0f, 0xb7,
    0x48, 0x0a, 0xda, 0x82, 0x40, 0xc9, 0x55, 0x20, 0x01, 0x3b, 0x35, 0xb2,
    0x94, 0x68, 0xab, 0x02, 0x01, 0x02
};

static tls_buffer dh_parameters_1024 = {
    .data = dh_parameters_1024_data,
    .length = sizeof(dh_parameters_1024_data),
};


/* A dummy ocsp response */
static uint8_t ocsp_response_data[] = {
    0x30, 0x82, 0x01, 0x72, 0x0a, 0x01, 0x00, 0xa0, 0x82, 0x01, 0x6b, 0x30,
    0x82, 0x01, 0x67, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30,
    0x01, 0x01, 0x04, 0x82, 0x01, 0x58, 0x30, 0x82, 0x01, 0x54, 0x30, 0x81,
    0xbe, 0xa1, 0x22, 0x30, 0x20, 0x31, 0x1e, 0x30, 0x1c, 0x06, 0x03, 0x55,
    0x04, 0x03, 0x13, 0x15, 0x63, 0x6f, 0x72, 0x65, 0x54, 0x4c, 0x53, 0x20,
    0x43, 0x41, 0x20, 0x43, 0x65, 0x72, 0x74, 0x20, 0x28, 0x52, 0x53, 0x41,
    0x29, 0x18, 0x0f, 0x32, 0x30, 0x31, 0x34, 0x30, 0x38, 0x32, 0x30, 0x32,
    0x31, 0x31, 0x33, 0x30, 0x37, 0x5a, 0x30, 0x62, 0x30, 0x60, 0x30, 0x3a,
    0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00, 0x04,
    0x14, 0x20, 0xd4, 0x96, 0xb3, 0xfb, 0xd1, 0xb8, 0x84, 0x3a, 0x38, 0x14,
    0xdb, 0x33, 0xd1, 0x0d, 0xa8, 0xca, 0x96, 0xba, 0x13, 0x04, 0x14, 0xb2,
    0x23, 0x1b, 0x0f, 0x2c, 0x5a, 0xa2, 0x1d, 0xeb, 0x96, 0x34, 0xa7, 0x6f,
    0x9d, 0x97, 0x11, 0x81, 0x14, 0x61, 0xbb, 0x02, 0x01, 0x01, 0xa1, 0x11,
    0x18, 0x0f, 0x32, 0x30, 0x31, 0x34, 0x30, 0x38, 0x32, 0x30, 0x30, 0x30,
    0x30, 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f, 0x32, 0x30, 0x31, 0x34, 0x30,
    0x38, 0x32, 0x30, 0x32, 0x31, 0x31, 0x33, 0x30, 0x37, 0x5a, 0xa1, 0x23,
    0x30, 0x21, 0x30, 0x1f, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07,
    0x30, 0x01, 0x02, 0x04, 0x12, 0x04, 0x10, 0x4c, 0xc5, 0x63, 0xf2, 0x0a,
    0x84, 0x8c, 0x03, 0xa4, 0x0d, 0x97, 0xd1, 0xa2, 0xbb, 0x1e, 0xb2, 0x30,
    0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05,
    0x05, 0x00, 0x03, 0x81, 0x81, 0x00, 0x1b, 0x21, 0xd7, 0x01, 0xde, 0xb8,
    0x58, 0x4b, 0x79, 0x6a, 0xa3, 0x8b, 0xa7, 0xe0, 0xbd, 0xa8, 0xda, 0x58,
    0x48, 0xbb, 0xa7, 0xcd, 0xf7, 0x91, 0x15, 0xb3, 0x38, 0x70, 0xd9, 0x43,
    0x25, 0x72, 0x0e, 0xc3, 0x3d, 0xf9, 0xc7, 0x30, 0x2d, 0xb4, 0x9f, 0x1c,
    0x4b, 0x62, 0x31, 0x48, 0xb4, 0x9f, 0x00, 0xbd, 0x57, 0xb6, 0xec, 0xda,
    0xf0, 0xa2, 0x42, 0x61, 0xfc, 0xef, 0x73, 0xc5, 0x55, 0xc1, 0xf6, 0x72,
    0x79, 0xcf, 0x55, 0x01, 0x09, 0xe4, 0xd2, 0xee, 0xbd, 0xa6, 0x08, 0xc6,
    0x39, 0x3a, 0x17, 0x76, 0x98, 0xaa, 0x61, 0x82, 0xb9, 0x41, 0xe1, 0xbb,
    0x4f, 0x67, 0x5e, 0x0b, 0x5e, 0xfa, 0x3c, 0x12, 0x15, 0xbe, 0x90, 0x8e,
    0x29, 0xe6, 0x5c, 0x9b, 0xfc, 0xaf, 0x40, 0xa4, 0x31, 0xd7, 0xa4, 0xc6,
    0x71, 0x22, 0x01, 0xfa, 0xb2, 0xcd, 0x6e, 0x1f, 0x26, 0xdb, 0xb1, 0xa3,
    0xec, 0x43
};
static tls_buffer ocsp_response = {
    .data = ocsp_response_data,
    .length = sizeof(ocsp_response_data),
};

static void
logger(void * __unused ctx, const char *scope, const char *function, const char *str)
{
    printf("[%s] %s: %s\n", scope, function, str);
}


int main(int argc, const char * argv[])
{

    bool use_ecdsa_cert = false;
    bool use_empty_cert = false;
    uint16_t cipher_to_use;
    tls_server_params params;

    memset(&params, 0, sizeof(params));
    params.hostname="localhost";
    params.port=4443;
    params.allow_resumption = true;
    params.allow_ext_master_secret = true;

    if (argc > 1) {
        int i = 0;
        for (i = 0 ; i < argc ; i+=1){
            if (strcmp(argv[i], "--port") == 0 && argv[i+1] != NULL){
                params.port = atoi(argv[++i]);
            }
            if (strcmp(argv[i], "--config") == 0 && argv[i+1] != NULL){
                params.config = argv[++i];
            }
            if (strcmp(argv[i], "--cipher") == 0 && argv[i+1] != NULL){
                cipher_to_use = sslcipher_atoi((char*)argv[++i]);
                params.ciphersuites = &(cipher_to_use);
                params.num_ciphersuites = 1;
            }
            if ((strcmp(argv[i], "--protocol_min") == 0) && (argv[i+1] != NULL)){
                params.protocol_min = (tls_protocol_version)strtoul(argv[++i], NULL, 0);
            }
            if ((strcmp(argv[i], "--protocol_max") == 0) && (argv[i+1] != NULL)){
                params.protocol_max = (tls_protocol_version)strtoul(argv[++i], NULL, 0);
            }
            if (strcmp(argv[i], "--no-resumption") == 0){
                params.allow_resumption = false;
            }
            if (strcmp(argv[i], "--allow-renego") == 0){
                params.allow_renegotiation = true;
            }
            if (strcmp(argv[i], "--use_ecdsa_cert") == 0){
                use_ecdsa_cert = true;
            }
            if (strcmp(argv[i], "--empty_cert_test") == 0){
                use_empty_cert = true;
            }
            if (strcmp(argv[i], "--freak") == 0){
                params.rsa_server_key_exchange = true;
            }
            if ((strcmp(argv[i], "--dh_params") == 0) && (argv[i+1] != NULL)) {
                unsigned long dh_params_size = strtoul(argv[++i], NULL, 0);
                switch (dh_params_size) {
                    case 16:
                        params.dh_parameters = &dh_parameters_16;
                        break;
                    case 512:
                        params.dh_parameters = &dh_parameters_512;
                        break;
                    case 768:
                        params.dh_parameters = &dh_parameters_768;
                        break;
                    case 1024:
                        params.dh_parameters = &dh_parameters_1024;
                        break;
                    default:
                        params.dh_parameters = NULL;
                }
            }
            if (strcmp(argv[i], "--no-extended-ms") == 0){
                params.allow_ext_master_secret = false;
            }
            if (strcmp(argv[i], "--debug") == 0){
                tls_add_debug_logger(logger, NULL);
            }
            if (strcmp(argv[i], "--verbose") == 0){
                params.verbose = true;
            }
            if (strcmp(argv[i], "--client_auth") == 0) {
                params.client_auth = true;
            }
            if (strcmp(argv[i], "--dtls") == 0) {
                params.dtls = true;
            }
            if (strcmp(argv[i], "--use_kext") == 0) {
                params.use_kext = true;
            }
        }
    }

    if (use_empty_cert)
    {
        init_server_keys(false,
                         NULL, 0,
                         Server1_Key_rsa_der,Server1_Key_rsa_der_len,
                         &params.cert1, &params.key1);
        init_server_keys(false,
                         NULL, 0,
                         Server2_Key_rsa_der, Server2_Key_rsa_der_len,
                         &params.cert2, &params.key2);
    }
    else if (use_ecdsa_cert)
    {
        init_server_keys(true,
                         eccert_der, eccert_der_len,
                         eckey_der, eckey_der_len,
                         &params.cert1, &params.key1);
        init_server_keys(false,
                         Server2_Cert_rsa_rsa_der, Server2_Cert_rsa_rsa_der_len,
                         Server2_Key_rsa_der, Server2_Key_rsa_der_len,
                         &params.cert2, &params.key2);
    }
    else
    {
        init_server_keys(false,
                         Server1_Cert_rsa_rsa_der, Server1_Cert_rsa_rsa_der_len,
                         Server1_Key_rsa_der,Server1_Key_rsa_der_len,
                         &params.cert1, &params.key1);
        init_server_keys(false,
                         Server2_Cert_rsa_rsa_der, Server2_Cert_rsa_rsa_der_len,
                         Server2_Key_rsa_der, Server2_Key_rsa_der_len,
                         &params.cert2, &params.key2);
    }
    params.ocsp = &ocsp_response;
    fprintf(stderr, "***** Listenning for connection at  %s:%d\n", params.hostname, params.port);

    return start_server(&params);
}
