//
//  tls_client.c
//  coretls_client
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <AssertMacros.h>


/* extern struct ccrng_state *ccDRBGGetRngState(); */
#include <CommonCrypto/CommonRandomSPI.h>
#define CCRNGSTATE() ccDRBGGetRngState()

#include <tls_ciphersuites.h>
#include <tls_helpers.h>
#include <tls_cache.h>
#include <Security/CipherSuite.h>

#include <Security/SecKeyPriv.h>
#include <Security/SecIdentity.h>

#include "appleSession.h"
#include "secCrypto.h"
#include "tls_client.h"
#include "tls_alloc.h"
#include "sockets.h"

static
__attribute__((format(printf, 3, 4)))
void _log(const tls_client_ctx_t *cc, const char *function, const char *str, ...)
{
    va_list ap;

    va_start(ap, str);

    if(cc) {
        printf("[%p] ", cc);
    }
    printf("%s: ", function);
    vprintf(str, ap);
}

#define session_log(...) _log(cc, __FUNCTION__, __VA_ARGS__);
#define client_log(...) _log(NULL, __FUNCTION__, __VA_ARGS__);


//#define test_printf(x,...)
#define test_printf printf
#define DEBUG_ONLY __attribute__((unused))


/*** tls_handshake Callbacks ***/

static int tcp_write(tls_client_ctx_t *cc, tls_buffer out)
{
    while(out.length) {
        ssize_t nwr;
        nwr = send(cc->sock, out.data, out.length, 0);
        if (nwr == -1) {
            session_log("Error writing %zd bytes to socket : %s\n", out.length, strerror(errno));
            return errno;
        }
        out.data += nwr;
        out.length -= nwr;
    }
    return 0;
}

static int udp_write(tls_client_ctx_t *cc, tls_buffer out)
{
    ssize_t nwr;
    nwr = send(cc->sock, out.data, out.length, 0);
    if (nwr == -1) {
        session_log("Error writing %zd bytes to socket : %s\n", out.length, strerror(errno));
        return errno;
    }
    return 0;
}

static
int encrypt_and_write(tls_client_ctx_t *cc, const tls_buffer data, uint8_t content_type)
{
    int err;
    tls_buffer encrypted = {0, };

    err=mySSLAlloc(&encrypted, tls_record_encrypted_size(cc->rec, content_type, data.length));
    require_noerr(err, fail);

    err=tls_record_encrypt(cc->rec, data, content_type, &encrypted);
    require_noerr(err, fail);

    session_log("Writing %5zd encrypted bytes\n", encrypted.length);

    if(cc->params->dtls) {
        err = udp_write(cc, encrypted);
    } else {
        err = tcp_write(cc, encrypted);
    }

fail:
    mySSLFree(&encrypted);
    return err;
}

static
int tls_handshake_write_callback(tls_handshake_ctx_t ctx, const tls_buffer data, uint8_t content_type)
{
    __block int err;
    tls_client_ctx_t *cc = (tls_client_ctx_t *)ctx;

    err = encrypt_and_write(cc, data, content_type);

    return err;
}


static uint8_t alpn_http_1_1[] =  {0x08, 0x68, 0x74, 0x74, 0x70, 0x2f, 0x31, 0x2e, 0x31};
__unused static tls_buffer alpnData = {
    .data = alpn_http_1_1,
    .length = sizeof(alpn_http_1_1),
};

static uint8_t npn_http_1_1[] =  {0x68, 0x74, 0x74, 0x70, 0x2f, 0x31, 0x2e, 0x31};
static tls_buffer npnHttpData = {
    .data = npn_http_1_1,
    .length = sizeof(npn_http_1_1),
};


static int
tls_handshake_message_callback(tls_handshake_ctx_t ctx, tls_handshake_message_t event)
{
    tls_client_ctx_t *cc = (tls_client_ctx_t *)ctx;
    int err = 0;

    session_log("event = %d\n", event);

    switch(event) {
        case tls_handshake_message_client_hello:
            break;
        case tls_handshake_message_certificate:
            require_noerr((err = tls_helper_set_peer_pubkey(cc->hdsk)), errOut);
            break;
        case tls_handshake_message_certificate_request:
            cc->certificate_requested++;
            require_noerr((err = tls_handshake_set_client_auth_type(cc->hdsk, tls_client_auth_type_RSASign)), errOut);
            require_noerr((err = tls_handshake_set_identity(cc->hdsk, &cc->params->certs, cc->params->key)), errOut);
            break;
        case tls_handshake_message_server_hello_done:
            /* Always says the cert is ok */
            require_noerr((err = tls_evaluate_trust(cc->hdsk, cc->trustRef)), errOut);
            break;
        case tls_handshake_message_server_hello:
            /* See if we have NPN,ALPN: */
        {
            const tls_buffer *data;
            data = tls_handshake_get_peer_npn_data(cc->hdsk);
            if(data) {
                session_log("NPN Data = %p, %zd\n", data->data, data->length);
                require_noerr((err=tls_handshake_set_npn_data(cc->hdsk, npnHttpData)), errOut);
            }
            data = tls_handshake_get_peer_alpn_data(cc->hdsk);
            if(data) {
                session_log("ALPN Data = %p, %zd\n", data->data, data->length);
            }

        }
            break;
        default:
            break;
    }

errOut:
    return err;
}

static void
tls_handshake_ready_callback(tls_handshake_ctx_t ctx, bool write, bool ready)
{
    tls_client_ctx_t *cc = (tls_client_ctx_t *)ctx;

    session_log("%s ready=%d\n", write?"write":"read", ready);

    if(ready) {
        // Sending an HTTP get request...
        if(write) {
            cc->write_ready_received++;
        } else {
            cc->read_ready_received++;
        }
    }
}

static int
tls_handshake_set_retransmit_timer_callback(tls_handshake_ctx_t ctx, int attempt)
{
    tls_client_ctx_t DEBUG_ONLY *cc = (tls_client_ctx_t *)ctx;

    session_log("attempt=%d\n", attempt);

    return ENOTSUP;
}

static
int mySSLRecordInitPendingCiphersFunc(tls_handshake_ctx_t ref,
                                      uint16_t            selectedCipher,
                                      bool                server,
                                      tls_buffer           key)
{
    tls_client_ctx_t *cc = (tls_client_ctx_t *)ref;
    session_log("%s: %s, cipher=%04x, server=%d\n", __FUNCTION__, ref, selectedCipher, server);
    return tls_record_init_pending_ciphers(cc->rec, selectedCipher, server, key);
}

static
int mySSLRecordAdvanceWriteCipherFunc(tls_handshake_ctx_t ref)
{
    tls_client_ctx_t *cc = (tls_client_ctx_t *)ref;
    session_log("\n");
    return tls_record_advance_write_cipher(cc->rec);
}

static
int mySSLRecordRollbackWriteCipherFunc(tls_handshake_ctx_t ref)
{
    tls_client_ctx_t *cc = (tls_client_ctx_t *)ref;
    session_log("\n");
    return tls_record_rollback_write_cipher(cc->rec);
}

static
int mySSLRecordAdvanceReadCipherFunc(tls_handshake_ctx_t ref)
{
    tls_client_ctx_t *cc = (tls_client_ctx_t *)ref;
    session_log("\n");
    return tls_record_advance_read_cipher(cc->rec);
}

static
int mySSLRecordSetProtocolVersionFunc(tls_handshake_ctx_t ref,
                                      tls_protocol_version  protocolVersion)
{
    tls_client_ctx_t *cc = (tls_client_ctx_t *)ref;
    session_log("pv=%04x\n", protocolVersion);
    return tls_record_set_protocol_version(cc->rec, protocolVersion);
}


static tls_cache_t g_cache = NULL;

static int
tls_handshake_save_session_data_callback(tls_handshake_ctx_t ctx, tls_buffer sessionKey, tls_buffer sessionData)
{
    tls_client_ctx_t DEBUG_ONLY *cc = (tls_client_ctx_t *)ctx;
    session_log("key = %s data=[%p,%zd]\n", sessionKey.data, sessionData.data, sessionData.length);
    return tls_cache_save_session_data(g_cache, &sessionKey, &sessionData, 0);
}

static int
tls_handshake_load_session_data_callback(tls_handshake_ctx_t ctx, tls_buffer sessionKey, tls_buffer *sessionData)
{
    tls_client_ctx_t DEBUG_ONLY *cc = (tls_client_ctx_t *)ctx;
    int err = tls_cache_load_session_data(g_cache, &sessionKey, sessionData);
    session_log("key = %s data=[%p,%zd], err=%d\n", sessionKey.data, sessionData->data, sessionData->length, err);
    return err;
}

static int
tls_handshake_delete_session_data_callback(tls_handshake_ctx_t ctx, tls_buffer sessionKey)
{
    tls_client_ctx_t DEBUG_ONLY *cc = (tls_client_ctx_t *)ctx;
    session_log("\n");
    return tls_cache_delete_session_data(g_cache, &sessionKey);
}

static int
tls_handshake_delete_all_sessions_callback(tls_handshake_ctx_t ctx)
{
    tls_client_ctx_t DEBUG_ONLY *cc = (tls_client_ctx_t *)ctx;
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
int tls_stream_parser_process(tls_stream_parser_ctx_t ctx, tls_buffer record)
{
    int err = errSecAllocate;
    tls_client_ctx_t *cc = (tls_client_ctx_t *)ctx;
    tls_buffer out;
    uint8_t content_type;

    session_log("len = %zu\n", record.length);
    size_t dlen = tls_record_decrypted_size(cc->rec, record.length);

    mySSLAlloc(&out, dlen+1); // 1 extra byte for \0
    require(out.data, fail);

    require_noerr((err=tls_record_decrypt(cc->rec, record, &out, &content_type)), fail);

    if(content_type!=tls_record_type_AppData) {
        session_log("processing protocol message of type %d, len=%zu\n", content_type, out.length);
        require_noerr_quiet((err = tls_handshake_process(cc->hdsk, out, content_type)), fail);
    } else {
        if(cc->read_ready_received<0)
            session_log("Received data before read_ready\n");
        session_log("received data record, len = %zu\n", out.length);
        out.data[out.length]=0;
        printf("*** DATA ***\n%s\n*** END DATA ***\n", out.data);
    }

fail:
    mySSLFree(&out);
    cc->err = err; // set the last error in context
    return err;
}


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
    CIPHER(TLS_ECDHE_RSA_WITH_NULL_SHA),
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
    
    CIPHER(TLS_ECDH_anon_WITH_NULL_SHA),
    CIPHER(TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA),
    CIPHER(TLS_ECDH_anon_WITH_AES_128_CBC_SHA),
    CIPHER(TLS_ECDH_anon_WITH_AES_256_CBC_SHA),

    { 0, NULL }
};


static uint16_t sslcipher_atoi(char* cipherstring){
    const CipherSuiteName *a = ciphers;
    while (a->cipher > 0) {
        if (strcmp(cipherstring, a->name) == 0) break;
        a++;
    }
    return a->cipher;
}

static const char *sslcipher_itoa(uint16_t cs)
{
    const CipherSuiteName *a = ciphers;
    while(a->cipher > 0) {
        if (cs == a->cipher) break;
        a++;
    }
    return a->name;
}

static
int init_context(tls_client_ctx_t *cc, tls_client_params *params)
{
    int err = errSecAllocate;
    memset(cc, 0, sizeof(tls_client_ctx_t));

    struct ccrng_state *rng = CCRNGSTATE();
    if (!rng) {
        abort();
    }

    cc->params = params;

    require((cc->sock = SocketConnect(params->hostname, params->service, params->dtls))>=0, fail);
    require((cc->rec = tls_record_create(params->dtls, rng)), fail);
    require((cc->hdsk = tls_handshake_create(params->dtls, false)), fail);
    require((cc->parser = tls_stream_parser_create(cc, tls_stream_parser_process)), fail);

    require_noerr((err=tls_handshake_set_callbacks(cc->hdsk,
                                                   &tls_handshake_callbacks,
                                                   cc)),
                  fail);
    require_noerr((err=tls_handshake_set_false_start(cc->hdsk, true)), fail);
    require_noerr((err=tls_handshake_set_peer_hostname(cc->hdsk, params->hostname, strlen(params->hostname))), fail);
    require_noerr((err=tls_handshake_set_npn_enable(cc->hdsk, true)), fail);
    /* TODO: convert ALPN string into ALPN data */
    if(params->alpn_string) {
        tls_buffer alpnData;

        alpnData.length = strlen(params->alpn_string)+1;
        alpnData.data = malloc(alpnData.length);
        alpnData.data[0] = alpnData.length-1;
        memcpy(alpnData.data+1, params->alpn_string, alpnData.length-1);

        require_noerr((err=tls_handshake_set_alpn_data(cc->hdsk, alpnData)), fail);
    }

    if(params->config)
        require_noerr((err=tls_handshake_set_config(cc->hdsk, atoi(params->config))), fail);
    if(params->num_ciphersuites)
        require_noerr((err=tls_handshake_set_ciphersuites(cc->hdsk, params->ciphersuites, params->num_ciphersuites)), fail);
    if(params->protocol_min)
        require_noerr((err=tls_handshake_set_min_protocol_version(cc->hdsk, params->protocol_min)), fail);
    if(params->protocol_max)
        require_noerr((err=tls_handshake_set_max_protocol_version(cc->hdsk, params->protocol_max)), fail);
    require_noerr((err=tls_handshake_set_resumption(cc->hdsk,params->allow_resumption)), fail);
    require_noerr((err=tls_handshake_set_session_ticket_enabled(cc->hdsk, params->session_tickets_enabled)), fail);
    require_noerr((err=tls_handshake_set_ocsp_enable(cc->hdsk, params->ocsp_enabled)), fail);
    require_noerr((err=tls_handshake_set_min_dh_group_size(cc->hdsk, params->min_dh_size)), fail);
    require_noerr((err=tls_handshake_set_fallback(cc->hdsk, params->fallback)), fail);
    require_noerr((err=tls_handshake_set_ems_enable(cc->hdsk, params->allow_ext_master_secret)), fail);

fail:
    return err;
}

static
void clean_context(tls_client_ctx_t *cc)
{
    if(cc->hdsk) tls_handshake_destroy(cc->hdsk);
    if(cc->rec) tls_record_destroy(cc->rec);
    if(cc->parser) tls_stream_parser_destroy(cc->parser);
    if(cc->sock>=0) close(cc->sock);
}


static int session_status(tls_client_ctx_t *cc)
{
    int err = 0;

    printf("read_ready received: %d\n",cc->read_ready_received);
    printf("write_ready received: %d\n",cc->write_ready_received);
    printf("certificate requested: %d\n", cc->certificate_requested);
    printf("negotiated ciphersuite: %04x\n", tls_handshake_get_negotiated_cipherspec(cc->hdsk));

    return err;
}

#define MAX_READ 16384
static unsigned char databuffer[MAX_READ];
static 
int read_and_process_socket(tls_client_ctx_t *cc)
{
    ssize_t nr;
    tls_buffer readbuffer;

    nr = recv(cc->sock, databuffer, MAX_READ, 0);
    if(nr==0) return -1; // EOF ?
    if(nr<0) {
        perror("recv");
        return errno;
    }

    readbuffer.data = databuffer;
    readbuffer.length = nr;
    printf("recvd %zd bytes, parse it\n", nr);
    if(cc->params->dtls) {
        return tls_stream_parser_process(cc, readbuffer);
    } else {
        return tls_stream_parser_parse(cc->parser, readbuffer);
    }
}

static
int read_and_process_stdin(tls_client_ctx_t *cc)
{
    ssize_t nr;
    tls_buffer readbuffer;

    nr = read(STDIN_FILENO, databuffer, MAX_READ);
    if(nr==0) return -1; // EOF ?
    if(nr<=0) return errno;

    if(databuffer[0]=='R') { //renegotiate
        return tls_handshake_negotiate(cc->hdsk, NULL);
    } else if(databuffer[0]=='Q') {
        return -2; //Quit
    } else {
        readbuffer.data = databuffer;
        readbuffer.length = nr;
        printf("input %zd bytes, send it\n", nr);
        return tls_handshake_callbacks.write(cc, readbuffer, tls_record_type_AppData);
    }
}

static int client_connect(tls_client_params *params)
{
    int err;
    tls_client_ctx_t client;
    tls_client_ctx_t *cc = &client;

    require_noerr((err=init_context(cc, params)), errOut);

    tls_buffer peerID = { sizeof(params->peer_id), (uint8_t *)&params->peer_id};

    unsigned enabled_ciphers_count = 0;
    const uint16_t *enabled_ciphers = NULL;
    tls_handshake_get_ciphersuites(cc->hdsk, &enabled_ciphers, &enabled_ciphers_count);

    session_log("Enabled %d ciphersuites:\n", enabled_ciphers_count);
    for(unsigned i=0; i<enabled_ciphers_count; i++) {
        session_log("%02d: (%04x) %s\n", i, enabled_ciphers[i], sslcipher_itoa(enabled_ciphers[i]));
    }

    // Start handshake...
    if(params->peer_id) {
        require_noerr((err=tls_handshake_negotiate(cc->hdsk, &peerID)), errOut);
    } else {
        require_noerr((err=tls_handshake_negotiate(cc->hdsk, NULL)), errOut);
    }

    /* read and process socket until write ready */
    while(cc->write_ready_received==0 || cc->read_ready_received==0) {
        require_noerr((err=read_and_process_socket(cc)), errOut);
    }

    session_status(cc);


    printf("Sending request: %s\n", cc->params->request);

    tls_buffer request = {
        .data = (uint8_t *)cc->params->request,
        .length = strlen(cc->params->request),
    };


    tls_handshake_callbacks.write(cc, request, tls_record_type_AppData);


    /* then read stdin and socket / select */
    while(true){
        fd_set read_fds;

        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO,&read_fds);
        FD_SET(cc->sock,&read_fds);

        if (select(cc->sock+1,&read_fds,NULL,NULL,NULL) == -1){
            perror("select:");
            goto errClose;
        }

        // if there are any data ready to read from the socket
        if (FD_ISSET(cc->sock, &read_fds)){
            session_log("Data on socket\n");
            if((err=read_and_process_socket(cc))) {
                session_log("Error while reading socket : %d\n", err);
                goto errClose;
            }
        }

        // if there is something in stdin
        if (FD_ISSET(STDIN_FILENO, &read_fds)){
            printf("Data on stdin\n");
            if((err=read_and_process_stdin(cc))) {
                if(err==-2) {
                    session_log("Quitting\n");
                    goto errClose;
                }
                session_log("Error while reading stdin : %d\n", err);
                if(err!=-1) perror("stdin");
                //exit(1);   //Delete this line since the iOS automation will keep coming with null stdin
            }
        }
    }

errClose:
    tls_handshake_close(cc->hdsk);


errOut:

    clean_context(cc);
    return err;
}


static void
logger(void * __unused ctx, const char *scope, const char *function, const char *str)
{
    printf("[%s] %s: %s\n", scope, function, str);
}

//#define HOSTNAME "scotthelme.co.uk"
#define HOSTNAME "www.google.com"
int main(int argc, const char * argv[])
{
    int reconnects = 0;
    int reconnect_delay = 0;
    uint16_t cipher_to_use;
    tls_client_params params;
    int err;
    static char get_request[100];
    bool resume_with_higher_version = false;

    memset(&params, 0, sizeof(params));
    //params.hostname="www.imperialviolet.org";
    //params.port="6628";
    params.hostname=HOSTNAME;
    params.service="https";
    params.request = get_request;
    params.ocsp_enabled = true;
    params.session_tickets_enabled = true;
    params.allow_resumption = true;
    params.allow_ext_master_secret = true;
    params.peer_id = 'P';

    if (argc > 1) {
        int i = 0;
        for (i = 0 ; i < argc ; i+=1){
            if (strcmp(argv[i], "--host") == 0 && argv[i+1] != NULL){
                params.hostname = argv[++i];
            }
            if (strcmp(argv[i], "--port") == 0 && argv[i+1] != NULL){
                params.service = argv[++i];
            }
            if (strcmp(argv[i], "--config") == 0 && argv[i+1] != NULL){
                params.config = argv[++i];
            }
            if (strcmp(argv[i], "--alpn") == 0 && argv[i+1] != NULL){
                params.alpn_string = argv[++i];
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
            if (strcmp(argv[i], "--resume_with_higher_version") == 0) {
                reconnects = 1;
                resume_with_higher_version = true;
                params.protocol_max = tls_protocol_version_TLS_1_0;

            }
            if (strcmp(argv[i], "--reconnect") == 0 && argv[i+1] != NULL){
                reconnects = atoi(argv[++i]);
            }
            if (strcmp(argv[i], "--reconnect_delay") == 0 && argv[i+1] != NULL){
                reconnect_delay = atoi(argv[++i]);
            }
            if (strcmp(argv[i], "--no-resumption") == 0){
                params.allow_resumption = false;
            }
            if (strcmp(argv[i], "--no-tickets") == 0){
                params.session_tickets_enabled = false;
            }
            if (strcmp(argv[i], "--no-ocsp") == 0){
                params.ocsp_enabled = false;
            }
            if (strcmp(argv[i], "--min-dh-size") == 0 && argv[i+1] != NULL){
                params.min_dh_size = atoi(argv[++i]);;
            }
            if (strcmp(argv[i], "--dtls") == 0){
                params.dtls = true;
            }
            if (strcmp(argv[i], "--fallback") == 0){
                params.fallback = true;
            }
            if (strcmp(argv[i], "--no-extended-ms") == 0){
                params.allow_ext_master_secret = false;
            }
            if (strcmp(argv[i], "--debug") == 0){
                tls_add_debug_logger(logger, NULL);
            }
        }
    }

    memset(get_request, 0, sizeof(get_request));
    snprintf(get_request, sizeof(get_request), "GET / HTTP/1.1\r\nHost: %s:%s\r\nConnection: close\r\n\r\n", params.hostname, params.service);


    g_cache = tls_cache_create();

    do {
        client_log("***** Connection to  %s:%s (reconnects=%d) starting\n", params.hostname, params.service, reconnects);

        err = client_connect(&params);

        client_log("***** Connection to  %s:%s (reconnects=%d) ended with err = %d\n\n", params.hostname, params.service, reconnects, err);

        if(resume_with_higher_version) {
            params.protocol_max = tls_protocol_version_TLS_1_2;
        }

        if(reconnects)
            sleep(reconnect_delay);

    } while(reconnects--);

    tls_cache_destroy(g_cache);
}
